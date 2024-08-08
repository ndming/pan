#include "engine/VertexBuffer.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"

#include <plog/Log.h>

#include <format>
#include <ranges>
#include <stdexcept>


VertexBuffer::Builder& VertexBuffer::Builder::vertexCount(const uint32_t count) {
    _vertexCount = count;
    return *this;
}

VertexBuffer::Builder& VertexBuffer::Builder::bindingCount(const uint32_t count) {
    _bindings.resize(count);
    return *this;
}

VertexBuffer::Builder& VertexBuffer::Builder::binding(const uint32_t binding, const uint32_t byteStride) {
    if (const auto bindingCount = _bindings.size(); binding >= bindingCount) {
        PLOGE << std::format("Binding index must be in the range 0 to {}, received: {}", bindingCount - 1, binding);
        throw std::invalid_argument("Received invalid binding index");
    }
    // We're not supporting instanced rendering at this moment
    auto bindingDescription = VkVertexInputBindingDescription2EXT{};
    bindingDescription.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
    bindingDescription.binding = binding;
    bindingDescription.stride = byteStride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescription.divisor = 1;

    _bindings[binding] = bindingDescription;
    return *this;
}

VertexBuffer::Builder& VertexBuffer::Builder::attribute(
    const uint32_t binding,
    const uint32_t location,
    const AttributeFormat format,
    const uint32_t byteOffset
) {
    auto attributeDescription = VkVertexInputAttributeDescription2EXT{};
    attributeDescription.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    attributeDescription.binding = binding;
    attributeDescription.location = location;
    attributeDescription.format = static_cast<VkFormat>(getFormat(format));
    attributeDescription.offset = byteOffset;

    _attributes.push_back(attributeDescription);
    return *this;
}

vk::Format VertexBuffer::Builder::getFormat(const AttributeFormat format) {
    using enum AttributeFormat;
    switch (format) {
        case Float:  return vk::Format::eR32Sfloat;
        case Float2: return vk::Format::eR32G32Sfloat;
        case Float3: return vk::Format::eR32G32B32Sfloat;
        case Float4: return vk::Format::eR32G32B32A32Sfloat;
        case Uint:   return vk::Format::eR32Uint;
        case Uint2:  return vk::Format::eR32G32Uint;
        case Uint3:  return vk::Format::eR32G32B32Uint;
        case Uint4:  return vk::Format::eR32G32B32A32Uint;
        case Int:    return vk::Format::eR32Sint;
        case Int2:   return vk::Format::eR32G32Sint;
        case Int3:   return vk::Format::eR32G32B32Sint;
        case Int4:   return vk::Format::eR32G32B32A32Sint;
        case Double: return vk::Format::eR64Sfloat;
        default:     return vk::Format::eUndefined;
    }
}

VertexBuffer* VertexBuffer::Builder::build(const Engine& engine) {
    // Calculate the offsets for each binding
    const auto bindingCount = _bindings.size();
    auto offsets = std::vector<vk::DeviceSize>(bindingCount);
    offsets[0] = vk::DeviceSize{ 0 };
    for (int i = 0; i < bindingCount - 1; ++i) {
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
    std::vector<VkVertexInputBindingDescription2EXT>&& bindings,
    std::vector<VkVertexInputAttributeDescription2EXT>&& attributes,
    std::vector<vk::DeviceSize>&& offsets,
    const uint32_t vertexCount,
    const vk::Buffer& buffer,
    void* const allocation
) : Buffer{ buffer, allocation },
    _bindingDescriptions{ std::move(bindings) },
    _attributeDescriptions{ std::move(attributes) },
    _offsets{ std::move(offsets) },
    _vertexCount{ vertexCount } {
}

void VertexBuffer::setData(const uint32_t binding, const void* const data, const Engine& engine) const {
    transferBufferData(_vertexCount * _bindingDescriptions[binding].stride, data, _offsets[binding], engine);
}

const std::vector<VkVertexInputBindingDescription2EXT>& VertexBuffer::getBindingDescriptions() const {
    return _bindingDescriptions;
}

const std::vector<VkVertexInputAttributeDescription2EXT>& VertexBuffer::getAttributeDescriptions() const {
    return _attributeDescriptions;
}

const std::vector<vk::DeviceSize>& VertexBuffer::getOffsets() const {
    return _offsets;
}
