#pragma once

#include <vulkan/vulkan.hpp>


class Engine;

enum class IndexType {
    Uint16,
    Uint32,
};

class IndexBuffer {
public:
    class Builder {
    public:
        Builder& indexCount(int count);
        Builder& indexType(IndexType type);

        [[nodiscard]] IndexBuffer* build(const Engine& engine) const;

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
    IndexBuffer(uint32_t indexCount, vk::IndexType indexType, std::size_t bufferSize);

    uint32_t _indexCount;
    vk::IndexType _indexType;
    std::size_t _bufferSize;

    vk::Buffer _buffer{};
    void* allocation{ nullptr };

    // The Engine needs access to the vk::Buffer and allocation when destroying the buffer
    friend class Engine;
};
