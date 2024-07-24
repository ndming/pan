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
    const auto allocInfo = vk::CommandBufferAllocateInfo{ engine.getTransferCommandPool(), vk::CommandBufferLevel::ePrimary, 1 };
    const auto commandBuffer = engine.getDevice().allocateCommandBuffers(allocInfo)[0];

    // Use the command buffer once and wait with returning from the function until the copy operation has finished
    constexpr auto beginInfo = vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Buffer::endSingleTimeTransferCommands(const vk::CommandBuffer &commandBuffer, const Engine& engine) {
    const auto transferQueue = engine.getTransferQueue();

    commandBuffer.end();
    transferQueue.submit(vk::SubmitInfo{ {}, {}, {}, 1, &commandBuffer });
    transferQueue.waitIdle();
    engine.getDevice().freeCommandBuffers(engine.getTransferCommandPool(), 1, &commandBuffer);
}
