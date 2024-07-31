#include "engine/VertexBuffer.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"

#include <plog/Log.h>

#include <format>
#include <ranges>
#include <stdexcept>


VertexBuffer::Builder::Builder(const int bindingCount) : _bindingCount{ bindingCount }  {
    if (bindingCount <= 0) {
        PLOGE << "Received non-positive binding count: " << bindingCount;
        throw std::invalid_argument("Binding count is non-positive");
    }
    _bindings.resize(bindingCount);
}

VertexBuffer::Builder & VertexBuffer::Builder::vertexCount(const int count) {
    if (count <= 0) {
        PLOGE << "Received non-positive vertex count: " << count;
        throw std::invalid_argument("Vertex count is non-positive");
    }
    _vertexCount = count;
    return *this;
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
    _attributes.emplace_back(location, binding, getFormat(format), byteOffset);
    return *this;
}

vk::Format VertexBuffer::Builder::getFormat(const AttributeFormat format) {
    using enum AttributeFormat;
    switch (format) {
        case Float:  return vk::Format::eR32Sfloat;
        case Vec2:   return vk::Format::eR32G32Sfloat;
        case Vec3:   return vk::Format::eR32G32B32Sfloat;
        case Vec4:   return vk::Format::eR32G32B32A32Sfloat;
        case Uint:   return vk::Format::eR32Uint;
        case Uvec2:  return vk::Format::eR32G32Uint;
        case Uvec3:  return vk::Format::eR32G32B32Uint;
        case Uvec4:  return vk::Format::eR32G32B32A32Uint;
        case Int:    return vk::Format::eR32Sint;
        case Ivec2:  return vk::Format::eR32G32Sint;
        case Ivec3:  return vk::Format::eR32G32B32Sint;
        case Ivec4:  return vk::Format::eR32G32B32A32Sint;
        case Double: return vk::Format::eR64Sfloat;
        default:     return vk::Format::eUndefined;
    }
}

VertexBuffer* VertexBuffer::Builder::build(const Engine& engine) {
    // Calculate the offsets for each binding
    auto offsets = std::vector<vk::DeviceSize>(_bindingCount);
    offsets[0] = vk::DeviceSize{ 0 };
    for (int i = 0; i < _bindingCount - 1; ++i) {
        offsets[i + 1] = vk::DeviceSize{ offsets[i] + _vertexCount * _bindings[i].stride };
    }

    // Calculate the buffer total size
    const auto bufferSize = std::ranges::fold_left(
        _bindings, 0, [this](const auto accum, const auto& it) { return accum + it.stride * _vertexCount; });
    // We must be able to transfer data down to this buffer from the CPU, hence the transfer dst flag
    constexpr auto usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;

    // Allocate a dedicated buffer in the GPU
    const auto allocator = engine.getResourceAllocator();
    auto allocation = VmaAllocation{};
    const auto buffer = allocator->allocateDedicatedBuffer(bufferSize, usage, &allocation);

    return new VertexBuffer{
        std::move(_bindings), std::move(_attributes), std::move(offsets), _vertexCount, buffer, allocation };
}

VertexBuffer::VertexBuffer(
    std::vector<vk::VertexInputBindingDescription>&& bindings,
    std::vector<vk::VertexInputAttributeDescription>&& attributes,
    std::vector<vk::DeviceSize>&& offsets,
    const int vertexCount,
    const vk::Buffer& buffer,
    void* const allocation
) : Buffer{ buffer, allocation },
    _bindingDescriptions{ std::move(bindings) },
    _attributeDescriptions{ attributes },
    _offsets{ std::move(offsets) },
    _vertexCount{ vertexCount } {
}

void VertexBuffer::setBindingData(const uint32_t binding, const void* const data, const Engine& engine) const {
    transferBufferData(_vertexCount * _bindingDescriptions[binding].stride, data, _offsets[binding], engine);
}
