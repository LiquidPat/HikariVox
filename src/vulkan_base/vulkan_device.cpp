#include "vulkan_base.h"
#include <vector>
#include <cstring>

static bool hasLayer(const std::vector<VkLayerProperties>& layers, const char* layerName) {
    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, layerName) == 0) {
            return true;
        }
    }
    return false;
}

static bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const char* extensionName) {
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }
    return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData) {
    (void)messageType;
    (void)userData;

    const char* message = (callbackData != nullptr && callbackData->pMessage != nullptr) ? callbackData->pMessage : "No validation message text.";
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        LOG_ERROR("[Validation] ", message);
    } else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        LOG_WARN("[Validation] ", message);
    } else {
        LOG_INFO("[Validation] ", message);
    }

    return VK_FALSE;
}

static void fillDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo) {
    *createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo->messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
}

bool initVulkanInstance(VulkanContext* context, uint32_t instanceExtensionCount, const char* const* instanceExtensions) {
    uint32_t layerPropertyCount = 0;
    VKA(vkEnumerateInstanceLayerProperties(&layerPropertyCount, nullptr));
    std::vector<VkLayerProperties> layerProperties(layerPropertyCount);
    if (layerPropertyCount > 0) {
        VKA(vkEnumerateInstanceLayerProperties(&layerPropertyCount, layerProperties.data()));
    }
    for (uint32_t i = 0; i < layerPropertyCount; ++i) {
#ifdef VULKAN_INFO_OUTPUT
        LOG_INFO("Layer: ", layerProperties[i].layerName, " - ", layerProperties[i].description);
#endif
    }

    const char* enabledLayers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    const bool validationLayerAvailable = hasLayer(layerProperties, enabledLayers[0]);
    if (!validationLayerAvailable) {
        LOG_WARN("Validation layer '", enabledLayers[0], "' is not available. Continuing without instance validation layer.");
    } else {
        LOG_INFO("Enabled validation layer: ", enabledLayers[0]);
    }

    uint32_t availableInstanceExtensionCount = 0;
    VKA(vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, nullptr));
    std::vector<VkExtensionProperties> instanceExtensionProperties(availableInstanceExtensionCount);
    if (availableInstanceExtensionCount > 0) {
        VKA(vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, instanceExtensionProperties.data()));
    }
#ifdef VULKAN_INFO_OUTPUT
    for (uint32_t i = 0; i < availableInstanceExtensionCount; ++i) {
        LOG_INFO("Instance extension: ", instanceExtensionProperties[i].extensionName);
    }
#endif

    std::vector<const char*> enabledExtensions;
    enabledExtensions.reserve(instanceExtensionCount + 2);
    for (uint32_t i = 0; i < instanceExtensionCount; ++i) {
        enabledExtensions.push_back(instanceExtensions[i]);
    }

    auto isEnabled = [&enabledExtensions](const char* extensionName) -> bool {
        for (const char* enabledExtension : enabledExtensions) {
            if (std::strcmp(enabledExtension, extensionName) == 0) {
                return true;
            }
        }
        return false;
    };

    const bool hasDebugUtils = hasExtension(instanceExtensionProperties, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    const bool hasValidationFeatures = hasExtension(instanceExtensionProperties, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    if (hasDebugUtils && !isEnabled(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    if (hasValidationFeatures && !isEnabled(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
        enabledExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    }

    if (!hasDebugUtils) {
        LOG_WARN("Extension '", VK_EXT_DEBUG_UTILS_EXTENSION_NAME, "' is not available. Debug messenger will be disabled.");
    }
    if (!hasValidationFeatures) {
        LOG_WARN("Extension '", VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, "' is not available. Validation feature controls will be disabled.");
    }

    LOG_INFO("Enabled instance extensions:");
    for (const char* extensionName : enabledExtensions) {
        LOG_INFO("  - ", extensionName);
    }

    VkApplicationInfo applicationInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "Vulkan Test";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    applicationInfo.pEngineName = "HikariVox";
    applicationInfo.apiVersion = VK_API_VERSION_1_4;

    VkValidationFeatureEnableEXT validationFeatureEnables[] = {
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };
    VkValidationFeaturesEXT validationFeatures = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validationFeatures.enabledValidationFeatureCount = ARRAY_COUNT(validationFeatureEnables);
    validationFeatures.pEnabledValidationFeatures = validationFeatureEnables;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    fillDebugMessengerCreateInfo(&debugCreateInfo);

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledLayerCount = validationLayerAvailable ? ARRAY_COUNT(enabledLayers) : 0;
    createInfo.ppEnabledLayerNames = validationLayerAvailable ? enabledLayers : nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    void* instanceCreatePNext = nullptr;
    if (hasValidationFeatures && validationLayerAvailable) {
        validationFeatures.pNext = instanceCreatePNext;
        instanceCreatePNext = &validationFeatures;
        LOG_INFO("Enabled validation features: BEST_PRACTICES, SYNCHRONIZATION_VALIDATION");
    }
    if (hasDebugUtils) {
        debugCreateInfo.pNext = instanceCreatePNext;
        instanceCreatePNext = &debugCreateInfo;
    }
    createInfo.pNext = instanceCreatePNext;

   if (VK(vkCreateInstance(&createInfo, 0, &context->instance)) != VK_SUCCESS) {
        LOG_ERROR("Error creating Vulkan instance");
       return false;
    }

    context->debugMessenger = VK_NULL_HANDLE;
    if (hasDebugUtils) {
        PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createDebugUtilsMessenger != nullptr) {
            VkResult debugResult = VK(createDebugUtilsMessenger(context->instance, &debugCreateInfo, nullptr, &context->debugMessenger));
            if (debugResult == VK_SUCCESS) {
                LOG_INFO("Debug utils messenger created.");
            } else {
                LOG_WARN("Failed to create debug utils messenger. VkResult = ", static_cast<int>(debugResult));
            }
        } else {
            LOG_WARN("vkCreateDebugUtilsMessengerEXT is not available from instance proc addr.");
        }
    }

    return true;
}

    bool selectPhysicalDevice(VulkanContext* context) {
    uint32_t numDevices = 0;
    VKA(vkEnumeratePhysicalDevices(context->instance, &numDevices, 0));
    if (numDevices == 0) {
        LOG_ERROR("NO GPU with Vulkan support");
        context->physicalDevice = 0;
        return false;
    }
    VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[numDevices];
    VKA(vkEnumeratePhysicalDevices(context->instance, &numDevices, physicalDevices));
    context->physicalDevice = physicalDevices[0];
    LOG_INFO("Found ", numDevices, " GPU(s):");
    for (uint32_t i = 0; i < numDevices; ++i) {
        VkPhysicalDeviceProperties properties = {};
        VK(vkGetPhysicalDeviceProperties(physicalDevices[i], &properties));
        LOG_INFO("GPU", i, ": ", properties.deviceName);
    }
    context->physicalDevice = physicalDevices[0];
    VK(vkGetPhysicalDeviceProperties(context->physicalDevice, &context->physicalDeviceProperties));
    LOG_INFO("Selected GPU: ", context->physicalDeviceProperties.deviceName);


    delete[] physicalDevices;

    return true;
}


bool createLogicalDevice(VulkanContext* context, uint32_t deviceExtensionCount, const char** deviceExtensions) {

    //Queues
    uint32_t numQueueFamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context->physicalDevice, &numQueueFamilies, 0);
    VkQueueFamilyProperties* queueFamilies = new VkQueueFamilyProperties[numQueueFamilies];
    vkGetPhysicalDeviceQueueFamilyProperties(context->physicalDevice, &numQueueFamilies, queueFamilies);

    uint32_t graphicsQueueIndex = 0;
    for (uint32_t i = 0; i < numQueueFamilies; ++i) {
        VkQueueFamilyProperties queueFamily = queueFamilies[i];
        if (queueFamily.queueCount > 0) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueIndex = i;
                break;
            }
        }
    }


    float priorities = { 1.0f };
    VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priorities;

    VkPhysicalDeviceFeatures enabledFeatures = {};

    VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = deviceExtensionCount;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.pEnabledFeatures = &enabledFeatures;

    if (vkCreateDevice(context->physicalDevice, &createInfo, 0, &context->device)) {
        LOG_ERROR("Failed to create/find vulkan logical device");
        return false;
    }

    // Aquire queues
    context->graphicsQueue.familyIndex = graphicsQueueIndex;
    VK(vkGetDeviceQueue(context->device, graphicsQueueIndex, 0, &context->graphicsQueue.queue));


    return true;
}


VulkanContext* initVulkan(uint32_t instanceExtensionCount, const char* const* instanceExtensions, uint32_t deviceExtensionCount, const char** deviceExtensions) {
    VulkanContext* context = new VulkanContext{};


    if (!initVulkanInstance(context, instanceExtensionCount, instanceExtensions)) {
        return 0;
    }


    if (!selectPhysicalDevice(context)) {
        return 0;
    }

    if (!createLogicalDevice(context, deviceExtensionCount, deviceExtensions)) {
        return 0;
    }


    return context;
}

void exitVulkan(VulkanContext* context) {
    VKA(vkDeviceWaitIdle(context->device));
    VK(vkDestroyDevice(context->device, 0));

    if (context->debugMessenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyDebugUtilsMessenger != nullptr) {
            destroyDebugUtilsMessenger(context->instance, context->debugMessenger, nullptr);
        }
        context->debugMessenger = VK_NULL_HANDLE;
    }

    vkDestroyInstance(context->instance, 0);
}
