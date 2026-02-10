#include "engine/GpuCollisionSystem.hpp"

#include "engine/GameEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace vkengine {

namespace {

std::vector<char> readShaderFile(const std::filesystem::path& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename.string());
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

constexpr uint32_t WORKGROUP_SIZE = 256;

uint32_t divideRoundUp(uint32_t dividend, uint32_t divisor)
{
    return (dividend + divisor - 1) / divisor;
}

} // namespace

GpuCollisionSystem::GpuCollisionSystem() = default;

GpuCollisionSystem::~GpuCollisionSystem()
{
    cleanup();
}

void GpuCollisionSystem::initialize(VkDevice deviceHandle, VkPhysicalDevice physicalDeviceHandle,
                                    VkQueue queue, uint32_t queueFamily,
                                    const std::filesystem::path& shaderDirectory)
{
    if (initialized) {
        cleanup();
    }

    device = deviceHandle;
    physicalDevice = physicalDeviceHandle;
    computeQueue = queue;
    computeQueueFamily = queueFamily;
    shaderDir = shaderDirectory;

    createCommandPool();
    createDescriptorPool();
    createComputePipelines();
    createBuffers();

    initialized = true;
}

void GpuCollisionSystem::cleanup()
{
    if (!initialized || device == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(device);

    destroyBuffers();
    destroyPipelines();

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }

    if (computeFence != VK_NULL_HANDLE) {
        vkDestroyFence(device, computeFence, nullptr);
        computeFence = VK_NULL_HANDLE;
    }

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    initialized = false;
}

void GpuCollisionSystem::setConfig(const GpuCollisionConfig& newConfig)
{
    bool needsBufferRealloc = (newConfig.maxObjects != config.maxObjects ||
                               newConfig.maxPairs != config.maxPairs ||
                               newConfig.maxResults != config.maxResults);

    config = newConfig;

    if (initialized && needsBufferRealloc) {
        vkDeviceWaitIdle(device);
        destroyBuffers();
        createBuffers();
    }
}

void GpuCollisionSystem::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = computeQueueFamily;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute command buffer");
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute fence");
    }
}

void GpuCollisionSystem::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 32; // Enough for all our descriptor sets

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 8;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

VkShaderModule GpuCollisionSystem::loadShaderModule(const std::filesystem::path& path)
{
    auto code = readShaderFile(path);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module: " + path.string());
    }

    return shaderModule;
}

void GpuCollisionSystem::createComputePipelines()
{
    // Create descriptor set layouts

    // Broad phase layout: colliders (read), cell entries (write), cell counts (read/write)
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &broadPhaseDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create broad phase descriptor set layout");
        }
    }

    // Prefix sum layout: data buffer (read/write)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &prefixSumDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create prefix sum descriptor set layout");
        }
    }

    // Sort layout: cell entries (read/write)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &sortDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sort descriptor set layout");
        }
    }

    // Pair generation layout: cell entries (read), pairs (write), counter (read/write), cell ranges (read)
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        for (uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &pairGenDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pair gen descriptor set layout");
        }
    }

    // Narrow phase layout: colliders (read), pairs (read), results (write), counter (read/write)
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        for (uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &narrowPhaseDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create narrow phase descriptor set layout");
        }
    }

    // Create pipeline layouts with push constants

    // Broad phase pipeline layout
    {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(BroadPhasePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &broadPhaseDescriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &broadPhasePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create broad phase pipeline layout");
        }
    }

    // Prefix sum pipeline layout
    {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(PrefixSumPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &prefixSumDescriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &prefixSumPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create prefix sum pipeline layout");
        }
    }

    // Sort pipeline layout
    {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(BitonicSortPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &sortDescriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &sortPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sort pipeline layout");
        }
    }

    // Pair gen pipeline layout
    {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(PairGenPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &pairGenDescriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pairGenPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pair gen pipeline layout");
        }
    }

    // Narrow phase pipeline layout
    {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(NarrowPhasePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &narrowPhaseDescriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &narrowPhasePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create narrow phase pipeline layout");
        }
    }

    // Create compute pipelines
    auto createPipeline = [this](const std::filesystem::path& shaderPath, VkPipelineLayout layout) {
        VkShaderModule shaderModule = loadShaderModule(shaderPath);

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = shaderModule;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = layout;

        VkPipeline pipeline;
        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(device, shaderModule, nullptr);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline: " + shaderPath.string());
        }

        return pipeline;
    };

    const auto computeDir = shaderDir / "compute";
    broadPhasePipeline = createPipeline(computeDir / "collision_broad_phase.comp.spv", broadPhasePipelineLayout);
    broadPhaseWritePipeline = createPipeline(computeDir / "collision_broad_phase_write.comp.spv", broadPhasePipelineLayout);
    prefixSumPipeline = createPipeline(computeDir / "collision_prefix_sum.comp.spv", prefixSumPipelineLayout);
    bitonicSortPipeline = createPipeline(computeDir / "collision_bitonic_sort.comp.spv", sortPipelineLayout);
    pairGenPipeline = createPipeline(computeDir / "collision_pair_gen.comp.spv", pairGenPipelineLayout);
    narrowPhasePipeline = createPipeline(computeDir / "collision_narrow_phase.comp.spv", narrowPhasePipelineLayout);
}

void GpuCollisionSystem::createBuffers()
{
    const VkBufferUsageFlags gpuOnly = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkBufferUsageFlags gpuReadback = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkMemoryPropertyFlags deviceLocal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const VkMemoryPropertyFlags hostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Collider buffer
    VkDeviceSize colliderSize = sizeof(GpuColliderAABB) * config.maxObjects;
    createBuffer(colliderSize, gpuOnly, deviceLocal, colliderBuffer, colliderMemory);

    // Cell entry buffer (worst case: each object spans multiple cells)
    VkDeviceSize cellEntrySize = sizeof(GpuCellEntry) * config.maxObjects * 8; // Assume max 8 cells per object
    createBuffer(cellEntrySize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, deviceLocal, cellEntryBuffer, cellEntryMemory);

    // Cell count/offset buffers
    VkDeviceSize cellCountSize = sizeof(uint32_t) * config.maxObjects;
    createBuffer(cellCountSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                deviceLocal, cellCountBuffer, cellCountMemory);
    createBuffer(cellCountSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, deviceLocal, cellOffsetBuffer, cellOffsetMemory);

    // Cell range buffer
    VkDeviceSize cellRangeSize = sizeof(uint32_t) * 2 * config.gridResolution * config.gridResolution;
    createBuffer(cellRangeSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, deviceLocal, cellRangeBuffer, cellRangeMemory);

    // Pair buffer - needs TRANSFER_DST for CPU upload of pairs
    VkDeviceSize pairSize = sizeof(GpuCollisionPair) * config.maxPairs;
    createBuffer(pairSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocal, pairBuffer, pairMemory);

    // Pair count buffer
    createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                deviceLocal, pairCountBuffer, pairCountMemory);

    // Result buffer
    VkDeviceSize resultSize = sizeof(GpuCollisionResult) * config.maxResults;
    createBuffer(resultSize, gpuReadback, deviceLocal, resultBuffer, resultMemory);

    // Result count buffer
    createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                deviceLocal, resultCountBuffer, resultCountMemory);

    // Staging buffer for CPU-GPU transfers
    stagingBufferSize = std::max({colliderSize, resultSize, sizeof(uint32_t) * 4});
    createBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                hostVisible, stagingBuffer, stagingMemory);

    // Allocate and write descriptor sets
    std::array<VkDescriptorSetLayout, 5> layouts = {
        broadPhaseDescriptorLayout,
        prefixSumDescriptorLayout,
        sortDescriptorLayout,
        pairGenDescriptorLayout,
        narrowPhaseDescriptorLayout
    };

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, 5> sets;
    if (vkAllocateDescriptorSets(device, &allocInfo, sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    broadPhaseDescriptorSet = sets[0];
    prefixSumDescriptorSet = sets[1];
    sortDescriptorSet = sets[2];
    pairGenDescriptorSet = sets[3];
    narrowPhaseDescriptorSet = sets[4];

    // Update descriptor sets with buffer bindings
    auto writeBufferDescriptor = [](VkDescriptorSet set, uint32_t binding, VkBuffer buffer, VkDeviceSize size) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = size;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        return write;
    };

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(20);

    // Broad phase descriptor set
    bufferInfos.push_back({colliderBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({cellEntryBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({cellCountBuffer, 0, VK_WHOLE_SIZE});

    for (uint32_t i = 0; i < 3; ++i) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = broadPhaseDescriptorSet;
        write.dstBinding = i;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos[i];
        writes.push_back(write);
    }

    // Prefix sum descriptor set
    bufferInfos.push_back({cellCountBuffer, 0, VK_WHOLE_SIZE});
    {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = prefixSumDescriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos[3];
        writes.push_back(write);
    }

    // Sort descriptor set
    bufferInfos.push_back({cellEntryBuffer, 0, VK_WHOLE_SIZE});
    {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sortDescriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos[4];
        writes.push_back(write);
    }

    // Pair gen descriptor set
    bufferInfos.push_back({cellEntryBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({pairBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({pairCountBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({cellRangeBuffer, 0, VK_WHOLE_SIZE});

    for (uint32_t i = 0; i < 4; ++i) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = pairGenDescriptorSet;
        write.dstBinding = i;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos[5 + i];
        writes.push_back(write);
    }

    // Narrow phase descriptor set
    bufferInfos.push_back({colliderBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({pairBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({resultBuffer, 0, VK_WHOLE_SIZE});
    bufferInfos.push_back({resultCountBuffer, 0, VK_WHOLE_SIZE});

    for (uint32_t i = 0; i < 4; ++i) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = narrowPhaseDescriptorSet;
        write.dstBinding = i;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos[9 + i];
        writes.push_back(write);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void GpuCollisionSystem::destroyBuffers()
{
    auto destroyBuffer = [this](VkBuffer& buffer, VkDeviceMemory& memory) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    };

    destroyBuffer(colliderBuffer, colliderMemory);
    destroyBuffer(cellEntryBuffer, cellEntryMemory);
    destroyBuffer(cellCountBuffer, cellCountMemory);
    destroyBuffer(cellOffsetBuffer, cellOffsetMemory);
    destroyBuffer(cellRangeBuffer, cellRangeMemory);
    destroyBuffer(pairBuffer, pairMemory);
    destroyBuffer(pairCountBuffer, pairCountMemory);
    destroyBuffer(resultBuffer, resultMemory);
    destroyBuffer(resultCountBuffer, resultCountMemory);
    destroyBuffer(stagingBuffer, stagingMemory);
}

void GpuCollisionSystem::destroyPipelines()
{
    auto destroyPipeline = [this](VkPipeline& pipeline) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
    };

    auto destroyLayout = [this](VkPipelineLayout& layout) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    };

    auto destroyDescriptorLayout = [this](VkDescriptorSetLayout& layout) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    };

    destroyPipeline(broadPhasePipeline);
    destroyPipeline(broadPhaseWritePipeline);
    destroyPipeline(prefixSumPipeline);
    destroyPipeline(bitonicSortPipeline);
    destroyPipeline(pairGenPipeline);
    destroyPipeline(narrowPhasePipeline);

    destroyLayout(broadPhasePipelineLayout);
    destroyLayout(prefixSumPipelineLayout);
    destroyLayout(sortPipelineLayout);
    destroyLayout(pairGenPipelineLayout);
    destroyLayout(narrowPhasePipelineLayout);

    destroyDescriptorLayout(broadPhaseDescriptorLayout);
    destroyDescriptorLayout(prefixSumDescriptorLayout);
    destroyDescriptorLayout(sortDescriptorLayout);
    destroyDescriptorLayout(pairGenDescriptorLayout);
    destroyDescriptorLayout(narrowPhaseDescriptorLayout);
}

void GpuCollisionSystem::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties,
                                      VkBuffer& buffer, VkDeviceMemory& memory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

uint32_t GpuCollisionSystem::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void GpuCollisionSystem::copyBufferToDevice(const void* srcData, VkBuffer dstBuffer, VkDeviceSize size)
{
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mappedData);
    std::memcpy(mappedData, srcData, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkResetFences(device, 1, &computeFence);
    vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
    vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);
}

void GpuCollisionSystem::copyBufferFromDevice(VkBuffer srcBuffer, void* dstData, VkDeviceSize size)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, stagingBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkResetFences(device, 1, &computeFence);
    vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
    vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);

    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mappedData);
    std::memcpy(dstData, mappedData, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);
}

void GpuCollisionSystem::insertMemoryBarrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void GpuCollisionSystem::uploadColliders(Scene& scene)
{
    const auto& objects = scene.objectsCached();

    cpuColliders.clear();
    objectIndexMap.clear();
    cpuColliders.reserve(objects.size());
    objectIndexMap.reserve(objects.size());

    uint32_t gpuIndex = 0;
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& object = *objects[i];
        if (!object.hasCollider()) {
            continue;
        }

        const AABB& bounds = object.worldBounds();
        const Collider* collider = object.collider();

        GpuColliderAABB gpuAABB;
        gpuAABB.minPos = glm::vec4(bounds.min, static_cast<float>(gpuIndex));
        gpuAABB.maxPos = glm::vec4(bounds.max, collider->isStatic ? 1.0f : 0.0f);

        cpuColliders.push_back(gpuAABB);
        objectIndexMap.push_back(static_cast<uint32_t>(i));
        ++gpuIndex;
    }

    if (!cpuColliders.empty()) {
        copyBufferToDevice(cpuColliders.data(), colliderBuffer,
                          sizeof(GpuColliderAABB) * cpuColliders.size());
    }
}

void GpuCollisionSystem::recordBroadPhaseCommands(VkCommandBuffer cmd, uint32_t objectCount)
{
    BroadPhasePushConstants pushConstants{};
    pushConstants.cellSize = config.cellSize;
    pushConstants.invCellSize = 1.0f / config.cellSize;
    pushConstants.objectCount = objectCount;
    pushConstants.gridResolution = config.gridResolution;
    pushConstants.worldMin = glm::vec4(config.worldMin, 0.0f);
    pushConstants.worldMax = glm::vec4(config.worldMax, 0.0f);

    // Pass 1: Count cells per object
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, broadPhasePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, broadPhasePipelineLayout,
                           0, 1, &broadPhaseDescriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, broadPhasePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(BroadPhasePushConstants), &pushConstants);

    uint32_t workgroups = divideRoundUp(objectCount, WORKGROUP_SIZE);
    vkCmdDispatch(cmd, workgroups, 1, 1);

    insertMemoryBarrier(cmd);

    // Pass 2: Write cell entries (after prefix sum computes offsets)
    // Note: In a full implementation, we'd run prefix sum here
    // For simplicity, this version writes entries directly

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, broadPhaseWritePipeline);
    vkCmdDispatch(cmd, workgroups, 1, 1);

    insertMemoryBarrier(cmd);
}

void GpuCollisionSystem::recordSortCommands(VkCommandBuffer cmd, uint32_t entryCount)
{
    if (entryCount < 2) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bitonicSortPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sortPipelineLayout,
                           0, 1, &sortDescriptorSet, 0, nullptr);

    // Round up to next power of 2
    uint32_t n = 1;
    while (n < entryCount) {
        n *= 2;
    }

    // Bitonic sort: O(log^2 n) passes
    for (uint32_t k = 2; k <= n; k *= 2) {
        for (uint32_t j = k / 2; j > 0; j /= 2) {
            BitonicSortPushConstants pushConstants{};
            pushConstants.elementCount = entryCount;
            pushConstants.stage = k;
            pushConstants.passIndex = j;
            pushConstants.ascending = 1;

            vkCmdPushConstants(cmd, sortPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                              0, sizeof(BitonicSortPushConstants), &pushConstants);

            uint32_t workgroups = divideRoundUp(entryCount, WORKGROUP_SIZE);
            vkCmdDispatch(cmd, workgroups, 1, 1);

            insertMemoryBarrier(cmd);
        }
    }
}

void GpuCollisionSystem::recordPairGenCommands(VkCommandBuffer cmd, uint32_t entryCount)
{
    // Clear pair count
    vkCmdFillBuffer(cmd, pairCountBuffer, 0, sizeof(uint32_t), 0);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pairGenPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pairGenPipelineLayout,
                           0, 1, &pairGenDescriptorSet, 0, nullptr);

    PairGenPushConstants pushConstants{};
    pushConstants.totalEntries = entryCount;
    pushConstants.maxPairs = config.maxPairs;
    pushConstants.uniqueCellCount = entryCount; // Simplified: assume each entry is unique cell

    vkCmdPushConstants(cmd, pairGenPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(PairGenPushConstants), &pushConstants);

    uint32_t workgroups = divideRoundUp(entryCount, WORKGROUP_SIZE);
    vkCmdDispatch(cmd, workgroups, 1, 1);

    insertMemoryBarrier(cmd);
}

void GpuCollisionSystem::recordNarrowPhaseCommands(VkCommandBuffer cmd, uint32_t pairCount)
{
    // Clear result count
    vkCmdFillBuffer(cmd, resultCountBuffer, 0, sizeof(uint32_t), 0);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, narrowPhasePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, narrowPhasePipelineLayout,
                           0, 1, &narrowPhaseDescriptorSet, 0, nullptr);

    NarrowPhasePushConstants pushConstants{};
    pushConstants.pairCount = pairCount;
    pushConstants.maxResults = config.maxResults;
    pushConstants.penetrationSlop = config.penetrationSlop;

    vkCmdPushConstants(cmd, narrowPhasePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(NarrowPhasePushConstants), &pushConstants);

    uint32_t workgroups = divideRoundUp(pairCount, WORKGROUP_SIZE);
    vkCmdDispatch(cmd, workgroups, 1, 1);
}

std::vector<GpuCollisionSystem::CollisionData> GpuCollisionSystem::detectCollisions(Scene& scene)
{
    std::vector<CollisionData> results;

    if (!isEnabled()) {
        return results;
    }

    // Upload collider AABBs to GPU
    uploadColliders(scene);

    uint32_t objectCount = static_cast<uint32_t>(cpuColliders.size());
    if (objectCount < 2) {
        return results;
    }

    lastStats.objectCount = objectCount;

    // Simplified pipeline: For small object counts, skip broad phase and do all-pairs
    // This is more efficient for typical game scenarios with <100 objects
    uint32_t pairCount = 0;
    bool useSimplifiedPath = (objectCount <= 256);
    
    if (useSimplifiedPath) {
        // Generate all pairs directly (N*(N-1)/2 pairs)
        pairCount = objectCount * (objectCount - 1) / 2;
        lastStats.pairCount = pairCount;
        
        // Generate pairs on CPU and upload to GPU BEFORE starting command buffer
        std::vector<GpuCollisionPair> cpuPairs;
        cpuPairs.reserve(pairCount);
        for (uint32_t i = 0; i < objectCount; ++i) {
            for (uint32_t j = i + 1; j < objectCount; ++j) {
                GpuCollisionPair pair{};
                pair.objectA = i;
                pair.objectB = j;
                cpuPairs.push_back(pair);
            }
        }
        
        // Upload pairs to GPU (this uses its own command buffer submission)
        if (!cpuPairs.empty()) {
            copyBufferToDevice(cpuPairs.data(), pairBuffer, sizeof(GpuCollisionPair) * cpuPairs.size());
        }
    }

    // Record and submit compute commands
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    if (useSimplifiedPath) {
        // Clear counters
        vkCmdFillBuffer(commandBuffer, pairCountBuffer, 0, sizeof(uint32_t), 0);
        vkCmdFillBuffer(commandBuffer, resultCountBuffer, 0, sizeof(uint32_t), 0);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Run narrow phase directly with all pairs
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, narrowPhasePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, narrowPhasePipelineLayout,
                               0, 1, &narrowPhaseDescriptorSet, 0, nullptr);

        NarrowPhasePushConstants pushConstants{};
        pushConstants.pairCount = pairCount;
        pushConstants.maxResults = config.maxResults;
        pushConstants.penetrationSlop = config.penetrationSlop;

        vkCmdPushConstants(commandBuffer, narrowPhasePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(NarrowPhasePushConstants), &pushConstants);

        uint32_t workgroups = divideRoundUp(pairCount, WORKGROUP_SIZE);
        vkCmdDispatch(commandBuffer, workgroups, 1, 1);
    } else {
        // Full broad phase + narrow phase pipeline for larger object counts
        recordBroadPhaseCommands(commandBuffer, objectCount);

        // Estimate cell entry count (will be refined in full implementation)
        uint32_t estEntryCount = objectCount * 4;
        lastStats.cellEntryCount = estEntryCount;

        recordSortCommands(commandBuffer, estEntryCount);
        recordPairGenCommands(commandBuffer, estEntryCount);

        // Read back pair count and run narrow phase
        // Simplified: assume reasonable pair count
        uint32_t estPairCount = objectCount * objectCount / 4;
        lastStats.pairCount = estPairCount;

        recordNarrowPhaseCommands(commandBuffer, estPairCount);
    }

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkResetFences(device, 1, &computeFence);
    vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
    vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);

    // Read back collision count
    uint32_t collisionCount = 0;
    copyBufferFromDevice(resultCountBuffer, &collisionCount, sizeof(uint32_t));
    collisionCount = std::min(collisionCount, config.maxResults);
    lastStats.collisionCount = collisionCount;

    if (collisionCount == 0) {
        return results;
    }

    // Read back collision results
    std::vector<GpuCollisionResult> gpuResults(collisionCount);
    copyBufferFromDevice(resultBuffer, gpuResults.data(), sizeof(GpuCollisionResult) * collisionCount);

    // Convert to output format
    results.reserve(collisionCount);
    for (const auto& gpuResult : gpuResults) {
        if (gpuResult.contactPoint.w < 0.5f) {
            continue; // Invalid result
        }

        CollisionData data;
        data.objectA = objectIndexMap[gpuResult.objectA];
        data.objectB = objectIndexMap[gpuResult.objectB];
        data.normal = glm::vec3(gpuResult.normal);
        data.penetrationDepth = gpuResult.normal.w;
        data.contactPoint = glm::vec3(gpuResult.contactPoint);
        results.push_back(data);
    }

    return results;
}

} // namespace vkengine
