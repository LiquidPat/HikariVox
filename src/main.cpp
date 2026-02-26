#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "logger.h"
#include "vulkan_base/vulkan_base.h"
#include <SDL3/SDL_vulkan.h>

struct ApplicationState {
    SDL_Window* window;
    VulkanContext* context;
    VkSurfaceKHR surface;
    VulkanSwapChain swapchain;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    
};

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

bool initApplication(ApplicationState* app) {
    app->window = nullptr;
    app->context = nullptr;
    app->surface = VK_NULL_HANDLE;
    app->renderPass = VK_NULL_HANDLE;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: ", SDL_GetError());
        return false;
    }

    app->window = SDL_CreateWindow(
        "Vulkan Test",
        1240,
        720,
        SDL_WINDOW_VULKAN
        );

    if (app->window == NULL) {
        LOG_ERROR("Error creating window: ", SDL_GetError());
        SDL_Quit();
        return false;
    }

    uint32_t instanceExtensionCount = 0;
    const char* const* enabledInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);
    if (enabledInstanceExtensions == nullptr || instanceExtensionCount == 0) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: ", SDL_GetError());
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    app->context = initVulkan(instanceExtensionCount, enabledInstanceExtensions, ARRAY_COUNT(deviceExtensions), deviceExtensions);
    if (app->context == nullptr) {
        LOG_ERROR("Vulkan initialization failed.");
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    if (!SDL_Vulkan_CreateSurface(app->window, app->context->instance, nullptr, &app->surface)) {
        LOG_ERROR("SDL_Vulkan_CreateSurface failed: ", SDL_GetError());
        exitVulkan(app->context);
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    app->swapchain = createSwapChain(app->context, app->surface, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    app->renderPass = createRenderPass(app->context, app->swapchain.format);
    app->framebuffers.resize(app->swapchain.images.size());
    for (uint32_t i = 0; i < app->swapchain.images.size(); i++) {
        VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = app->renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &app->swapchain.imageViews[i];
        createInfo.width = app->swapchain.width;
        createInfo.height = app->swapchain.height;
        createInfo.layers = 1;
        VKA(vkCreateFramebuffer(app->context->device, &createInfo, 0, &app->framebuffers[i]))
    }

    return true;
}

void renderApplication() {
    while (handleMessage()) {
    //VULKAN
    }
}

void shutdownApplication(ApplicationState* app) {
    VKA(vkDeviceWaitIdle(app->context->device));
    for (uint32_t i = 0; i < app->framebuffers.size(); i++) {
        VK(vkDestroyFramebuffer(app->context->device, app->framebuffers[i], nullptr));
    }
    app->framebuffers.clear();
    destroyRenderPass(app->context, app->renderPass);

    destroySwapChain(app->context, &app->swapchain);

   VK(vkDestroySurfaceKHR(app->context->instance, app->surface, nullptr));
    exitVulkan(app->context);

    SDL_DestroyWindow(app->window);
    SDL_Quit();
}

int main() {
    ApplicationState app = {};
    if (!initApplication(&app)) {
        return 1;
    }

    renderApplication();
    shutdownApplication(&app);
    return 0;
}
