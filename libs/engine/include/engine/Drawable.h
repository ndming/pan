#pragma once

#include "engine/Transformable.h"

#include <vector>


class Engine;
class ShaderInstance;
class IndexBuffer;
class VertexBuffer;


class Drawable final : public Transformable {
    struct Primitive {
        vk::PrimitiveTopology topology;
        const VertexBuffer* vertexBuffer;
        const IndexBuffer* indexBuffer;
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
    };

public:
    enum class Topology {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
    };

    class Builder {
    public:
        explicit Builder(uint32_t meshCount);

        Builder& geometry(
            uint32_t meshIndex, Topology topology, const VertexBuffer* vertexBuffer, const IndexBuffer* indexBuffer,
            uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0);

        Builder& material(uint32_t meshIndex, const ShaderInstance* instance);

        [[nodiscard]] std::shared_ptr<Drawable> build(const Engine& engine);

    private:
        [[nodiscard]] static vk::PrimitiveTopology getPrimitiveTopology(Topology topology);

        std::vector<Primitive> _primitives{};
        std::vector<const ShaderInstance*> _shaderInstances{};
    };

    [[nodiscard]] std::vector<vk::CommandBuffer> recordDrawingCommands(
        uint32_t frameIndex,
        const vk::CommandBuffer& commandBuffer,
        const glm::mat4& cameraMatrix,
        const glm::mat4& currentTransform,
        const std::function<void(const vk::CommandBuffer&)>& onPipelineBound) const override;

    void recordDrawingCommands(
        uint32_t frameIndex,
        const vk::CommandBuffer &commandBuffer,
        const glm::mat4& cameraMatrix,
        const glm::mat4 &currentTransform) const override;

    Drawable(
        std::vector<Primitive>&& primitives,
        std::vector<const ShaderInstance*>&& shaderInstances,
        PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInput);

private:
    std::vector<Primitive> _primitives{};
    std::vector<const ShaderInstance*> _shaderInstances{};

    PFN_vkCmdSetVertexInputEXT _vkCmdSetVertexInput;
};
