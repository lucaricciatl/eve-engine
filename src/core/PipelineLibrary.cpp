#include "core/PipelineLibrary.hpp"

#include "core/RenderData.hpp"
#include "core/Vertex.hpp"

#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>

namespace vkcore {
namespace {

std::vector<char> readFile(const std::filesystem::path& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file " + filename.string());
    }
    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

} // namespace

CubePipelineOutputs createCubePipelines(const CubePipelineInputs& inputs)
{
    if (inputs.device == VK_NULL_HANDLE || inputs.renderPass == VK_NULL_HANDLE || inputs.shadowRenderPass == VK_NULL_HANDLE) {
        throw std::runtime_error("PipelineLibrary: missing required Vulkan handles");
    }

    const auto shaderDir = inputs.shaderDirectory;
    auto vertShaderCode = readFile(shaderDir / "cube.vert.spv");
    auto fragShaderCode = readFile(shaderDir / "cube.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(inputs.device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(inputs.device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = Vertex::bindingDescription();
    auto attributeDescriptions = Vertex::attributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(inputs.swapChainExtent.width);
    viewport.height = static_cast<float>(inputs.swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = inputs.swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(vkcore::ObjectPushConstants);

    std::array<VkDescriptorSetLayout, 2> setLayouts = {inputs.descriptorSetLayout, inputs.materialDescriptorSetLayout};

    CubePipelineOutputs outputs{};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(inputs.device, &pipelineLayoutInfo, nullptr, &outputs.pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(inputs.device, fragShaderModule, nullptr);
        vkDestroyShaderModule(inputs.device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = outputs.pipelineLayout;
    pipelineInfo.renderPass = inputs.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(inputs.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outputs.graphicsPipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(inputs.device, outputs.pipelineLayout, nullptr);
        vkDestroyShaderModule(inputs.device, fragShaderModule, nullptr);
        vkDestroyShaderModule(inputs.device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    VkPipelineInputAssemblyStateCreateInfo lineAssembly = inputAssembly;
    lineAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkGraphicsPipelineCreateInfo lineInfo = pipelineInfo;
    lineInfo.pInputAssemblyState = &lineAssembly;

    if (vkCreateGraphicsPipelines(inputs.device, VK_NULL_HANDLE, 1, &lineInfo, nullptr, &outputs.linePipeline) != VK_SUCCESS) {
        vkDestroyPipeline(inputs.device, outputs.graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(inputs.device, outputs.pipelineLayout, nullptr);
        vkDestroyShaderModule(inputs.device, fragShaderModule, nullptr);
        vkDestroyShaderModule(inputs.device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create line pipeline");
    }

    vkDestroyShaderModule(inputs.device, fragShaderModule, nullptr);
    vkDestroyShaderModule(inputs.device, vertShaderModule, nullptr);

    // Shadow pipeline
    auto shadowVertCode = readFile(shaderDir / "shadow.vert.spv");
    VkShaderModule shadowVertModule = createShaderModule(inputs.device, shadowVertCode);

    VkPipelineShaderStageCreateInfo shadowStage{};
    shadowStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shadowStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shadowStage.module = shadowVertModule;
    shadowStage.pName = "main";

    VkPipelineViewportStateCreateInfo shadowViewportState{};
    VkViewport shadowViewport{};
    shadowViewport.x = 0.0f;
    shadowViewport.y = 0.0f;
    shadowViewport.width = static_cast<float>(2048);
    shadowViewport.height = static_cast<float>(2048);
    shadowViewport.minDepth = 0.0f;
    shadowViewport.maxDepth = 1.0f;
    VkRect2D shadowScissor{};
    shadowScissor.offset = {0, 0};
    shadowScissor.extent = {2048, 2048};
    shadowViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    shadowViewportState.viewportCount = 1;
    shadowViewportState.pViewports = &shadowViewport;
    shadowViewportState.scissorCount = 1;
    shadowViewportState.pScissors = &shadowScissor;

    VkPipelineDepthStencilStateCreateInfo shadowDepthStencil{};
    shadowDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    shadowDepthStencil.depthTestEnable = VK_TRUE;
    shadowDepthStencil.depthWriteEnable = VK_TRUE;
    shadowDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    shadowDepthStencil.depthBoundsTestEnable = VK_FALSE;
    shadowDepthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo shadowColorBlending{};
    shadowColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    shadowColorBlending.logicOpEnable = VK_FALSE;
    shadowColorBlending.attachmentCount = 0;
    shadowColorBlending.pAttachments = nullptr;

    VkGraphicsPipelineCreateInfo shadowInfo{};
    shadowInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    shadowInfo.stageCount = 1;
    shadowInfo.pStages = &shadowStage;
    shadowInfo.pVertexInputState = &vertexInputInfo;
    shadowInfo.pInputAssemblyState = &inputAssembly;
    shadowInfo.pViewportState = &shadowViewportState;
    shadowInfo.pRasterizationState = &rasterizer;
    shadowInfo.pMultisampleState = &multisampling;
    shadowInfo.pDepthStencilState = &shadowDepthStencil;
    shadowInfo.pColorBlendState = &shadowColorBlending;
    shadowInfo.layout = outputs.pipelineLayout;
    shadowInfo.renderPass = inputs.shadowRenderPass;
    shadowInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(inputs.device, VK_NULL_HANDLE, 1, &shadowInfo, nullptr, &outputs.shadowPipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(inputs.device, shadowVertModule, nullptr);
        destroyCubePipelines(inputs.device, outputs);
        throw std::runtime_error("Failed to create shadow pipeline");
    }

    vkDestroyShaderModule(inputs.device, shadowVertModule, nullptr);

    // Particle pipeline
    auto particleVertCode = readFile(shaderDir / "particle.vert.spv");
    auto particleFragCode = readFile(shaderDir / "particle.frag.spv");
    VkShaderModule particleVertModule = createShaderModule(inputs.device, particleVertCode);
    VkShaderModule particleFragModule = createShaderModule(inputs.device, particleFragCode);

    VkPipelineShaderStageCreateInfo particleVertStage{};
    particleVertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    particleVertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    particleVertStage.module = particleVertModule;
    particleVertStage.pName = "main";

    VkPipelineShaderStageCreateInfo particleFragStage{};
    particleFragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    particleFragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    particleFragStage.module = particleFragModule;
    particleFragStage.pName = "main";

    VkPipelineShaderStageCreateInfo particleStages[] = {particleVertStage, particleFragStage};

    auto particleBinding = ParticleVertex::bindingDescription();
    auto particleAttributes = ParticleVertex::attributeDescriptions();

    VkPipelineVertexInputStateCreateInfo particleVertexInput{};
    particleVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    particleVertexInput.vertexBindingDescriptionCount = 1;
    particleVertexInput.pVertexBindingDescriptions = &particleBinding;
    particleVertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(particleAttributes.size());
    particleVertexInput.pVertexAttributeDescriptions = particleAttributes.data();

    VkPipelineViewportStateCreateInfo particleViewportState{};
    particleViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    particleViewportState.viewportCount = 1;
    particleViewportState.pViewports = &viewport;
    particleViewportState.scissorCount = 1;
    particleViewportState.pScissors = &scissor;

    VkPipelineDepthStencilStateCreateInfo particleDepth{};
    particleDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    particleDepth.depthTestEnable = VK_TRUE;
    particleDepth.depthWriteEnable = VK_FALSE;
    particleDepth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    particleDepth.depthBoundsTestEnable = VK_FALSE;
    particleDepth.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState particleBlendAttachment{};
    particleBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    particleBlendAttachment.blendEnable = VK_TRUE;
    particleBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    particleBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    particleBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    particleBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    particleBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    particleBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo particleColorBlend{};
    particleColorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    particleColorBlend.logicOpEnable = VK_FALSE;
    particleColorBlend.attachmentCount = 1;
    particleColorBlend.pAttachments = &particleBlendAttachment;

    VkGraphicsPipelineCreateInfo particleInfo{};
    particleInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    particleInfo.stageCount = 2;
    particleInfo.pStages = particleStages;
    particleInfo.pVertexInputState = &particleVertexInput;
    particleInfo.pInputAssemblyState = &inputAssembly;
    particleInfo.pViewportState = &particleViewportState;
    particleInfo.pRasterizationState = &rasterizer;
    particleInfo.pMultisampleState = &multisampling;
    particleInfo.pDepthStencilState = &particleDepth;
    particleInfo.pColorBlendState = &particleColorBlend;
    particleInfo.layout = outputs.pipelineLayout;
    particleInfo.renderPass = inputs.renderPass;
    particleInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(inputs.device, VK_NULL_HANDLE, 1, &particleInfo, nullptr, &outputs.particlePipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(inputs.device, particleFragModule, nullptr);
        vkDestroyShaderModule(inputs.device, particleVertModule, nullptr);
        destroyCubePipelines(inputs.device, outputs);
        throw std::runtime_error("Failed to create particle pipeline");
    }

    vkDestroyShaderModule(inputs.device, particleFragModule, nullptr);
    vkDestroyShaderModule(inputs.device, particleVertModule, nullptr);

    return outputs;
}

void destroyCubePipelines(VkDevice device, CubePipelineOutputs& pipelines)
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    if (pipelines.graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipelines.graphicsPipeline, nullptr);
        pipelines.graphicsPipeline = VK_NULL_HANDLE;
    }
    if (pipelines.linePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipelines.linePipeline, nullptr);
        pipelines.linePipeline = VK_NULL_HANDLE;
    }
    if (pipelines.shadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipelines.shadowPipeline, nullptr);
        pipelines.shadowPipeline = VK_NULL_HANDLE;
    }
    if (pipelines.particlePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipelines.particlePipeline, nullptr);
        pipelines.particlePipeline = VK_NULL_HANDLE;
    }
    if (pipelines.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelines.pipelineLayout, nullptr);
        pipelines.pipelineLayout = VK_NULL_HANDLE;
    }
}

} // namespace vkcore
