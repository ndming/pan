#pragma once

#include "engine/Buffer.h"
#include "engine/Context.h"
#include "engine/Composable.h"
#include "engine/Renderer.h"
#include "engine/ShaderInstance.h"
#include "engine/SwapChain.h"

#include <memory>


class ResourceAllocator;


struct EngineFeature {
    bool sampleShading{ false };
}; // Any update to this struct requires an asscoiate update to the getPhysicalDeviceFeatures method


class Engine final {
public:
    static std::unique_ptr<Engine> create(Surface* surface, const EngineFeature& feature = {});
    void destroy() noexcept;

    [[nodiscard]] SwapChain* createSwapChain() const;
    void destroySwapChain(SwapChain* swapChain) const noexcept;

    [[nodiscard]] Renderer* createRenderer() const;
    void destroyRenderer(Renderer* renderer) const noexcept;

    void destroyBuffer(const Buffer* buffer) const noexcept;

    void destroyShader(const Shader* shader) const noexcept;
    void destroyShaderInstance(const ShaderInstance* instance) const noexcept;

    void waitIdle() const;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    [[nodiscard]] const EngineFeature& getEngineFeature() const;

    [[nodiscard]] uint32_t getLimitPushConstantSize() const;
    [[nodiscard]] uint32_t getLimitMinUniformBufferOffsetAlignment() const;

    [[nodiscard]] vk::Instance getNativeInstance() const;
    [[nodiscard]] vk::Device getNativeDevice() const;
    [[nodiscard]] vk::Queue getNativeTransferQueue() const;
    [[nodiscard]] vk::CommandPool getNativeTransferCommandPool() const;

    [[nodiscard]] ResourceAllocator* getResourceAllocator() const;

private:
    Engine(GLFWwindow* window, const EngineFeature& feature);
    static vk::PhysicalDeviceFeatures2 getPhysicalDeviceFeatures(const EngineFeature& feature);
    static void cleanupPhysicalDeviceFeatures(const vk::PhysicalDeviceFeatures2& deviceFeatures);

    // The EngineFeature affects how graphics pipelines are created
    EngineFeature _feature;

    // The instance is the connection between our application and the Vulkan library
    vk::Instance _instance;

#ifndef NDEBUG
    // The validation layers will print debug messages to the standard output by default,
    // but we can also handle them ourselves by providing an explicit callback in our program
    vk::DebugUtilsMessengerEXT _debugMessenger;
#endif

    // The SwapChain will be automatically created when the Engine is created. This is because the SwapChain manages
    // the underlying surface which plays a crucial role in selecting the physical device. A call to createSwapChain
    // will instead populate the SwapChain's resources (eg. render targets). The SwapChain will own the physial device
    // and queue families since it uses them more frequently.
    SwapChain* _swapChain;

    vk::Device _device;

    vk::Queue _transferQueue;
    vk::CommandPool _transferCommandPool;

    // Our internal allocator, backed by the VMA library. Note that we cannot use a unique_ptr here because otherwise
    // the compiler would need to see the full definition of ResourceAllocator. This would require the library to
    // expose the VMA and other allocator infrastructure.
    ResourceAllocator* _allocator;
};
