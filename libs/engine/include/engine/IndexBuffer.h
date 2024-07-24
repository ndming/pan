#pragma once

#include "engine/Buffer.h"

#include <cstdint>
#include <memory>


class Engine;

enum class IndexType {
    Uint16,
    Uint32,
};

class IndexBuffer final : public Buffer {
public:
    class Builder {
    public:
        Builder& indexCount(int count);
        Builder& indexType(IndexType type);

        [[nodiscard]] std::shared_ptr<IndexBuffer> build(const Engine& engine) const;

    private:
        static std::size_t getSize(IndexType type);
        static vk::IndexType getIndexType(IndexType type);

        int _indexCount{ 0 };
        IndexType _indexType{ IndexType::Uint16 };
    };

    void setData(const void* data, const Engine& engine) const;

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

private:
    uint32_t _indexCount;
    vk::IndexType _indexType;
    std::size_t _bufferSize;

public:
    /**
     * This constructor is meant for internal use only. To construct an IndexBuffer, use `IndexBuffer::Builder` class.
     */
    IndexBuffer(
        uint32_t indexCount,
        vk::IndexType indexType,
        std::size_t bufferSize,
        vk::BufferUsageFlags usage,
        const Engine& engine);
};
