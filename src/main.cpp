#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "logger.h"
#include "vulkan_base/vulkan_base.h"
#include <SDL3/SDL_vulkan.h>

bool handleMessage() {

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                return false;
        }
    }
    return true;
}


int main() {

    SDL_Window *window;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: ", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "Vulkan Test",
        1240,
        720,
        SDL_WINDOW_VULKAN
        );

    if (window == NULL) {
        LOG_ERROR("Error creating window: ", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    uint32_t instanceExtensionCount = 0;
    const char* const* enabledInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);
    if (enabledInstanceExtensions == nullptr || instanceExtensionCount == 0) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: ", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }



    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };


    VulkanContext* context = initVulkan(instanceExtensionCount, enabledInstanceExtensions, ARRAY_COUNT(deviceExtensions), deviceExtensions);
    if (context == nullptr) {
        LOG_ERROR("Vulkan initialization failed.");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, context->instance, nullptr, &surface)) {
        LOG_ERROR("SDL_Vulkan_CreateSurface failed: ", SDL_GetError());
        exitVulkan(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VulkanSwapChain swapchain = createSwapChain(context, surface, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    VkRenderPass renderPass = createRenderPass(context, swapchain.format);
    std::vector<VkFramebuffer> framebuffers;
    framebuffers.resize(swapchain.images.size());
    for (uint32_t i = 0; i < swapchain.images.size(); i++) {
        VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &swapchain.imageViews[i];
        createInfo.width = swapchain.width;
        createInfo.height = swapchain.height;
        createInfo.layers = 1;
        VKA(vkCreateFramebuffer(context->device, &createInfo, 0, &framebuffers[i]))
    }

    while (handleMessage()) {
    //VULKAN




    }

    VKA(vkDeviceWaitIdle(context->device));
    for (uint32_t i = 0; i < framebuffers.size(); i++) {
        VK(vkDestroyFramebuffer(context->device, framebuffers[i], nullptr));
    }
    framebuffers.clear();
    destroyRenderPass(context, renderPass);

    destroySwapChain(context, &swapchain);

   VK(vkDestroySurfaceKHR(context->instance, surface, nullptr));
    exitVulkan(context);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
