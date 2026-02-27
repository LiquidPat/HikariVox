//
// Created by liqui on 26.02.2026.
//
#include "vulkan_base.h"

VkShaderModule createShaderModule(VulkanContext* context, const char* shaderFilename) {
    VkShaderModule result = {};

    //Read Shader File
    FILE* file = fopen(shaderFilename, "rb");
    if (!file) {
        LOG_ERROR("Shader file not found", shaderFilename);
        return result;
    }
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert((fileSize & 0x03) == 0);
    uint8_t* buffer = new uint8_t[fileSize];
    fread(buffer, 1, fileSize, file);

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = fileSize;
    createInfo.pCode = (uint32_t*)buffer;
    VKA(vkCreateShaderModule(context->device, &createInfo, nullptr, &result));

    delete[] buffer;
    fclose(file);

    return result;

}


VulkanPipeline createPipeline(VulkanContext* context, const char* vertexShaderFilename, const char* fragmentShaderFilename, VkRenderPass renderPass, uint32_t width, uint32_t height) {
    VkShaderModule vertexShaderModule = createShaderModule(context, vertexShaderFilename);
    VkShaderModule fragmentShaderModule = createShaderModule(context, fragmentShaderFilename);

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShaderModule;
    shaderStages[0].pName = "main";
    shaderStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShaderModule;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription vertexBindingDescription = {};
    vertexBindingDescription.binding = 0;
    vertexBindingDescription.stride = sizeof(float) * 5;
    vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexAttributeDescriptions[2] = {};
    vertexAttributeDescriptions[0].location = 0;
    vertexAttributeDescriptions[0].binding = 0;
    vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributeDescriptions[0].offset = 0;

    vertexAttributeDescriptions[1].location = 1;
    vertexAttributeDescriptions[1].binding = 0;
    vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributeDescriptions[1].offset = sizeof(float) * 2;

    VkPipelineVertexInputStateCreateInfo vertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexBindingDescription;
    vertexInputState.vertexAttributeDescriptionCount = ARRAY_COUNT(vertexAttributeDescriptions);
    vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    VkViewport viewport = {0.0f, 0.0f, (float)width, (float)height };
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    VkRect2D scissor = {{0, 0}, {width, height} };
    viewportState.pScissors = &scissor;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = ARRAY_COUNT(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples= VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT ;
    colorBlendAttachment.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;


    VkPipelineLayout pipelineLayout;
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        VKA(vkCreatePipelineLayout(context->device, &createInfo, nullptr, &pipelineLayout));
    }

    VkPipeline pipeline;
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        createInfo.stageCount = ARRAY_COUNT(shaderStages);
        createInfo.pStages = shaderStages;
        createInfo.pVertexInputState = &vertexInputState;
        createInfo.pInputAssemblyState = &inputAssemblyState;
        createInfo.pViewportState = &viewportState;
        createInfo.pRasterizationState = &rasterizationState;
        createInfo.pMultisampleState = &multisampleState;
        createInfo.pColorBlendState = &colorBlendState;
        createInfo.pDynamicState = &dynamicState;
        createInfo.layout = pipelineLayout;
        createInfo.renderPass = renderPass;
        createInfo.subpass = 0;
        VKA(vkCreateGraphicsPipelines(context->device, nullptr, 1, &createInfo, nullptr, &pipeline));
    }

    // Module can be destroyed after pipeline creation
    VK(vkDestroyShaderModule(context->device, vertexShaderModule, nullptr));
    VK(vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr));

    VulkanPipeline result = {};
    result.pipeline = pipeline;
    result.pipelineLayout = pipelineLayout;
    return result;
}

void destroyPipeline(VulkanContext* context, VulkanPipeline* pipeline) {
    VK(vkDestroyPipeline(context->device, pipeline->pipeline, nullptr));
    VK(vkDestroyPipelineLayout(context->device, pipeline->pipelineLayout, nullptr));
}
