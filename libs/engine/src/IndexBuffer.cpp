#include "engine/IndexBuffer.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"


IndexBuffer::Builder & IndexBuffer::Builder::indexCount(const int count) {
    _indexCount = count;
    return *this;
}

IndexBuffer::Builder & IndexBuffer::Builder::indexType(const IndexType type) {
    _indexType = type;
    return *this;
}

IndexBuffer* IndexBuffer::Builder::build(const Engine& engine) const {
    // Calculate the buffer size
    const auto bufferSize = getSize(_indexType) * _indexCount;
    // We must be able to transfer data down to this buffer from the CPU, hence the transfer dst flag
    constexpr auto usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;

    // Allocate a dedicated buffer in the GPU
    const auto allocator = engine.getResourceAllocator();
    auto allocation = VmaAllocation{};
    const auto buffer = allocator->allocateDedicatedBuffer(bufferSize, usage, &allocation);

    return new IndexBuffer{
        static_cast<uint32_t>(_indexCount), getIndexType(_indexType), bufferSize, buffer, allocation };
}

std::size_t IndexBuffer::Builder::getSize(const IndexType type) {
    switch (type) {
        case IndexType::Uint16: return sizeof(uint16_t);
        case IndexType::Uint32: return sizeof(uint32_t);
        default: throw std::runtime_error("Unsupported index type");
    }
}

vk::IndexType IndexBuffer::Builder::getIndexType(const IndexType type) {
    switch (type) {
        case IndexType::Uint16: return vk::IndexType::eUint16;
        case IndexType::Uint32: return vk::IndexType::eUint32;
        default: return vk::IndexType{};
    }
}

IndexBuffer::IndexBuffer(
    const uint32_t indexCount,
    const vk::IndexType indexType,
    const std::size_t bufferSize,
    const vk::Buffer& buffer,
    void* const allocation
) : Buffer{ buffer, allocation },
    _indexCount{ indexCount },
    _indexType{ indexType },
    _bufferSize{ bufferSize } {
}

void IndexBuffer::setData(const void* const data, const Engine& engine) const {
    transferBufferData(_bufferSize, data, 0, engine);
}

uint32_t IndexBuffer::getIndexCount() const {
    return _indexCount;
}

vk::IndexType IndexBuffer::getNativeIndexType() const {
    return _indexType;
}
