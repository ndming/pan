#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>
#include <unordered_set>


class Composable : public std::enable_shared_from_this<Composable> {
public:
    void attach(const std::shared_ptr<Composable>& child);
    void detach(const std::shared_ptr<Composable>& child) noexcept;

    void attachAll(std::initializer_list<std::shared_ptr<Composable>> children);
    void detachAll() noexcept;

    [[nodiscard]] bool attached() const noexcept;
    [[nodiscard]] bool hasChild(const std::shared_ptr<Composable>& child) const noexcept;

    [[nodiscard]] virtual std::vector<vk::CommandBuffer> recordDrawingCommands(
        uint32_t frameIndex,
        const vk::CommandBuffer& commandBuffer,
        const glm::mat4& cameraMatrix,
        const glm::mat4& currentTransform,
        const std::function<void(const vk::CommandBuffer&)>& onPipelineBound) const = 0;

    virtual void recordDrawingCommands(
        uint32_t frameIndex,
        const vk::CommandBuffer& commandBuffer,
        const glm::mat4& cameraMatrix,
        const glm::mat4& currentTransform) const = 0;

    virtual ~Composable();

    Composable(const Composable&) = delete;
    Composable& operator=(const Composable&) = delete;

protected:
    Composable() = default;

    std::weak_ptr<Composable> _parent{};
    std::unordered_set<std::shared_ptr<Composable>> _children{};
};
