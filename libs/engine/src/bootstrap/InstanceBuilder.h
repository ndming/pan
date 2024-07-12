#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan.hpp>


class InstanceBuilder {
public:
    InstanceBuilder& applicationName(std::string name);
    InstanceBuilder& applicationVersion(int major, int minor, int patch);
    InstanceBuilder& apiVersion(int major, int minor, int patch);
    InstanceBuilder& layers(const char* const* layers, std::size_t count);

    [[nodiscard]] vk::Instance build() const;

private:
    std::string _applicationName{};
    uint32_t _applicationVersion{};
    uint32_t _apiVersion{};
    std::vector<const char*> _layers{};
};