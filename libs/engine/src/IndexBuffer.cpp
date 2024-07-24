#include "engine/IndexBuffer.h"


IndexBuffer::Builder & IndexBuffer::Builder::indexCount(const int count) {
    _indexCount = count;
    return *this;
}

IndexBuffer::Builder & IndexBuffer::Builder::indexType(const IndexType type) {
    _indexType = type;
    return *this;
}

std::shared_ptr<IndexBuffer> IndexBuffer::Builder::build(const Engine& engine) const {
    // Construct an IndexBuffer object
    const auto bufferSize = getSize(_indexType) * _indexCount;
    const auto buffer = std::make_shared<IndexBuffer>(
        static_cast<uint32_t>(_indexCount),
        getIndexType(_indexType),
        bufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        engine);

    return buffer;
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
    const vk::BufferUsageFlags usage,
    const Engine& engine
) : Buffer{ bufferSize, usage, engine },
    _indexCount{ indexCount },
    _indexType{ indexType },
    _bufferSize{ bufferSize } {
}

void IndexBuffer::setData(const void* const data, const Engine& engine) const {
    transferBufferData(_bufferSize, data, 0, engine);
}
