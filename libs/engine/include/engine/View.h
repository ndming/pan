#pragma once

#include "engine/Camera.h"
#include "engine/Scene.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>


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

    void setCamera(const std::shared_ptr<Camera>& camera);
    [[nodiscard]] std::shared_ptr<Camera> getCamera() const;

    void setScene(const std::shared_ptr<Scene>& scene);
    [[nodiscard]] std::shared_ptr<Scene> getScene() const;

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

    void setPrimitiveRestart(bool enabled);
    [[nodiscard]] vk::Bool32 getNativePrimitiveRestartEnabled() const;

    void setLineWidth(float width);
    [[nodiscard]] float getLineWidth() const;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

private:
    explicit View(const vk::Extent2D& swapImageExtent);

    std::shared_ptr<Scene> _scene{ Scene::create() };
    std::shared_ptr<Camera> _camera{ Camera::create() };

    vk::Viewport _viewport;
    vk::Rect2D _scissor;

    vk::PolygonMode _polygonMode{ vk::PolygonMode::eFill };
    vk::CullModeFlagBits _cullMode{ vk::CullModeFlagBits::eBack };
    vk::FrontFace _frontFace{ vk::FrontFace::eCounterClockwise };

    vk::Bool32 _primitveRestartEnabled{ vk::False };
    float _lineWidth{ 1.0f };
};
