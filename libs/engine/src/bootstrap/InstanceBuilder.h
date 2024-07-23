#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>


class InstanceBuilder {
public:
    InstanceBuilder& applicationName(std::string name);
    InstanceBuilder& applicationVersion(int major, int minor, int patch);
    InstanceBuilder& apiVersion(int major, int minor, int patch);
    InstanceBuilder& layers(const char* const* layers, std::size_t count);
    InstanceBuilder& callback(PFN_vkDebugUtilsMessengerCallbackEXT callback);

    [[nodiscard]] vk::Instance build() const;

private:
    std::string _applicationName{};
    uint32_t _applicationVersion{};
    uint32_t _apiVersion{};
    std::vector<const char*> _layers{};
    PFN_vkDebugUtilsMessengerCallbackEXT _callback{};
};