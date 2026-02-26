#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "logger.h"
#include "vulkan_base/vulkan_base.h"

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
    bool done = false;

    SDL_Init(SDL_INIT_VIDEO);

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

    VulkanContext* context = initVulkan();


    while (handleMessage()) {
    //VULKAN




    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
