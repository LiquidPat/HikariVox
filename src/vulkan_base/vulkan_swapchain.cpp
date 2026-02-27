//
// Created by liqui on 26.02.2026.
//
#include "vulkan_base.h"

VulkanSwapChain createSwapChain(VulkanContext* context, VkSurfaceKHR surface, VkImageUsageFlags usage) {
    VulkanSwapChain result = {};
    VkBool32 supportsPresent = VK_FALSE;
    VKA(vkGetPhysicalDeviceSurfaceSupportKHR(context->physicalDevice, context->graphicsQueue.familyIndex, surface, &supportsPresent));
    if (!supportsPresent) {
        LOG_ERROR("Graphics queue not supported");
        return result;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    VKA(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice, surface, &surfaceCapabilities));

    if ((surfaceCapabilities.supportedUsageFlags & usage) != usage) {
        LOG_ERROR("Swapchain image usage flags are not supported by the surface");
        return result;
    }

    uint32_t numFormats = 0;
    VKA(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, surface, &numFormats, nullptr));
    if (numFormats == 0) {
        LOG_ERROR("No surface formats available");
        return result;
    }
    VkSurfaceFormatKHR* availableFormats = new VkSurfaceFormatKHR[numFormats];
    VKA(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, surface, &numFormats, availableFormats));

    // First available format
    VkFormat format = availableFormats[0].format;
    VkColorSpaceKHR colorSpace = availableFormats[0].colorSpace;
    if (surfaceCapabilities.currentExtent.width == UINT32_MAX) {
        surfaceCapabilities.currentExtent.width = surfaceCapabilities.minImageExtent.width;
    }
    if (surfaceCapabilities.currentExtent.height == UINT32_MAX) {
        surfaceCapabilities.currentExtent.height = surfaceCapabilities.minImageExtent.height;
    }

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format;
    createInfo.imageColorSpace = colorSpace;
    createInfo.imageExtent = surfaceCapabilities.currentExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = usage;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (VK(vkCreateSwapchainKHR(context->device, &createInfo, nullptr, &result.swapChain)) != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain");
        delete[] availableFormats;
        return result;
    }


    result.format = format;
    result.width = surfaceCapabilities.currentExtent.width;
    result.height = surfaceCapabilities.currentExtent.height;

    uint32_t numImages;
    VKA(vkGetSwapchainImagesKHR(context->device, result.swapChain, &numImages, nullptr));
    if (numImages == 0) {
        LOG_ERROR("Swapchain has no images");
        VK(vkDestroySwapchainKHR(context->device, result.swapChain, nullptr));
        result.swapChain = VK_NULL_HANDLE;
        delete[] availableFormats;
        return result;
    }

    // Acquire swapchain images
    result.images.resize(numImages);
    VKA(vkGetSwapchainImagesKHR(context->device, result.swapChain, &numImages, result.images.data()));

    // Create image views
    result.imageViews.resize(numImages);
    for (uint32_t i = 0; i < numImages; i++) {
        VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = result.images[i];

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        createInfo.format = format;
        createInfo.components = {};
        createInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VKA(vkCreateImageView(context->device, &createInfo, 0, &result.imageViews[i]));
    }

    delete[] availableFormats;
    return result;
}

void destroySwapChain(VulkanContext* context, VulkanSwapChain* swapChain) {

    if (swapChain == nullptr || swapChain->swapChain == VK_NULL_HANDLE) {
        return;
    }

    for (uint32_t i = 0; i < swapChain->imageViews.size(); i++) {
        VK(vkDestroyImageView(context->device, swapChain->imageViews[i], nullptr));
    }

    VK(vkDestroySwapchainKHR(context->device, swapChain->swapChain, nullptr));
    swapChain->swapChain = VK_NULL_HANDLE;
    swapChain->images.clear();
    swapChain->imageViews.clear();


}
