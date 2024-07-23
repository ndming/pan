#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <optional>


class QueueFamilyFinder {
public:
    [[nodiscard]] QueueFamilyFinder& requestPresentFamily(const vk::SurfaceKHR& surface);
    [[nodiscard]] QueueFamilyFinder& requestComputeFamily();

    bool find(const vk::PhysicalDevice& candidate);
    [[nodiscard]] bool completed(bool relaxAsyncComputeRequest = false) const;

    [[nodiscard]] uint32_t getGraphicsFamily() const;
    [[nodiscard]] uint32_t getPresentFamily() const;
    [[nodiscard]] uint32_t getComputeFamily() const;

    void reset();

private:
    vk::SurfaceKHR _surface{};
    bool _findPresentFamily{ false };

    bool _findComputeFamily{ false };

    std::optional<uint32_t> _graphicsFamily{};
    std::optional<uint32_t> _presentFamily{};
    std::optional<uint32_t> _computeFamily{};
};
