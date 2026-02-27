#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "logger.h"
#include "vulkan_base/vulkan_base.h"
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <climits>
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../libs/SDL/src/video/stb_image.h"

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static constexpr uint32_t BASE_RENDER_WIDTH = 1240;
static constexpr uint32_t BASE_RENDER_HEIGHT = 720;
static constexpr const char* IMAGE_PATH_CANDIDATES[] = {
    "../assets/texture.png",
    "../libs/SDL/examples/renderer/06-textures/thumbnail.png",
    "../libs/SDL/test/testyuv.png"
};

struct Vertex {
    float position[2];
    float color[3];
};

static constexpr std::array<Vertex, 4> TRIANGLE_VERTICES = {{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}
}};

static constexpr std::array<uint16_t, 6> TRIANGLE_INDICES = {{
    0, 1, 2,
    2, 3, 0
}};

struct ApplicationState {
    SDL_Window* window;
    VulkanContext* context;
    VkSurfaceKHR surface;
    VulkanSwapChain swapchain;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    VulkanPipeline pipeline;
    std::vector<VkCommandPool> commandPools;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> inFlightFences;
    std::vector<VkSemaphore> acquireSemaphores;
    std::vector<VkSemaphore> releaseSemaphores;
    std::vector<VkFence> imagesInFlight;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t vertexCount;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    uint32_t textureWidth;
    uint32_t textureHeight;
    uint32_t framesInFlight;
    uint32_t currentFrame;
    bool framebufferResized;
};

bool handleMessage(ApplicationState* app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                return false;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                app->framebufferResized = true;
                break;
            default:
                break;
        }
    }
    return true;
}

bool createVertexResources(ApplicationState* app) {
    const VkDeviceSize vertexBufferSize = sizeof(Vertex) * TRIANGLE_VERTICES.size();
    if (!uploadToDeviceLocalBuffer(
            app->context,
            TRIANGLE_VERTICES.data(),
            vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            &app->vertexBuffer,
            &app->vertexBufferMemory)) {
        LOG_ERROR("Failed to upload vertex data to GPU buffer.");
        return false;
    }

    app->vertexCount = static_cast<uint32_t>(TRIANGLE_VERTICES.size());
    return true;
}

void destroyVertexResources(ApplicationState* app) {
    destroyBuffer(app->context, &app->vertexBuffer, &app->vertexBufferMemory);
    app->vertexCount = 0;
}

bool createIndexResources(ApplicationState* app) {
    const VkDeviceSize indexBufferSize = sizeof(uint16_t) * TRIANGLE_INDICES.size();
    if (!uploadToDeviceLocalBuffer(
            app->context,
            TRIANGLE_INDICES.data(),
            indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            &app->indexBuffer,
            &app->indexBufferMemory)) {
        LOG_ERROR("Failed to upload index data to GPU buffer.");
        return false;
    }

    app->indexCount = static_cast<uint32_t>(TRIANGLE_INDICES.size());
    return true;
}

void destroyIndexResources(ApplicationState* app) {
    destroyBuffer(app->context, &app->indexBuffer, &app->indexBufferMemory);
    app->indexCount = 0;
}

bool createImageResources(ApplicationState* app) {
    int imageWidth = 0;
    int imageHeight = 0;
    int imageChannels = 0;
    stbi_uc* pixels = nullptr;
    const char* loadedPath = nullptr;
    void* fileBytes = nullptr;
    size_t fileSize = 0;

    for (uint32_t i = 0; i < ARRAY_COUNT(IMAGE_PATH_CANDIDATES); ++i) {
        const char* candidatePath = IMAGE_PATH_CANDIDATES[i];
        fileBytes = SDL_LoadFile(candidatePath, &fileSize);
        if (fileBytes == nullptr || fileSize == 0) {
            if (fileBytes != nullptr) {
                SDL_free(fileBytes);
                fileBytes = nullptr;
            }
            continue;
        }

        if (fileSize > static_cast<size_t>(INT_MAX)) {
            SDL_free(fileBytes);
            fileBytes = nullptr;
            continue;
        }

        pixels = stbi_load_from_memory(
            static_cast<const stbi_uc*>(fileBytes),
            static_cast<int>(fileSize),
            &imageWidth,
            &imageHeight,
            &imageChannels,
            STBI_rgb_alpha
        );
        SDL_free(fileBytes);
        fileBytes = nullptr;
        if (pixels != nullptr) {
            loadedPath = candidatePath;
            break;
        }
    }

    if (pixels == nullptr) {
        LOG_ERROR("Failed to load image file from all candidate paths.");
        return false;
    }

    const bool uploadOk = uploadToDeviceLocalImageRGBA8(
        app->context,
        pixels,
        static_cast<uint32_t>(imageWidth),
        static_cast<uint32_t>(imageHeight),
        &app->textureImage,
        &app->textureImageMemory,
        &app->textureImageView
    );
    stbi_image_free(pixels);

    if (!uploadOk) {
        LOG_ERROR("Failed to upload loaded image to GPU.");
        return false;
    }

    app->textureWidth = static_cast<uint32_t>(imageWidth);
    app->textureHeight = static_cast<uint32_t>(imageHeight);
    LOG_INFO("Loaded image: ", loadedPath, " (", imageWidth, "x", imageHeight, ")");
    return true;
}

void destroyImageResources(ApplicationState* app) {
    destroyImageView(app->context, &app->textureImageView);
    destroyImage(app->context, &app->textureImage, &app->textureImageMemory);
    app->textureWidth = 0;
    app->textureHeight = 0;
}

void destroySwapchainResources(ApplicationState* app) {
    for (uint32_t i = 0; i < app->framebuffers.size(); i++) {
        VK(vkDestroyFramebuffer(app->context->device, app->framebuffers[i], nullptr));
    }
    app->framebuffers.clear();

    if (app->pipeline.pipeline != VK_NULL_HANDLE || app->pipeline.pipelineLayout != VK_NULL_HANDLE) {
        destroyPipeline(app->context, &app->pipeline);
        app->pipeline = {};
    }

    if (app->renderPass != VK_NULL_HANDLE) {
        destroyRenderPass(app->context, app->renderPass);
        app->renderPass = VK_NULL_HANDLE;
    }

    destroySwapChain(app->context, &app->swapchain);

    for (uint32_t i = 0; i < app->releaseSemaphores.size(); i++) {
        if (app->releaseSemaphores[i] != VK_NULL_HANDLE) {
            VK(vkDestroySemaphore(app->context->device, app->releaseSemaphores[i], nullptr));
        }
    }
    app->releaseSemaphores.clear();
    app->imagesInFlight.clear();
}

bool createSwapchainResources(ApplicationState* app) {
    app->swapchain = createSwapChain(app->context, app->surface, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    if (app->swapchain.swapChain == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to create swapchain.");
        return false;
    }

    app->renderPass = createRenderPass(app->context, app->swapchain.format);
    if (app->renderPass == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to create render pass.");
        destroySwapChain(app->context, &app->swapchain);
        return false;
    }

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

    app->pipeline = createPipeline(
        app->context,
        "../shaders/triangle_vert.spv",
        "../shaders/triangle_frag.spv",
        app->renderPass,
        app->swapchain.width,
        app->swapchain.height
    );
    if (app->pipeline.pipeline == VK_NULL_HANDLE || app->pipeline.pipelineLayout == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to create graphics pipeline.");
        for (uint32_t i = 0; i < app->framebuffers.size(); i++) {
            VK(vkDestroyFramebuffer(app->context->device, app->framebuffers[i], nullptr));
        }
        app->framebuffers.clear();
        destroyRenderPass(app->context, app->renderPass);
        app->renderPass = VK_NULL_HANDLE;
        destroySwapChain(app->context, &app->swapchain);
        return false;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    app->releaseSemaphores.resize(app->swapchain.images.size(), VK_NULL_HANDLE);
    for (uint32_t i = 0; i < app->swapchain.images.size(); i++) {
        VKA(vkCreateSemaphore(app->context->device, &semaphoreCreateInfo, nullptr, &app->releaseSemaphores[i]));
    }

    app->imagesInFlight.assign(app->swapchain.images.size(), VK_NULL_HANDLE);
    return true;
}

bool recreateSwapchain(ApplicationState* app) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    while (width == 0 || height == 0) {
        if (!handleMessage(app)) {
            return false;
        }
        SDL_Delay(10);
        SDL_GetWindowSizeInPixels(app->window, &width, &height);
    }

    VKA(vkDeviceWaitIdle(app->context->device));
    destroySwapchainResources(app);

    if (!createSwapchainResources(app)) {
        LOG_ERROR("Swapchain recreation failed.");
        return false;
    }

    app->framebufferResized = false;
    return true;
}

bool initApplication(ApplicationState* app) {
    app->window = nullptr;
    app->context = nullptr;
    app->surface = VK_NULL_HANDLE;
    app->swapchain = {};
    app->renderPass = VK_NULL_HANDLE;
    app->pipeline = {};
    app->vertexBuffer = VK_NULL_HANDLE;
    app->vertexBufferMemory = VK_NULL_HANDLE;
    app->vertexCount = 0;
    app->indexBuffer = VK_NULL_HANDLE;
    app->indexBufferMemory = VK_NULL_HANDLE;
    app->indexCount = 0;
    app->textureImage = VK_NULL_HANDLE;
    app->textureImageMemory = VK_NULL_HANDLE;
    app->textureImageView = VK_NULL_HANDLE;
    app->textureWidth = 0;
    app->textureHeight = 0;
    app->commandPools.clear();
    app->commandBuffers.clear();
    app->inFlightFences.clear();
    app->acquireSemaphores.clear();
    app->releaseSemaphores.clear();
    app->imagesInFlight.clear();
    app->framesInFlight = MAX_FRAMES_IN_FLIGHT;
    app->currentFrame = 0;
    app->framebufferResized = false;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: ", SDL_GetError());
        return false;
    }

    app->window = SDL_CreateWindow(
        "Vulkan Test",
        BASE_RENDER_WIDTH,
        BASE_RENDER_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
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

    if (!createSwapchainResources(app)) {
        VK(vkDestroySurfaceKHR(app->context->instance, app->surface, nullptr));
        exitVulkan(app->context);
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    if (!createVertexResources(app)) {
        destroySwapchainResources(app);
        VK(vkDestroySurfaceKHR(app->context->instance, app->surface, nullptr));
        exitVulkan(app->context);
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    if (!createIndexResources(app)) {
        destroyVertexResources(app);
        destroySwapchainResources(app);
        VK(vkDestroySurfaceKHR(app->context->instance, app->surface, nullptr));
        exitVulkan(app->context);
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    if (!createImageResources(app)) {
        destroyIndexResources(app);
        destroyVertexResources(app);
        destroySwapchainResources(app);
        VK(vkDestroySurfaceKHR(app->context->instance, app->surface, nullptr));
        exitVulkan(app->context);
        SDL_DestroyWindow(app->window);
        SDL_Quit();
        return false;
    }

    app->commandPools.resize(app->framesInFlight, VK_NULL_HANDLE);
    app->commandBuffers.resize(app->framesInFlight, VK_NULL_HANDLE);
    app->inFlightFences.resize(app->framesInFlight, VK_NULL_HANDLE);
    app->acquireSemaphores.resize(app->framesInFlight, VK_NULL_HANDLE);

    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        for (uint32_t frame = 0; frame < app->framesInFlight; frame++) {
            VKA(vkCreateSemaphore(app->context->device, &semaphoreCreateInfo, nullptr, &app->acquireSemaphores[frame]));
        }
    }

    for (uint32_t frame = 0; frame < app->framesInFlight; frame++) {
        VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK(vkCreateFence(app->context->device, &fenceCreateInfo, nullptr, &app->inFlightFences[frame]));

        VkCommandPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolCreateInfo.queueFamilyIndex = app->context->graphicsQueue.familyIndex;
        VKA(vkCreateCommandPool(app->context->device, &poolCreateInfo, 0, &app->commandPools[frame]));

        VkCommandBufferAllocateInfo bufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        bufferAllocateInfo.commandPool = app->commandPools[frame];
        bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferAllocateInfo.commandBufferCount = 1;
        VKA(vkAllocateCommandBuffers(app->context->device, &bufferAllocateInfo, &app->commandBuffers[frame]));
    }

    return true;
}

void renderApplication(ApplicationState* app) {
    while (handleMessage(app)) {
        static float greenChannel = 0.0f;
        greenChannel += 0.01f;
        if (greenChannel > 1.0f) greenChannel = 0.0f;

        if (app->framebufferResized) {
            if (!recreateSwapchain(app)) {
                break;
            }
            continue;
        }

        const uint32_t frame = app->currentFrame;
        VkFence frameFence = app->inFlightFences[frame];
        VkCommandPool frameCommandPool = app->commandPools[frame];
        VkCommandBuffer frameCommandBuffer = app->commandBuffers[frame];
        VkSemaphore acquireSemaphore = app->acquireSemaphores[frame];

        VKA(vkWaitForFences(app->context->device, 1, &frameFence, VK_TRUE, UINT64_MAX));

        uint32_t imageIndex = 0;
        VkResult acquireResult = VK(vkAcquireNextImageKHR(
            app->context->device,
            app->swapchain.swapChain,
            UINT64_MAX,
            acquireSemaphore,
            nullptr,
            &imageIndex
        ));
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            if (!recreateSwapchain(app)) {
                break;
            }
            continue;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            LOG_ERROR("vkAcquireNextImageKHR failed: ", static_cast<int>(acquireResult));
            break;
        }
        const bool swapchainSuboptimal = (acquireResult == VK_SUBOPTIMAL_KHR);

        if (app->imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            VKA(vkWaitForFences(app->context->device, 1, &app->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX));
        }
        app->imagesInFlight[imageIndex] = frameFence;

        VkSemaphore releaseSemaphore = app->releaseSemaphores[imageIndex];


        VKA(vkResetCommandPool(app->context->device, frameCommandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKA(vkBeginCommandBuffer(frameCommandBuffer, &beginInfo));
        {
            VkClearValue clearValue = {};
            clearValue.color = {{0.5f, greenChannel, 0.5f, 1.0f}};
            VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            beginInfo.renderPass = app->renderPass;
            beginInfo.framebuffer = app->framebuffers[imageIndex];
            beginInfo.renderArea = {{0, 0}, {app->swapchain.width, app->swapchain.height} };
            beginInfo.clearValueCount = 1;
            beginInfo.pClearValues = &clearValue;
            vkCmdBeginRenderPass(frameCommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);


            vkCmdBindPipeline(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline.pipeline);

            // Keep content fixed at BASE_RENDER size when the window grows.
            // If window is smaller than the base size, uniformly scale down to fit.
            const float scaleX = static_cast<float>(app->swapchain.width) / static_cast<float>(BASE_RENDER_WIDTH);
            const float scaleY = static_cast<float>(app->swapchain.height) / static_cast<float>(BASE_RENDER_HEIGHT);
            float renderScale = (scaleX < scaleY) ? scaleX : scaleY;
            if (renderScale > 1.0f) {
                renderScale = 1.0f;
            }

            uint32_t viewportWidth = static_cast<uint32_t>(static_cast<float>(BASE_RENDER_WIDTH) * renderScale);
            uint32_t viewportHeight = static_cast<uint32_t>(static_cast<float>(BASE_RENDER_HEIGHT) * renderScale);
            if (viewportWidth == 0) {
                viewportWidth = 1;
            }
            if (viewportHeight == 0) {
                viewportHeight = 1;
            }

            const int32_t viewportOffsetX = static_cast<int32_t>((app->swapchain.width - viewportWidth) / 2);
            const int32_t viewportOffsetY = static_cast<int32_t>((app->swapchain.height - viewportHeight) / 2);

            VkViewport viewport = {};
            viewport.x = static_cast<float>(viewportOffsetX);
            viewport.y = static_cast<float>(viewportOffsetY);
            viewport.width = static_cast<float>(viewportWidth);
            viewport.height = static_cast<float>(viewportHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(frameCommandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset = {viewportOffsetX, viewportOffsetY};
            scissor.extent = {viewportWidth, viewportHeight};
            vkCmdSetScissor(frameCommandBuffer, 0, 1, &scissor);

            VkDeviceSize vertexBufferOffset = 0;
            vkCmdBindVertexBuffers(frameCommandBuffer, 0, 1, &app->vertexBuffer, &vertexBufferOffset);
            vkCmdBindIndexBuffer(frameCommandBuffer, app->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(frameCommandBuffer, app->indexCount, 1, 0, 0, 0);




            vkCmdEndRenderPass(frameCommandBuffer);
        }
        VKA(vkEndCommandBuffer(frameCommandBuffer));


        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frameCommandBuffer;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &acquireSemaphore;
        VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitInfo.pWaitDstStageMask = &waitMask;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &releaseSemaphore;
        VKA(vkResetFences(app->context->device, 1, &frameFence));
        VKA(vkQueueSubmit(app->context->graphicsQueue.queue, 1, &submitInfo, frameFence));



        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &app->swapchain.swapChain;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &releaseSemaphore;
        VkResult presentResult = VK(vkQueuePresentKHR(app->context->graphicsQueue.queue, &presentInfo));
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || swapchainSuboptimal || app->framebufferResized) {
            if (!recreateSwapchain(app)) {
                break;
            }
        } else if (presentResult != VK_SUCCESS) {
            LOG_ERROR("vkQueuePresentKHR failed: ", static_cast<int>(presentResult));
            break;
        }

        app->currentFrame = (app->currentFrame + 1) % app->framesInFlight;
    }
}

void shutdownApplication(ApplicationState* app) {
    if (app->context == nullptr) {
        if (app->window != nullptr) {
            SDL_DestroyWindow(app->window);
        }
        SDL_Quit();
        return;
    }

    VKA(vkDeviceWaitIdle(app->context->device));

    destroyImageResources(app);
    destroyIndexResources(app);
    destroyVertexResources(app);
    destroySwapchainResources(app);

    for (uint32_t i = 0; i < app->acquireSemaphores.size(); i++) {
        if (app->acquireSemaphores[i] != VK_NULL_HANDLE) {
            VK(vkDestroySemaphore(app->context->device, app->acquireSemaphores[i], nullptr));
        }
    }
    app->acquireSemaphores.clear();

    for (uint32_t i = 0; i < app->inFlightFences.size(); i++) {
        if (app->inFlightFences[i] != VK_NULL_HANDLE) {
            VK(vkDestroyFence(app->context->device, app->inFlightFences[i], nullptr));
        }
    }
    app->inFlightFences.clear();

    for (uint32_t i = 0; i < app->commandPools.size(); i++) {
        if (app->commandPools[i] != VK_NULL_HANDLE) {
            VK(vkDestroyCommandPool(app->context->device, app->commandPools[i], nullptr));
        }
    }
    app->commandPools.clear();
    app->commandBuffers.clear();

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
