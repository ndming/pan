#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>


class InstanceBuilder {
public:
    InstanceBuilder& applicationName(std::string name);
    InstanceBuilder& applicationVersion(int major, int minor, int patch);
    InstanceBuilder& apiVersion(int major, int minor, int patch);

    vk::Instance build(vk::InstanceCreateFlags flags = {}) const;

private:
    std::string _applicationName{};
    uint32_t _applicationVersion{};
    uint32_t _apiVersion{};

    static const std::vector<const char*> VALIDATION_LAYERS;

#ifndef NDEBUG
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* userData);
#endif
};