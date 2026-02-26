#include "vulkan_base.h"

bool initVulkanInstance(VulkanContext* context) {
    VkApplicationInfo applicationInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "Vulkan Test";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    applicationInfo.pEngineName = "HikariVox";
    applicationInfo.apiVersion = VK_API_VERSION_1_4;


    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &applicationInfo;

    vkCreateInstance(&createInfo, 0, &context->instance);

    return true;
}


VulkanContext* initVulkan() {
    VulkanContext* context = new VulkanContext;

    if (!initVulkanInstance(context)) {
        return 0;
    }



    return context;
}