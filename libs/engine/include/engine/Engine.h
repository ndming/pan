#pragma once

#include "engine/Buffer.h"
#include "engine/Context.h"
#include "engine/Composable.h"
#include "engine/Image.h"
#include "engine/Renderer.h"
#include "engine/Sampler.h"
#include "engine/ShaderInstance.h"
#include "engine/SwapChain.h"

#include <memory>


class ResourceAllocator;


struct EngineFeature {
    bool sampleShading{ false };
    bool samplerAnisotropy{ false };
}; // Any update to this struct requires an asscoiate update to the getPhysicalDeviceFeatures method


class Engine final {
public:
    static std::unique_ptr<Engine> create(Surface* surface, const EngineFeature& feature = {});
    void destroy() noexcept;

    /**
     * Creates a SwapChain with an optional request for MSAA level. The default (and preferred) MSAA level is 4x MSAA
     * which is particularly efficient in tiler architectures where the multi-sampled attachment is resolved in
     * tile memory and can therefore be transient.
     *
     * Note that depending on the capability of the underlying hardware, the SwapChain may end up with a lower MSAA
     * level than specified. A fallback warning will be printed out in such cases if logging is set up.
     *
     * @param level The MSAA level to request.
     * @return A shared-pointer to the created SwapChain object.
     */
    [[nodiscard]] std::shared_ptr<SwapChain> createSwapChain(SwapChain::MSAA level = SwapChain::MSAA::x4) const;

    /**
     * Destroys all internal resources associated with the specified SwapChain. After this call, the SwapChain object
     * pointed to by the shared-pointer is still in valid state but won't be able to use for any rendering operation.
     * Attempting to use a destroyed SwapChain results in undefined behaviors.
     *
     * This function must be called prior to Engine::destroy when the program exits to free all native resources.
     *
     * @param swapChain The SwapChain to destroy.
     */
    void destroySwapChain(const std::shared_ptr<SwapChain>& swapChain) const noexcept;

    /**
     * Creates a Renderer object which can be used to render views and overlays.
     *
     * @return A unique-pointer to the created Renderer object.
     */
    [[nodiscard]] std::unique_ptr<Renderer> createRenderer() const;

    /**
     * Destroys all internal resources associated with the specified Renderer. After this call, the Renderer object
     * pointed to by the unique-pointer is still in valid state but won't be able to render anything. Attempting to use
     * a destroyed Renderer results in undefined behaviors.
     *
     * This function must be called prior to Engine::destroy when the program exits to free all native resources.
     *
     * @param renderer The Renderer to destroy.
     */
    void destroyRenderer(const std::unique_ptr<Renderer>& renderer) const noexcept;

    void destroyBuffer(const Buffer* buffer) const noexcept;

    void destroyImage(const std::shared_ptr<Image>& image) const noexcept;

    void destroySampler(const std::unique_ptr<Sampler>& sampler) const noexcept;

    void destroyShader(const Shader* shader) const noexcept;
    void destroyShaderInstance(const ShaderInstance* instance) const noexcept;

    void waitIdle() const;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    [[nodiscard]] const EngineFeature& getEngineFeature() const;

    [[nodiscard]] uint32_t getLimitPushConstantSize() const;
    [[nodiscard]] float getLimitMaxSamplerAnisotropy() const;
    [[nodiscard]] uint32_t getLimitMinUniformBufferOffsetAlignment() const;
    [[nodiscard]] uint32_t getLimitMinStorageBufferOffsetAlignment() const;
    [[nodiscard]] uint32_t getLimitMaxUniformBufferRange() const;
    [[nodiscard]] uint32_t getLimitMaxStorageBufferRange() const;
    [[nodiscard]] uint32_t getLimitMaxPerStageDescriptorUniformBuffers() const;
    [[nodiscard]] uint32_t getLimitMaxPerStageDescriptorStorageBuffers() const;

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
    std::shared_ptr<SwapChain> _swapChain;

    vk::Device _device;

    vk::Queue _transferQueue;
    vk::CommandPool _transferCommandPool;

    // Our internal allocator, backed by the VMA library. Note that we cannot use a unique_ptr here because otherwise
    // the compiler would need to see the full definition of ResourceAllocator. This would require the library to
    // expose the VMA and other allocator infrastructure.
    ResourceAllocator* _allocator;
};
