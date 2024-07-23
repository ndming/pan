#include "engine/VertexBuffer.h"

#include "Translator.h"

#include <plog/Log.h>
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <format>
#include <stdexcept>

#include <iostream>


VertexBuffer::Builder::Builder(const int vertexCount, const int bindingCount)
    : _vertexCount{ vertexCount }, _bindingCount{ bindingCount }  {
    if (vertexCount <= 0) {
        PLOGE << "Received non-positive vertex count: " << vertexCount;
        throw std::invalid_argument("Vertex count is non-positive");
    }
    if (bindingCount <= 0) {
        PLOGE << "Received non-positive binding count: " << bindingCount;
        throw std::invalid_argument("Binding count is non-positive");
    }
    _bindings.resize(bindingCount);
}

VertexBuffer::Builder& VertexBuffer::Builder::binding(const uint32_t binding, const uint32_t byteStride) {
    if (binding >= _bindingCount) {
        PLOGE << std::format("Binding index must be in the range 0 to {}, received: {}", _bindingCount - 1, binding);
        throw std::invalid_argument("Received invalid binding index");
    }
    // We're not supporting instanced rendering at this moment
    _bindings[binding] = { binding, byteStride, vk::VertexInputRate::eVertex };
    return *this;
}

VertexBuffer::Builder& VertexBuffer::Builder::attribute(
    const uint32_t binding,
    const uint32_t location,
    const AttributeFormat format,
    const uint32_t byteOffset
) {
    _attributes.emplace_back(location, binding, Translator::toFormat(format), byteOffset);
    return *this;
}

VertexBuffer* VertexBuffer::Builder::build(const Engine& engine) {
    // Calculate the offsets for each binding
    auto offsets = std::vector<vk::DeviceSize>(_bindingCount);
    offsets[0] = vk::DeviceSize{ 0 };
    for (int i = 0; i < _bindingCount - 1; ++i) {
        offsets[i + 1] = vk::DeviceSize{ offsets[i] + _vertexCount * _bindings[i].stride };
    }

    // Calculate the buffer total size and construct a VertexBuffer object
    const auto bufferSize = std::ranges::fold_left(
        _bindings, 0, [this](const auto accum, const Binding& it) { return accum + it.stride * _vertexCount; });
    const auto buffer = new VertexBuffer{ std::move(_bindings), std::move(_attributes), std::move(offsets), _vertexCount };

    // The buffer must also act as a transfer destination so that we can use staging buffer later to transfer data
    engine.createDeviceBuffer(
        bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        buffer->_buffer, &buffer->allocation);

    return buffer;
}

void VertexBuffer::setBindingData(const uint32_t binding, const void* const data, const Engine& engine) const {
    engine.transferBufferData(_vertexCount * _bindingDescriptions[binding].stride, data, _buffer, _offsets[binding]);
}

VertexBuffer::VertexBuffer(
    std::vector<Binding>&& bindings,
    std::vector<Attribute>&& attributes,
    std::vector<vk::DeviceSize>&& offsets,
    const int vertexCount) noexcept
: _bindingDescriptions{ std::move(bindings) }, _attributeDescriptions{ attributes },
  _offsets{ std::move(offsets) }, _vertexCount{ vertexCount } {
}
