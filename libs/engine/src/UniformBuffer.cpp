#include "engine/UniformBuffer.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"


UniformBuffer::Builder & UniformBuffer::Builder::dataByteSize(const std::size_t size) {
    _dataSize = size;
    return *this;
}

UniformBuffer* UniformBuffer::Builder::build(const Engine& engine) const {
    // We should have multiple buffers, because multiple frames may be in flight at the same time, and we don’t want to
    // update the buffer in preparation of the next frame while a previous one is still reading from it! Thus, we need
    // to have as many uniform buffers as we have frames in flight, and write to a uniform buffer that is not currently
    // being read by the GPU. To promote cache locality, we will only create and allocate a single native VkBuffer that
    // is big enough to hold as many buffers of _dataSize as required, and we will use offset values later to index to
    // the buffer we want to use when drawing. The only catch to this approach is that uniform buffer offsets must
    // respect the alignment limit specified in minUniformBufferOffsetAlignment. This means the actual bufferSize for
    // each in-flight frame buffer must be a multiple of this value.
    const auto minAlignment = engine.getLimitMinUniformBufferOffsetAlignment();
    std::size_t multiple = 0;
    while (multiple * minAlignment < _dataSize) ++multiple;
    const auto bufferSize = multiple * minAlignment;
    // Unlike VertexBuffer and IndexBuffer, it is more common to make a UniformBuffer persistently mapped.
    // Therefore, we don't need the transfer dst flag.
    constexpr auto usage = vk::BufferUsageFlagBits::eUniformBuffer;

    // Allocate a persistently mapped buffer
    const auto allocator = engine.getResourceAllocator();
    auto allocation = VmaAllocation{};          // allocation is a pointer
    auto allocationInfo = VmaAllocationInfo{};  // allocation info is a struct
    const auto buffer = allocator->allocatePersistentBuffer(
        bufferSize * Renderer::getMaxFramesInFlight(),  // as many buffers of bufferSize as for each in-flight frame
        usage, &allocation, &allocationInfo);

    return new UniformBuffer{ bufferSize, _dataSize, buffer, allocation, static_cast<std::byte*>(allocationInfo.pMappedData) };
}

UniformBuffer::UniformBuffer(
    const std::size_t bufferSize, const std::size_t dataSize,
    const vk::Buffer& buffer, void* const allocation, std::byte* const pMappedData
) : Buffer{ buffer, allocation, pMappedData }, _bufferSize{ bufferSize }, _dataSize{ dataSize } {
}

void UniformBuffer::setData(const uint32_t frameIndex, const void* const data) const {
    // Memory in Vulkan doesn't need to be unmapped before using it on GPU, but unless a memory types has
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT flag set, we need to manually invalidate cache before reading of mapped
    // pointer and flush cache after writing to mapped pointer. Map/unmap operations don't do that automatically.
    // Windows drivers from all 3 PC GPU vendors (AMD, Intel, NVIDIA) currently provide HOST_COHERENT flag on all
    // memory types that are HOST_VISIBLE, so on PC we may not need to bother.
    memcpy(_pMappedData + frameIndex * _bufferSize, data, _dataSize);
}

void UniformBuffer::setData(const void* const data) const {
    for (uint32_t index = 0; index < Renderer::getMaxFramesInFlight(); ++index) {
        setData(index, data);
    }
}

std::size_t UniformBuffer::getBufferSize() const {
    return _bufferSize;
}

std::size_t UniformBuffer::getDataSize() const {
    return _dataSize;
}
