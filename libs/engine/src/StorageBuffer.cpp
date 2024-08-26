#include "engine/StorageBuffer.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"

#include <plog/Log.h>


StorageBuffer::Builder & StorageBuffer::Builder::byteSize(const std::size_t size) {
    _bufferSize = size;
    return *this;
}

StorageBuffer* StorageBuffer::Builder::build(const Engine& engine) const {
    if (_bufferSize > engine.getLimitMaxStorageBufferRange()) {
        PLOGE << "Buffer byte size is bigger than the maximum limit of: " << engine.getLimitMaxStorageBufferRange();
        throw std::runtime_error("Buffer byte size is too big");
    }

    constexpr auto usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

    // Allocate a dedicated buffer in the GPU
    const auto allocator = engine.getResourceAllocator();
    auto allocation = VmaAllocation{};
    const auto buffer = allocator->allocateDedicatedBuffer(_bufferSize, usage, &allocation);

    return new StorageBuffer{ _bufferSize, buffer, allocation };
}

StorageBuffer::StorageBuffer(
    const std::size_t bufferSize,
    const vk::Buffer& buffer,
    void* allocation
) : Buffer{ buffer, allocation },
    _bufferSize{ bufferSize } {
}

void StorageBuffer::setData(const void* data, const Engine& engine) const {
    transferBufferData(_bufferSize, data, 0, engine);
}

uint32_t StorageBuffer::getBufferSize() const {
    return _bufferSize;
}
