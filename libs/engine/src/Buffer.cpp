#include "engine/Buffer.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"


vk::Buffer Buffer::getNativeBuffer() const {
    return _buffer;
}

void* Buffer::getAllocation() const {
    return _allocation;
}

Buffer::Buffer(const std::size_t bufferSize, const vk::BufferUsageFlags usage, const Engine &engine) {
    const auto allocator = engine.getResourceAllocator();
    auto allocation = VmaAllocation{};

    _buffer = allocator->allocateDeviceBuffer(bufferSize, static_cast<VkBufferUsageFlags>(usage), &allocation);
    _allocation = allocation;
}

void Buffer::transferBufferData(
    const std::size_t bufferSize,
    const void* const data,
    const vk::DeviceSize offset,
    const Engine& engine
) const {
    const auto allocator = engine.getResourceAllocator();

    // Create a staging buffer to handle the transfer
    auto stagingAllocation = VmaAllocation{};
    const auto stagingBuffer = allocator->allocateStagingBuffer(bufferSize, &stagingAllocation);

    allocator->mapData(bufferSize, data, stagingAllocation);
    const auto commandBuffer = beginSingleTimeTransferCommands(engine);
    commandBuffer.copyBuffer(stagingBuffer, _buffer, vk::BufferCopy{ 0, offset, bufferSize });
    endSingleTimeTransferCommands(commandBuffer, engine);

    allocator->destroyBuffer(stagingBuffer, stagingAllocation);
}

vk::CommandBuffer Buffer::beginSingleTimeTransferCommands(const Engine& engine) {
    const auto allocInfo = vk::CommandBufferAllocateInfo{ engine.getNativeTransferCommandPool(), vk::CommandBufferLevel::ePrimary, 1 };
    const auto commandBuffer = engine.getNativeDevice().allocateCommandBuffers(allocInfo)[0];

    // Use the command buffer once and wait with returning from the function until the copy operation has finished
    constexpr auto beginInfo = vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Buffer::endSingleTimeTransferCommands(const vk::CommandBuffer &commandBuffer, const Engine& engine) {
    const auto transferQueue = engine.getNativeTransferQueue();

    commandBuffer.end();
    transferQueue.submit(vk::SubmitInfo{ {}, {}, {}, 1, &commandBuffer });

    // Unlike the draw commands, there are no events we need to wait on this time. We just want to execute the transfer
    // on the buffers immediately. There are again two possible ways to wait on this transfer to complete. We could use
    // a fence and wait with vkWaitForFences, or simply wait for the transfer queue to become idle with vkQueueWaitIdle.
    // A fence would allow scheduling multiple transfers simultaneously and wait for all of them to complete, instead
    // of executing one at a time. That may give the driver more opportunities to optimize.
    transferQueue.waitIdle();

    engine.getNativeDevice().freeCommandBuffers(engine.getNativeTransferCommandPool(), 1, &commandBuffer);
}
