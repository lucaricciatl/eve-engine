#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <array>
#include <filesystem>
#include <vector>

namespace vkcore {

struct CubePipelineInputs {
    VkDevice device{VK_NULL_HANDLE};
    VkExtent2D swapChainExtent{};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkRenderPass shadowRenderPass{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout materialDescriptorSetLayout{VK_NULL_HANDLE};
    std::filesystem::path shaderDirectory{}; // directory containing cube.vert.spv, cube.frag.spv, shadow.vert.spv, particle.vert.spv, particle.frag.spv
};

constexpr size_t kStylePipelineCount = 7;

struct CubePipelineOutputs {
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline graphicsPipeline{VK_NULL_HANDLE};
    VkPipeline linePipeline{VK_NULL_HANDLE};
    VkPipeline shadowPipeline{VK_NULL_HANDLE};
    VkPipeline particlePipeline{VK_NULL_HANDLE};
    std::array<VkPipeline, kStylePipelineCount> stylePipelines{};
};

CubePipelineOutputs createCubePipelines(const CubePipelineInputs& inputs);
void destroyCubePipelines(VkDevice device, CubePipelineOutputs& pipelines);

} // namespace vkcore
