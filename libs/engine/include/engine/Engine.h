#pragma once

#include "engine/Context.h"
#include "engine/SwapChain.h"
#include "engine/VertexBuffer.h"

#include <memory>


class ResourceAllocator;

class Engine {
public:
    enum class Feature {
        SamplerAnisotropy,
        SampleRateShading,
    };

    static std::unique_ptr<Engine> create(const std::unique_ptr<Context>& context, const std::vector<Feature>& features = {});
    void destroy() noexcept;

    [[nodiscard]] SwapChain* createSwapChain() const;
    void destroySwapChain(SwapChain* swapChain) const noexcept;

    // [[nodiscard]] Renderer* createRenderer(SwapChain* swapChain, Renderer::Pipeline pipeline) const;
    // void destroyRenderer(const Renderer* renderer) const noexcept;

    void destroyVertexBuffer(VertexBuffer* buffer) const noexcept;

    void waitIdle() const;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    Engine(const std::unique_ptr<Context>& context, const std::vector<Feature>& features);

    [[nodiscard]] vk::CommandBuffer beginSingleTimeTransferCommands() const;
    void endSingleTimeTransferCommands(const vk::CommandBuffer &commandBuffer) const;

    void createDeviceBuffer(std::size_t bufferSize, vk::BufferUsageFlags usage, vk::Buffer& buffer, void** allocation) const;
    void transferBufferData(std::size_t bufferSize, const void* data, const vk::Buffer& buffer, vk::DeviceSize offset) const;

    // The instance is the connection between our application and the Vulkan library
    vk::Instance _instance{};

#ifndef NDEBUG
    // The validation layers will print debug messages to the standard output by default,
    // but we can also handle them ourselves by providing an explicit callback in our program
    vk::DebugUtilsMessengerEXT _debugMessenger{};
#endif

    // The SwapChain will be automatically created when the Engine is created. This is because the SwapChain manages the
    // the underlying surface which plays a crucial role in selecting the physical device. A call to createSwapChain
    // will instead populate the SwapChain's resources (eg. render targets). The SwapChain will own the physial device
    // and queue families since it uses them more frequently.
    std::unique_ptr<SwapChain> _swapChain{};

    vk::Device _device{};

    vk::Queue _transferQueue{};
    vk::CommandPool _transferCommandPool{};

    // Our internal allocator, backed by the VMA library. Note that we cannot use a unique_ptr here because otherwise
    // the compiler would need to see the full definition of ResourceAllocator.
    ResourceAllocator* _allocator{ nullptr };

    // The buffers need access to the createDeviceBuffer method
    friend class VertexBuffer;
};
