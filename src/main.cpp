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
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence fence;
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

    {
        VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK(vkCreateFence(app->context->device, &createInfo, nullptr, &app->fence));
    }


    {
        VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        createInfo.queueFamilyIndex = app->context->graphicsQueue.familyIndex;
        VKA(vkCreateCommandPool(app->context->device, &createInfo, 0, &app->commandPool));
    }
    {
        VkCommandBufferAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocateInfo.commandPool = app->commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        VKA(vkAllocateCommandBuffers(app->context->device, &allocateInfo, &app->commandBuffer));
    }

    return true;
}

void renderApplication(ApplicationState* app) {
    while (handleMessage()) {
        static float greenChannel = 0.0f;
        greenChannel += 0.01f;
        if (greenChannel > 1.0f) greenChannel = 0.0f;
        uint32_t imageIndex = 0;
        VK(vkAcquireNextImageKHR(app->context->device, app->swapchain.swapChain, UINT64_MAX, 0, app->fence, &imageIndex));

        VKA(vkResetCommandPool(app->context->device, app->commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKA(vkBeginCommandBuffer(app->commandBuffer, &beginInfo));
        {
            VkClearValue clearValue = {0.5f, greenChannel, 0.5f, 1.0f};
            VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            beginInfo.renderPass = app->renderPass;
            beginInfo.framebuffer = app->framebuffers[imageIndex];
            beginInfo.renderArea = {{0, 0}, {app->swapchain.width, app->swapchain.height} };
            beginInfo.clearValueCount = 1;
            beginInfo.pClearValues = &clearValue;
            vkCmdBeginRenderPass(app->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);


            vkCmdEndRenderPass(app->commandBuffer);
        }
        VKA(vkEndCommandBuffer(app->commandBuffer));

        VKA(vkWaitForFences(app->context->device, 1, &app->fence, VK_TRUE, UINT64_MAX));
        VKA(vkResetFences(app->context->device, 1, &app->fence));

        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &app->commandBuffer;
        VKA(vkQueueSubmit(app->context->graphicsQueue.queue, 1, &submitInfo, 0));

        VKA(vkDeviceWaitIdle(app->context->device));

        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &app->swapchain.swapChain;
        presentInfo.pImageIndices = &imageIndex;
        VK(vkQueuePresentKHR(app->context->graphicsQueue.queue, &presentInfo));
    }
}

void shutdownApplication(ApplicationState* app) {
        VKA(vkDeviceWaitIdle(app->context->device));

    VK(vkDestroyFence(app->context->device, app->fence, nullptr));
    VK(vkDestroyCommandPool(app->context->device, app->commandPool, nullptr));

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

    renderApplication(&app);
    shutdownApplication(&app);
    return 0;
}
