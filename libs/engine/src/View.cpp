#include "engine/View.h"
#include "engine/Engine.h"
#include "engine/SwapChain.h"

#include <plog/Log.h>

#include <ranges>


std::unique_ptr<View> View::create(const SwapChain& swapChain) {
    return std::unique_ptr<View>(new View{ swapChain.getNativeSwapImageExtent() });
}

void View::setScene(const std::shared_ptr<Scene>& scene) {
    _scene = scene;
}

std::shared_ptr<Scene> View::getScene() const {
    return _scene;
}

View::View(const vk::Extent2D& swapImageExtent) :
    _viewport{ 0.0f, 0.0f, static_cast<float>(swapImageExtent.width), static_cast<float>(swapImageExtent.height) },
    _scissor{ vk::Offset2D{ 0, 0 }, swapImageExtent } {
}

void View::setViewport(const float x, const float y, const float width, const float height) {
    _viewport = vk::Viewport{ x, y, width, height };
}

vk::Viewport View::getNativeViewport() const {
    return _viewport;
}

void View::setScissor(const int32_t offsetX, const int32_t offsetY, const uint32_t extentX, const uint32_t extentY) {
    _scissor = vk::Rect2D{ vk::Offset2D{ offsetX, offsetY }, vk::Extent2D{ extentX, extentY } };
}

vk::Rect2D View::getNativeScissor() const {
    return _scissor;
}

void View::setPolygonMode(const PolygonMode mode) {
    switch (mode) {
        case PolygonMode::Fill:  _polygonMode = vk::PolygonMode::eFill; break;
        case PolygonMode::Line:  _polygonMode = vk::PolygonMode::eLine; break;
        case PolygonMode::Point: _polygonMode = vk::PolygonMode::ePoint; break;
    }
}

vk::PolygonMode View::getNativePolygonMode() const {
    return _polygonMode;
}

void View::setCullMode(const CullMode mode) {
    switch (mode) {
        case CullMode::Front:     _cullMode = vk::CullModeFlagBits::eFront;        break;
        case CullMode::Back:      _cullMode = vk::CullModeFlagBits::eBack;         break;
        case CullMode::FrontBack: _cullMode = vk::CullModeFlagBits::eFrontAndBack; break;
    }
}

vk::CullModeFlagBits View::getNativeCullMode() const {
    return _cullMode;
}

void View::setFrontFace(const FrontFace direction) {
    switch (direction) {
        case FrontFace::Clockwise:        _frontFace = vk::FrontFace::eClockwise; break;
        case FrontFace::CounterClockwise: _frontFace = vk::FrontFace::eCounterClockwise; break;
    }
}

vk::FrontFace View::getNativeFrontFace() const {
    return _frontFace;
}

void View::setPrimitiveRestart(const bool enabled) {
    _primitveRestartEnabled = enabled;
}

vk::Bool32 View::getNativePrimitiveRestartEnabled() const {
    return _primitveRestartEnabled;
}

void View::setLineWidth(const float width) {
    _lineWidth = std::max(1.0f, width);
}

float View::getLineWidth() const {
    return _lineWidth;
}
