#include "engine/Drawable.h"
#include "engine/ShaderInstance.h"
#include "engine/IndexBuffer.h"
#include "engine/VertexBuffer.h"

#include <glm/gtc/type_ptr.hpp>
#include <plog/Log.h>

#include <ranges>


Drawable::Builder::Builder(const uint32_t meshCount) : _primitives(meshCount), _shaderInstances(meshCount) {
}

Drawable::Builder& Drawable::Builder::geometry(
    const uint32_t meshIndex,
    const Topology topology,
    const VertexBuffer* const vertexBuffer,
    const IndexBuffer* const indexBuffer,
    const uint32_t indexCount,
    const uint32_t firstIndex,
    const int32_t vertexOffset
) {
    _primitives[meshIndex] = { getPrimitiveTopology(topology), vertexBuffer, indexBuffer, indexCount, firstIndex, vertexOffset };
    return *this;
}

Drawable::Builder& Drawable::Builder::material(const uint32_t meshIndex, const ShaderInstance* const instance) {
    _shaderInstances[meshIndex] = instance;
    return *this;
}

std::shared_ptr<Drawable> Drawable::Builder::build([[maybe_unused]] const Engine& engine) {
    if (std::ranges::any_of(_shaderInstances, [](const auto it) { return it == nullptr; })) {
        PLOGE << "Constructing a Drawable with missing shader instance(s)";
        throw std::runtime_error("Missing shader instance");
    }
    return std::make_shared<Drawable>(std::move(_primitives), std::move(_shaderInstances));
}

vk::PrimitiveTopology Drawable::Builder::getPrimitiveTopology(const Topology topology) {
    switch (topology) {
        case Topology::PointList:     return vk::PrimitiveTopology::ePointList;
        case Topology::LineList:      return vk::PrimitiveTopology::eLineList;
        case Topology::LineStrip:     return vk::PrimitiveTopology::eLineStrip;
        case Topology::TriangleList:  return vk::PrimitiveTopology::eTriangleList;
        case Topology::TriangleStrip: return vk::PrimitiveTopology::eTriangleStrip;
        case Topology::TriangleFan:   return vk::PrimitiveTopology::eTriangleFan;
        default: throw std::runtime_error("Unsupported topology");
    }
}

Drawable::Drawable(std::vector<Primitive>&& primitives, std::vector<const ShaderInstance*>&& shaderInstances)
noexcept : _primitives{ std::move(primitives) }, _shaderInstances{ shaderInstances } {
}

std::vector<vk::CommandBuffer> Drawable::recordDrawingCommands(
    const uint32_t frameIndex,
    const vk::CommandBuffer& commandBuffer,
    const std::function<void(const vk::CommandBuffer&)>& onPipelineBound,
    const glm::mat4& cameraMatrix,
    const glm::mat4& currentTransform
) const {
    // Propagate down the transform
    const auto mvp = ModelViewProjection{ cameraMatrix, currentTransform * _localTransform };

    // Draw all primitives specified for this drawable
    for (std::size_t i = 0; i < _primitives.size(); ++i) {
        // Bind the graphics pipeline
        const auto instance = _shaderInstances[i];
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, instance->getShader()->getNativePipeline());

        // Tell the renderer it's time to set the dynamic state
        onPipelineBound(commandBuffer);

        // Our turn to record drawing commands
        const auto& [topology, vertexBuffer, indexBuffer, indexCount, firstIndex, vertexOffset] = _primitives[i];

        // Specify the remaining pipeline dynamic states
        commandBuffer.setVertexInputEXT(vertexBuffer->getBindingDescriptions(), vertexBuffer->getAttributeDescriptions());
        commandBuffer.setPrimitiveTopology(topology);

        // We only have a single VkBuffer and bindings are controlled through offsets
        const auto vertexBuffers = std::vector(vertexBuffer->getBindingDescriptions().size(), vertexBuffer->getNativeBuffer());
        commandBuffer.bindVertexBuffers(0, vertexBuffers, vertexBuffer->getOffsets());

        // Bind the index buffer
        commandBuffer.bindIndexBuffer(indexBuffer->getNativeBuffer(), 0, indexBuffer->getNativeIndexType());

        // Set the transform component through push constant
        // layout(push_constant, std430) uniform ModelViewProjection {
        //     mat4 cameraMat;
        //     mat4 transform;
        // } mvp;
        commandBuffer.pushConstants(
            instance->getShader()->getNativePipelineLayout(), vk::ShaderStageFlagBits::eVertex,
            /* byte offset */ 0, /* byte size */ sizeof(ModelViewProjection), &mvp);

        // Bind descriptor sets
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, instance->getShader()->getNativePipelineLayout(),
            0, instance->getNativeDescriptorSetAt(frameIndex), {});

        // The official draw call
        commandBuffer.drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
    }

    // The return value is meant for ShadingGroup where secondary buffers would be created to record drawing commands
    // and they would be returned to the renderer. For a Drawable, we won't be using any secondary command buffers,
    // but it's possible that this Composable has a ShadingGroup child and we will have to accumulate its buffers.
    auto buffers = std::vector<vk::CommandBuffer>{};
    for (const auto& composable : _children) {
        auto childBuffers = composable->recordDrawingCommands(
            frameIndex, commandBuffer, onPipelineBound, cameraMatrix, mvp.transform);
        buffers.insert(buffers.end(), std::make_move_iterator(childBuffers.begin()), std::make_move_iterator(childBuffers.end()));
    }
    return buffers;
}

void Drawable::recordDrawingCommands(
    const uint32_t frameIndex,
    const vk::CommandBuffer& commandBuffer,
    const glm::mat4& cameraMatrix,
    const glm::mat4& currentTransform
) const {
    // Propagate down the transform
    const auto mvp = ModelViewProjection{ cameraMatrix, currentTransform * _localTransform };

    // For this overload, we won't bother binding the graphics pipeline or signaling the renderer to set the
    // dynamic states. This overload is meant for ShadingGroup where those operations have already happen.
    for (std::size_t i = 0; i < _primitives.size(); ++i) {
        const auto& [topology, vertexBuffer, indexBuffer, indexCount, firstIndex, vertexOffset] = _primitives[i];

        // Specify the remaining pipeline dynamic states
        commandBuffer.setVertexInputEXT(vertexBuffer->getBindingDescriptions(), vertexBuffer->getAttributeDescriptions());
        commandBuffer.setPrimitiveTopology(topology);

        // Bind the vertex and index buffer
        const auto vertexBuffers = std::vector(vertexBuffer->getBindingDescriptions().size(), vertexBuffer->getNativeBuffer());
        commandBuffer.bindVertexBuffers(0, vertexBuffers, vertexBuffer->getOffsets());
        commandBuffer.bindIndexBuffer(indexBuffer->getNativeBuffer(), 0, indexBuffer->getNativeIndexType());

        // Transform component
        const auto instance = _shaderInstances[i];
        commandBuffer.pushConstants(
            instance->getShader()->getNativePipelineLayout(), vk::ShaderStageFlagBits::eVertex,
            /* byte offset */ 0, /* byte size */ sizeof(ModelViewProjection), &mvp);

        // Descriptor sets
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, instance->getShader()->getNativePipelineLayout(),
            0, instance->getNativeDescriptorSetAt(frameIndex), {});

        // Issue the draw call
        commandBuffer.drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
    }
}
