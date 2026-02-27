//
// Created by liqui on 27.02.2026.
//
#include "vulkan_base.h"
#include <cstring>

static bool beginSingleUseCommands(VulkanContext* context, VkCommandPool* commandPool, VkCommandBuffer* commandBuffer) {
    *commandPool = VK_NULL_HANDLE;
    *commandBuffer = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCreateInfo.queueFamilyIndex = context->graphicsQueue.familyIndex;
    if (VK(vkCreateCommandPool(context->device, &poolCreateInfo, nullptr, commandPool)) != VK_SUCCESS) {
        LOG_ERROR("Failed to create temporary command pool.");
        return false;
    }

    VkCommandBufferAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = *commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    if (VK(vkAllocateCommandBuffers(context->device, &allocateInfo, commandBuffer)) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate temporary command buffer.");
        VK(vkDestroyCommandPool(context->device, *commandPool, nullptr));
        *commandPool = VK_NULL_HANDLE;
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VKA(vkBeginCommandBuffer(*commandBuffer, &beginInfo));
    return true;
}

static bool endSingleUseCommands(VulkanContext* context, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
    VKA(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    VKA(vkQueueSubmit(context->graphicsQueue.queue, 1, &submitInfo, VK_NULL_HANDLE));
    VKA(vkQueueWaitIdle(context->graphicsQueue.queue));

    VK(vkDestroyCommandPool(context->device, commandPool, nullptr));
    return true;
}

uint32_t findMemoryType(VulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(context->physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        const bool typeMatches = ((typeFilter & (1u << i)) != 0);
        const bool propertyMatches = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeMatches && propertyMatches) {
            return i;
        }
    }

    LOG_ERROR("No suitable Vulkan memory type found.");
    assert(false);
    return 0;
}

bool createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    *buffer = VK_NULL_HANDLE;
    *bufferMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (VK(vkCreateBuffer(context->device, &bufferCreateInfo, nullptr, buffer)) != VK_SUCCESS) {
        LOG_ERROR("Failed to create buffer.");
        return false;
    }

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(context->device, *buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(context, memoryRequirements.memoryTypeBits, properties);
    if (VK(vkAllocateMemory(context->device, &allocateInfo, nullptr, bufferMemory)) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate buffer memory.");
        VK(vkDestroyBuffer(context->device, *buffer, nullptr));
        *buffer = VK_NULL_HANDLE;
        return false;
    }

    VKA(vkBindBufferMemory(context->device, *buffer, *bufferMemory, 0));
    return true;
}

void destroyBuffer(VulkanContext* context, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    if (buffer != nullptr && *buffer != VK_NULL_HANDLE) {
        VK(vkDestroyBuffer(context->device, *buffer, nullptr));
        *buffer = VK_NULL_HANDLE;
    }
    if (bufferMemory != nullptr && *bufferMemory != VK_NULL_HANDLE) {
        VK(vkFreeMemory(context->device, *bufferMemory, nullptr));
        *bufferMemory = VK_NULL_HANDLE;
    }
}

bool copyBuffer(VulkanContext* context, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (!beginSingleUseCommands(context, &commandPool, &commandBuffer)) {
        LOG_ERROR("Failed to begin temporary commands for buffer copy.");
        return false;
    }

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (!endSingleUseCommands(context, commandPool, commandBuffer)) {
        LOG_ERROR("Failed to submit temporary commands for buffer copy.");
        return false;
    }
    return true;
}

bool createImage(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory) {
    *image = VK_NULL_HANDLE;
    *imageMemory = VK_NULL_HANDLE;

    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.format = format;
    createInfo.tiling = tiling;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usage;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (VK(vkCreateImage(context->device, &createInfo, nullptr, image)) != VK_SUCCESS) {
        LOG_ERROR("Failed to create image.");
        return false;
    }

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(context->device, *image, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, memoryRequirements.memoryTypeBits, properties);
    if (VK(vkAllocateMemory(context->device, &allocInfo, nullptr, imageMemory)) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate image memory.");
        VK(vkDestroyImage(context->device, *image, nullptr));
        *image = VK_NULL_HANDLE;
        return false;
    }

    VKA(vkBindImageMemory(context->device, *image, *imageMemory, 0));
    return true;
}

void destroyImage(VulkanContext* context, VkImage* image, VkDeviceMemory* imageMemory) {
    if (image != nullptr && *image != VK_NULL_HANDLE) {
        VK(vkDestroyImage(context->device, *image, nullptr));
        *image = VK_NULL_HANDLE;
    }
    if (imageMemory != nullptr && *imageMemory != VK_NULL_HANDLE) {
        VK(vkFreeMemory(context->device, *imageMemory, nullptr));
        *imageMemory = VK_NULL_HANDLE;
    }
}

bool createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView* imageView) {
    *imageView = VK_NULL_HANDLE;

    VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (VK(vkCreateImageView(context->device, &createInfo, nullptr, imageView)) != VK_SUCCESS) {
        LOG_ERROR("Failed to create image view.");
        return false;
    }
    return true;
}

void destroyImageView(VulkanContext* context, VkImageView* imageView) {
    if (imageView != nullptr && *imageView != VK_NULL_HANDLE) {
        VK(vkDestroyImageView(context->device, *imageView, nullptr));
        *imageView = VK_NULL_HANDLE;
    }
}

bool transitionImageLayout(VulkanContext* context, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags) {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (!beginSingleUseCommands(context, &commandPool, &commandBuffer)) {
        LOG_ERROR("Failed to begin temporary commands for image layout transition.");
        return false;
    }

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        LOG_ERROR("Unsupported image layout transition: oldLayout=", static_cast<int>(oldLayout), ", newLayout=", static_cast<int>(newLayout));
        VK(vkDestroyCommandPool(context->device, commandPool, nullptr));
        return false;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    if (!endSingleUseCommands(context, commandPool, commandBuffer)) {
        LOG_ERROR("Failed to submit temporary commands for image layout transition.");
        return false;
    }
    return true;
}

bool copyBufferToImage(VulkanContext* context, VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height) {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (!beginSingleUseCommands(context, &commandPool, &commandBuffer)) {
        LOG_ERROR("Failed to begin temporary commands for buffer-to-image copy.");
        return false;
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(
        commandBuffer,
        srcBuffer,
        dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    if (!endSingleUseCommands(context, commandPool, commandBuffer)) {
        LOG_ERROR("Failed to submit temporary commands for buffer-to-image copy.");
        return false;
    }
    return true;
}

bool uploadToDeviceLocalImageRGBA8(VulkanContext* context, const void* pixelData, uint32_t width, uint32_t height, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView) {
    if (pixelData == nullptr || width == 0 || height == 0) {
        LOG_ERROR("Invalid image upload data or dimensions.");
        return false;
    }

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(
            context,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            &stagingBufferMemory)) {
        LOG_ERROR("Failed to create staging buffer for image upload.");
        return false;
    }

    {
        void* mappedMemory = nullptr;
        VKA(vkMapMemory(context->device, stagingBufferMemory, 0, imageSize, 0, &mappedMemory));
        std::memcpy(mappedMemory, pixelData, static_cast<size_t>(imageSize));
        vkUnmapMemory(context->device, stagingBufferMemory);
    }

    if (!createImage(
            context,
            width,
            height,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            image,
            imageMemory)) {
        LOG_ERROR("Failed to create device-local image.");
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    if (!transitionImageLayout(context, *image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT)) {
        LOG_ERROR("Failed image layout transition to TRANSFER_DST_OPTIMAL.");
        destroyImage(context, image, imageMemory);
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    if (!copyBufferToImage(context, stagingBuffer, *image, width, height)) {
        LOG_ERROR("Failed to copy staging buffer to image.");
        destroyImage(context, image, imageMemory);
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    if (!transitionImageLayout(context, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT)) {
        LOG_ERROR("Failed image layout transition to SHADER_READ_ONLY_OPTIMAL.");
        destroyImage(context, image, imageMemory);
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    if (!createImageView(context, *image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, imageView)) {
        LOG_ERROR("Failed to create image view for uploaded image.");
        destroyImage(context, image, imageMemory);
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
    return true;
}

bool uploadToDeviceLocalBuffer(VulkanContext* context, const void* srcData, VkDeviceSize size, VkBufferUsageFlags targetUsage, VkBuffer* dstBuffer, VkDeviceMemory* dstBufferMemory) {
    if (srcData == nullptr || size == 0) {
        LOG_ERROR("Invalid upload source data or size.");
        return false;
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(
            context,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            &stagingBufferMemory)) {
        LOG_ERROR("Failed to create staging buffer.");
        return false;
    }

    {
        void* mappedMemory = nullptr;
        VKA(vkMapMemory(context->device, stagingBufferMemory, 0, size, 0, &mappedMemory));
        std::memcpy(mappedMemory, srcData, static_cast<size_t>(size));
        vkUnmapMemory(context->device, stagingBufferMemory);
    }

    if (!createBuffer(
            context,
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | targetUsage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            dstBuffer,
            dstBufferMemory)) {
        LOG_ERROR("Failed to create device-local destination buffer.");
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    if (!copyBuffer(context, stagingBuffer, *dstBuffer, size)) {
        LOG_ERROR("Failed to copy staging buffer to destination buffer.");
        destroyBuffer(context, dstBuffer, dstBufferMemory);
        destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
        return false;
    }

    destroyBuffer(context, &stagingBuffer, &stagingBufferMemory);
    return true;
}
