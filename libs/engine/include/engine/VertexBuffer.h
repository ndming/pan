#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <vector>


class Engine;


enum class AttributeFormat {
    /**
     * A single-precision (32-bit) float
     */
    Float,

    /**
     * A 2-component vector of 32-bit floats
     */
    Vec2,

    /**
     * A 3-component vector of 32-bit floats
     */
    Vec3,

    /**
     * A 4-component vector of 32-bit floats
     */
    Vec4,

    /**
     * A 32-bit unsigned integer
     */
    Uint,

    /**
     * A 2-component vector of 32-bit unsigned integers
     */
    Uvec2,

    /**
     * A 3-component vector of 32-bit unsigned integers
     */
    Uvec3,

    /**
     * A 4-component vector of 32-bit unsigned integers
     */
    Uvec4,

    /**
     * A 32-bit signed integer
     */
    Int,

    /**
     * A 2-component vector of 32-bit signed integers
     */
    Ivec2,

    /**
     * A 3-component vector of 32-bit signed integers
     */
    Ivec3,

    /**
     * A 4-component vector of 32-bit signed integers
     */
    Ivec4,

    /**
     * A double-precision (64-bit) float
     */
    Double,
};


class VertexBuffer {
public:
    class Builder {
    public:
        explicit Builder(int vertexCount, int bindingCount);

        /**
         * Add a binding description to this vertex buffer.
         *
         * @param binding The binding to configure in this buffer and must range from 0 to bindingCount - 1.
         * @param byteStride The byte stride between consecutive elements within this binding, corresponding to how many
         * bytes to skip to get from one vertex to the next. For interleaved vertex data, this would be the size of the
         * vertex structure. For non-interleaved data, it would be the size of the particular attribute data if the
         * binding only contains one attribute.
         * @return this Builder object for chaining calls.
         */
        Builder& binding(uint32_t binding, uint32_t byteStride);

        /**
         * Add an attribute description to this vertex buffer set.
         * 
         * @param binding The binding from which vertex data of this attribute orginates.
         * @param location the location of this attribute in the vertex shader code.
         * @param format The type and number of components of this attribute.
         * @param byteOffset The byte offset of this attribute within each vertex, corresponding to the number of bytes
         * from the start of the vertex to the location of this attribute.
         * @return this Builder object for chaining calls.
         */
        Builder& attribute(uint32_t binding, uint32_t location, AttributeFormat format, uint32_t byteOffset = 0);

        [[nodiscard]] VertexBuffer* build(const Engine& engine);

    private:
        std::vector<vk::VertexInputAttributeDescription> _attributes{};
        int _vertexCount{ 0 };

        std::vector<vk::VertexInputBindingDescription> _bindings{};
        int _bindingCount{ 0 };
    };

    void setBindingData(uint32_t binding, const void* data, const Engine& engine) const;

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

private:
    using Binding = vk::VertexInputBindingDescription;
    using Attribute = vk::VertexInputAttributeDescription;
    VertexBuffer(
        std::vector<Binding>&& bindings,
        std::vector<Attribute>&& attributes,
        std::vector<vk::DeviceSize>&& offsets,
        std::size_t bufferSize) noexcept;

    std::vector<Binding> _bindingDescriptions;
    std::vector<Attribute> _attributeDescriptions;
    std::vector<vk::DeviceSize> _offsets;

    vk::Buffer _buffer{};
    std::size_t _bufferSize;
    void* allocation{ nullptr };

    // The Engine needs access to the vk::Buffer and allocation when destroying the buffer
    friend class Engine;
};
