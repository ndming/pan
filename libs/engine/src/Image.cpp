#include "engine/Image.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"


Image::Image(
    const vk::Image& image, const vk::ImageView& imageView, void* const allocation
) : _image{ image }, _imageView{ imageView },  _allocation{ allocation } {
}

void Image::transitionImageLayout(
    const vk::ImageLayout oldLayout,
    const vk::ImageLayout newLayout,
    const vk::PipelineStageFlags shaderReadStages,
    const Engine& engine
) const {
    const auto commandBuffer = beginSingleTimeCommands(engine);

    // One of the most common ways to transition the layout of an image is a pipeline barrier. Pipeline barriers are
    // primarily used for synchronizing access to resources, like making sure that an image was written to before it is
    // read, but they can also be used to transition layouts.
    auto barrier = vk::ImageMemoryBarrier{};
    barrier.image = _image;
    // It is possible to use eUndefined as oldLayout if we don’t care about the existing contents of the image.
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    // We're not using the barrier for transfering queue family ownership.
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    // Which aspect of the image will be affected
    barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, /* base mip */ 0, /* level count */ 1, 0, 1 };

    // Barriers are primarily used for synchronization purposes, so we must specify which types of operations that
    // involve the resource must happen before the barrier, and which operations that involve the resource must wait
    // on the barrier. We need to do that even if we're using vkQueueWaitIdle to manually synchronize.
    vk::PipelineStageFlags srcStage;
    vk::PipelineStageFlags dstStage;

    using enum vk::ImageLayout;
    if (oldLayout == eUndefined && newLayout == eTransferDstOptimal) {
        // Transfer writes that don’t need to wait on anything, we specify an empty access mask and the earliest
        // possible pipeline stage: eTopOfPipe
        barrier.srcAccessMask = static_cast<vk::AccessFlags>(0);
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == eTransferDstOptimal && newLayout == eShaderReadOnlyOptimal) {
        // Shader reads should wait on transfer writes.
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        // The image will be written in the same pipeline stage and subsequently read by a shader
        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = shaderReadStages;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(
        srcStage, dstStage,
        static_cast<vk::DependencyFlags>(0),
        {},      // for memory barriers
        {},      // for buffer memory barriers
        barrier  // for image memory barriers
    );

    endSingleTimeCommands(commandBuffer, engine);
}

void Image::copyBufferToImage(const vk::Buffer& buffer, const vk::Extent3D& extent, const Engine& engine) const {
    const auto commandBuffer = beginSingleTimeCommands(engine);

    // Just like with buffer copies, you need to specify which part of the buffer is going to be copied to
    // which part of the image.
    auto region = vk::BufferImageCopy{};
    // Specify the byte offset in the buffer at which the pixel values start
    region.bufferOffset = 0;
    // The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory. For example, we
    // could have some padding bytes between rows of the image. Specifying 0 for both indicates that the pixels are
    // simply tightly packed like they are in our case.
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    // The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy.
    region.imageOffset = vk::Offset3D{ 0, 0, 0 };
    region.imageExtent = extent;
    region.imageSubresource = { vk::ImageAspectFlagBits::eColor, /* mip level */ 0, 0, 1 };

    // We're assuming here that the image has been transitioned to the layout that is optimal for copying pixels to.
    commandBuffer.copyBufferToImage(buffer, _image, vk::ImageLayout::eTransferDstOptimal, region);

    endSingleTimeCommands(commandBuffer, engine);
}

vk::CommandBuffer Image::beginSingleTimeCommands(const Engine &engine) {
    const auto allocInfo = vk::CommandBufferAllocateInfo{ engine.getNativeTransferCommandPool(), vk::CommandBufferLevel::ePrimary, 1 };
    const auto commandBuffer = engine.getNativeDevice().allocateCommandBuffers(allocInfo)[0];

    // Use the command buffer once and wait with returning from the function until the copy operation has finished
    constexpr auto beginInfo = vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Image::endSingleTimeCommands(const vk::CommandBuffer& commandBuffer, const Engine& engine) {
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

vk::Image Image::getNativeImage() const {
    return _image;
}

vk::ImageView Image::getNativeImageView() const {
    return _imageView;
}

void* Image::getAllocation() const {
    return _allocation;
}
