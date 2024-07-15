#include "QueueFamilyFinder.h"


QueueFamilyFinder& QueueFamilyFinder::requestPresentFamily(const vk::SurfaceKHR &surface) {
    _surface = surface;
    _findPresentFamily = true;
    return *this;
}

QueueFamilyFinder& QueueFamilyFinder::requestComputeFamily(const bool async) {
    _asyncCompute = async;
    _findComputeFamily = true;
    return *this;
}

bool QueueFamilyFinder::find(const vk::PhysicalDevice& candidate) {
    const auto queueFamilies = candidate.getQueueFamilyProperties();

    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics &&
            queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) {
            // We don't care whether we're finding for a compute family or not. Vulkan requires an implementation which
            // supports graphics operations to have at least one queue family that supports both graphics and compute
            // operations. Therefore, we always look for the family supporting both operations so that the caller
            // has an option to fall back if async compute does not support.
            _graphicsFamily = i;
        }

        // It's important that we only update the present family if we have not found one, otherwise,
        // we may end up with a queue family that is suboptimal to use
        if (!_presentFamily.has_value() && _findPresentFamily && candidate.getSurfaceSupportKHR(i, _surface)) {
            _presentFamily = i;
        }
        if (_findComputeFamily && _asyncCompute && queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute &&
            !(queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
            // A dedicated compute family is a signal of support for async compute
            _computeFamily = i;
        }

        if (completed()) {
            break;
        }
    }

    return completed();
}

bool QueueFamilyFinder::completed(const bool relaxAsyncComputeRequest) const {
    auto completed = _graphicsFamily.has_value();

    if (_findPresentFamily) {
        completed = completed && _presentFamily.has_value();
    }

    if (_findComputeFamily && _asyncCompute && !relaxAsyncComputeRequest) {
        completed = completed && _computeFamily.has_value();
    }

    return completed;
}

uint32_t QueueFamilyFinder::getGraphicsFamily() const {
    return _graphicsFamily.value();
}

uint32_t QueueFamilyFinder::getPresentFamily() const {
    return _presentFamily.value();
}

uint32_t QueueFamilyFinder::getComputeFamily() const {
    return _computeFamily.value();
}

void QueueFamilyFinder::reset() {
    _graphicsFamily.reset();
    _presentFamily.reset();
    _computeFamily.reset();
}


