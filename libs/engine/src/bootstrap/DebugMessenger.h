#pragma once

#include <vulkan/vulkan.hpp>


class DebugMessenger {
public:
    [[nodiscard]] static vk::DebugUtilsMessengerEXT create(
        const vk::Instance& instance, PFN_vkDebugUtilsMessengerCallbackEXT callback);

    static void destroy(const vk::Instance& instance, const vk::DebugUtilsMessengerEXT& messenger) noexcept;

    DebugMessenger() = delete;

private:
    static vk::Result createDebugUtilsMessengerEXT(
        const vk::Instance& instance,
        const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        vk::DebugUtilsMessengerEXT* pDebugMessenger,
        const vk::AllocationCallbacks* pAllocator = nullptr);

    static void destroyDebugUtilsMessengerEXT(
        const vk::Instance& instance,
        vk::DebugUtilsMessengerEXT debugMessenger,
        const vk::AllocationCallbacks* pAllocator = nullptr);
};
