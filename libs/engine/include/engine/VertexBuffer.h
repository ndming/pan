#pragma once

#include "engine/Buffer.h"

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


/**
 * Represents a dedicated GPU buffer object for vertex data. A VertexBuffer can have a single binding containing
 * interleaved vertex data, or multiple bindings, each contains a single or certain number of vertex attributes.
 * Both binding and attribute information shall be specified through the VertexBuffer::Builder class. Regardless of
 * which binding scheme is used, there will only be a single native buffer object created internally to promote
 * cache locality. In the case of multiple bindings, the class use offset values internally to manage them.
 */
class VertexBuffer final : public Buffer {
public:
    class Builder {
    public:
        explicit Builder(int bindingCount);

        /**
         * Specify the number of vertices for this vertex buffer.
         *
         * @param count The number of vertices, will be set for all bindings
         * @return this Builder object for chaining calls.
         */
        Builder& vertexCount(int count);

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
        static vk::Format getFormat(AttributeFormat format);

        std::vector<vk::VertexInputAttributeDescription2EXT> _attributes{};
        int _vertexCount{ 0 };

        std::vector<vk::VertexInputBindingDescription2EXT> _bindings{};
        int _bindingCount{ 0 };
    };

    /**
     * Transfer vertex data to this buffer at the binding. The operation is synchronous and the transfer is guaranteed
     * to complete when the function returns.
     *
     * @param binding The binding to set buffer data.
     * @param data The data should respect the binding size specified when constructing this vertex buffer.
     * @param engine The Engine where the transfer and allocation will take place.
     */
    void setBindingData(uint32_t binding, const void* data, const Engine& engine) const;

    [[nodiscard]] const std::vector<vk::VertexInputBindingDescription2EXT>& getBindingDescriptions() const;
    [[nodiscard]] const std::vector<vk::VertexInputAttributeDescription2EXT>& getAttributeDescriptions() const;
    [[nodiscard]] const std::vector<vk::DeviceSize>& getOffsets() const;

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

private:
    VertexBuffer(
       std::vector<vk::VertexInputBindingDescription2EXT>&& bindings,
       std::vector<vk::VertexInputAttributeDescription2EXT>&& attributes,
       std::vector<vk::DeviceSize>&& offsets,
       int vertexCount,
       const vk::Buffer& buffer,
       void* allocation);

    std::vector<vk::VertexInputBindingDescription2EXT> _bindingDescriptions;
    std::vector<vk::VertexInputAttributeDescription2EXT> _attributeDescriptions;
    std::vector<vk::DeviceSize> _offsets;

    int _vertexCount;
};
