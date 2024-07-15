#include "RenderPassBuilder.h"


RenderPassBuider & RenderPassBuider::sampleCount(const vk::SampleCountFlagBits msaa) {
    _msaaSamples = msaa;
    return *this;
}

vk::RenderPass RenderPassBuider::build(const vk::Device& device) const {
    const auto colorAttachment = vk::AttachmentDescription{
        {}, _surfaceFormat, _msaaSamples,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal };
    const auto colorAttachmentResolve = vk::AttachmentDescription{
        {}, _surfaceFormat, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR };

    static constexpr auto colorAttachmentRef = vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal };
    static constexpr auto colorAttachmentResolveRef = vk::AttachmentReference{ 1, vk::ImageLayout::eColorAttachmentOptimal };

    constexpr auto subpass = vk::SubpassDescription{
        {}, vk::PipelineBindPoint::eGraphics, {}, {}, 1, &colorAttachmentRef, &colorAttachmentResolveRef };

    const auto attachments = std::array{ colorAttachment, colorAttachmentResolve };

    auto dependency = vk::SubpassDependency{};
    // In subpass zero...
    dependency.dstSubpass = 0;
    // ... at these pipeline stages ...
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // ... wait before performing these operations ...
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    // ... until all operations of these types ...
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    // ... at these stages ...
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // ... occuring in submission order prior to vkCmdBeginRenderPass have completed execution.
    dependency.srcSubpass = vk::SubpassExternal;

    const auto renderPassInfo = vk::RenderPassCreateInfo{
        {}, static_cast<uint32_t>(attachments.size()), attachments.data(), 1, &subpass, 1, &dependency };
    return device.createRenderPass(renderPassInfo);
}
