#pragma once

#include <vulkan/vulkan.hpp>


class Renderer {
public:
    enum class Pipeline {
        Particle,
    };

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    Renderer() = default;

    vk::RenderPass _renderPass{};

    friend class Engine;
};
