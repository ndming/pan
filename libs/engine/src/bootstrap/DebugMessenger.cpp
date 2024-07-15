#include <plog/Log.h>

#include "DebugMessenger.h"


vk::DebugUtilsMessengerEXT DebugMessenger::create(
    const vk::Instance& instance,
    PFN_vkDebugUtilsMessengerCallbackEXT callback
) {
    using MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;
    const auto debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{ {},
        MessageSeverity::eVerbose | MessageSeverity::eWarning | MessageSeverity::eError,
        MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        callback, nullptr
    };

    auto debugMessenger = vk::DebugUtilsMessengerEXT{};
    if (createDebugUtilsMessengerEXT(instance, &debugCreateInfo, &debugMessenger) == vk::Result::eErrorExtensionNotPresent) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
    return debugMessenger;
}

vk::Result DebugMessenger::createDebugUtilsMessengerEXT(
    const vk::Instance& instance, const vk::DebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    vk::DebugUtilsMessengerEXT* pDebugMessenger, const vk::AllocationCallbacks *pAllocator
) {
    if (const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")); func != nullptr) {
        return static_cast<vk::Result>(
            func(
                instance, reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(pCreateInfo),
                reinterpret_cast<const VkAllocationCallbacks*>(pAllocator),
                reinterpret_cast<VkDebugUtilsMessengerEXT*>(pDebugMessenger)
            )
        );
        }
    return vk::Result::eErrorExtensionNotPresent;
}

void DebugMessenger::destroy(const vk::Instance &instance, const vk::DebugUtilsMessengerEXT &messenger) noexcept {
    destroyDebugUtilsMessengerEXT(instance, messenger);
}

void DebugMessenger::destroyDebugUtilsMessengerEXT(
    const vk::Instance& instance,
    const vk::DebugUtilsMessengerEXT debugMessenger,
    const vk::AllocationCallbacks *pAllocator
) {
    if (const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")); func != nullptr) {
        func(instance, debugMessenger, reinterpret_cast<const VkAllocationCallbacks*>(pAllocator));
        }
}
