#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>


class Engine;
struct EngineFeature;
class SwapChain;


class View final {
public:
    enum class PolygonMode {
        Fill,
        Line,
        Point,
    };

    enum class CullMode {
        Front,
        Back,
        FrontBack,
    };

    enum class FrontFace {
        Clockwise,
        CounterClockwise,
    };

    [[nodiscard]] static std::unique_ptr<View> create(const SwapChain& swapChain);

    void setViewport(float x, float y, float width, float height);
    [[nodiscard]] vk::Viewport getNativeViewport() const;

    void setScissor(int32_t offsetX, int32_t offsetY, uint32_t extentX, uint32_t extentY);
    [[nodiscard]] vk::Rect2D getNativeScissor() const;

    void setPolygonMode(PolygonMode mode);
    [[nodiscard]] vk::PolygonMode getNativePolygonMode() const;

    void setCullMode(CullMode mode);
    [[nodiscard]] vk::CullModeFlagBits getNativeCullMode() const;

    void setFrontFace(FrontFace direction);
    [[nodiscard]] vk::FrontFace getNativeFrontFace() const;

    void setLineWidth(float width);
    [[nodiscard]] float getLineWidth() const;

    void recordAllViewStates(const vk::CommandBuffer& buffer) const;

private:
    explicit View(const vk::Extent2D& swapImageExtent);

    vk::Viewport _viewport;
    vk::Rect2D _scissor;

    vk::PolygonMode _polygonMode{ vk::PolygonMode::eFill };
    vk::CullModeFlagBits _cullMode{ vk::CullModeFlagBits::eBack };
    vk::FrontFace _frontFace{ vk::FrontFace::eCounterClockwise };

    float _lineWidth{ 1.0f };
};
