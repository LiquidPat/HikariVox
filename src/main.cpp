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
        LOG_ERROR("Error creating window", SDL_GetError());
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

    VulkanContext* context = initVulkan(instanceExtensionCount, enabledInstanceExtensions, 0, 0);
    if (context == nullptr) {
        LOG_ERROR("Vulkan initialization failed.");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }


    while (handleMessage()) {
    //VULKAN




    }

    exitVulkan(context);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
