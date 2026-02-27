//
// Created by liqui on 26.02.2026.
//
#include "../logger.h"

#include <vulkan/vulkan.h>
#include <cassert>
#include <vector>

#define ASSERT_VULKAN(val) if (val != VK_SUCCESS) {assert(false);}
#ifndef VK
#define VK(f) (f)
#endif
#ifndef VKA
#define VKA(f) ASSERT_VULKAN(VK(f))
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

struct VulkanQueue {
    VkQueue queue;
    uint32_t familyIndex;
};

struct VulkanSwapChain {
    VkSwapchainKHR swapChain;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
};

struct VulkanPipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct VulkanContext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkDevice device;
    VulkanQueue graphicsQueue;
};

VulkanContext* initVulkan(uint32_t instanceExtensionCount, const char* const* instanceExtensions, uint32_t deviceExtensionCount, const char** deviceExtensions);
void exitVulkan(VulkanContext* context);

VulkanSwapChain createSwapChain(VulkanContext* context, VkSurfaceKHR surface, VkImageUsageFlags usage);
void destroySwapChain(VulkanContext* context, VulkanSwapChain* swapChain);

VkRenderPass createRenderPass(VulkanContext* context, VkFormat format);
void destroyRenderPass(VulkanContext* context, VkRenderPass renderPass);

VulkanPipeline createPipeline(VulkanContext* context, const char* vertexShaderFilename, const char* fragmentShaderFilename, VkRenderPass renderPass, uint32_t width, uint32_t height);
void destroyPipeline(VulkanContext* context, VulkanPipeline* pipeline);

uint32_t findMemoryType(VulkanContext* context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
bool createBuffer(VulkanContext* context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);
void destroyBuffer(VulkanContext* context, VkBuffer* buffer, VkDeviceMemory* bufferMemory);
bool copyBuffer(VulkanContext* context, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
bool uploadToDeviceLocalBuffer(VulkanContext* context, const void* srcData, VkDeviceSize size, VkBufferUsageFlags targetUsage, VkBuffer* dstBuffer, VkDeviceMemory* dstBufferMemory);
bool createImage(VulkanContext* context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
void destroyImage(VulkanContext* context, VkImage* image, VkDeviceMemory* imageMemory);
bool createImageView(VulkanContext* context, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView* imageView);
void destroyImageView(VulkanContext* context, VkImageView* imageView);
bool transitionImageLayout(VulkanContext* context, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags);
bool copyBufferToImage(VulkanContext* context, VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height);
bool uploadToDeviceLocalImageRGBA8(VulkanContext* context, const void* pixelData, uint32_t width, uint32_t height, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView);

