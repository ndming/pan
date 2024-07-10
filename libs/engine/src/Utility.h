#pragma once

#include <optional>


struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    [[nodiscard]] bool isComplete() const { return graphics.has_value() && present.has_value(); }
};


struct BindOptions {
    bool enabledSamplerAnisotropy{ false };
};
