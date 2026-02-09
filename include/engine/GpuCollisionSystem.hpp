#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include "engine/GpuCollisionTypes.hpp"

#include <filesystem>
#include <functional>
#include <vector>

namespace vkengine {

class Scene;
class GameObject;
struct AABB;

namespace physics_detail {
struct CollisionResult;
}

// GPU-accelerated collision detection system using Vulkan compute shaders
// Implements broad-phase spatial hashing followed by narrow-phase AABB tests
// Based on GPU Gems 3, Chapter 32, ported from CUDA to Vulkan compute
class GpuCollisionSystem {
public:
    GpuCollisionSystem();
    ~GpuCollisionSystem();

    // Non-copyable, non-movable (owns Vulkan resources)
    GpuCollisionSystem(const GpuCollisionSystem&) = delete;
    GpuCollisionSystem& operator=(const GpuCollisionSystem&) = delete;
    GpuCollisionSystem(GpuCollisionSystem&&) = delete;
    GpuCollisionSystem& operator=(GpuCollisionSystem&&) = delete;

    // Initialize Vulkan compute resources
    // Must be called after Vulkan device is created
    void initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                   VkQueue computeQueue, uint32_t computeQueueFamily,
                   const std::filesystem::path& shaderDirectory);

    // Cleanup Vulkan resources
    void cleanup();

    // Configure collision detection parameters
    void setConfig(const GpuCollisionConfig& config);
    [[nodiscard]] const GpuCollisionConfig& getConfig() const noexcept { return config; }

    // Enable/disable GPU collision detection
    void setEnabled(bool enabled) noexcept { config.enabled = enabled; }
    [[nodiscard]] bool isEnabled() const noexcept { return config.enabled && initialized; }

    // Perform GPU collision detection on the scene
    // Returns collision results that can be used by the physics solver
    struct CollisionData {
        std::uint32_t objectA{0};
        std::uint32_t objectB{0};
        glm::vec3 normal{0.0f};
        float penetrationDepth{0.0f};
        glm::vec3 contactPoint{0.0f};
    };

    // Main collision detection entry point
    // Uploads AABBs to GPU, runs compute shaders, downloads results
    std::vector<CollisionData> detectCollisions(Scene& scene);

    // Get statistics from last detection pass
    struct Statistics {
        std::uint32_t objectCount{0};
        std::uint32_t cellEntryCount{0};
        std::uint32_t pairCount{0};
        std::uint32_t collisionCount{0};
        float gpuTimeMs{0.0f};
    };
    [[nodiscard]] const Statistics& getLastStatistics() const noexcept { return lastStats; }

private:
    // Vulkan resource management
    void createCommandPool();
    void createDescriptorPool();
    void createComputePipelines();
    void createBuffers();
    void destroyBuffers();
    void destroyPipelines();

    // Shader loading
    VkShaderModule loadShaderModule(const std::filesystem::path& path);

    // Buffer helpers
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& memory);
    void copyBufferToDevice(const void* srcData, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferFromDevice(VkBuffer srcBuffer, void* dstData, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Compute dispatch helpers
    void recordBroadPhaseCommands(VkCommandBuffer cmd, std::uint32_t objectCount);
    void recordSortCommands(VkCommandBuffer cmd, std::uint32_t entryCount);
    void recordPairGenCommands(VkCommandBuffer cmd, std::uint32_t entryCount);
    void recordNarrowPhaseCommands(VkCommandBuffer cmd, std::uint32_t pairCount);
    void insertMemoryBarrier(VkCommandBuffer cmd);

    // Upload scene AABBs to GPU
    void uploadColliders(Scene& scene);

private:
    bool initialized{false};
    GpuCollisionConfig config{};
    Statistics lastStats{};

    // Vulkan handles
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkQueue computeQueue{VK_NULL_HANDLE};
    uint32_t computeQueueFamily{0};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence computeFence{VK_NULL_HANDLE};

    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

    // Compute pipelines
    VkPipelineLayout broadPhasePipelineLayout{VK_NULL_HANDLE};
    VkPipeline broadPhasePipeline{VK_NULL_HANDLE};
    VkPipeline broadPhaseWritePipeline{VK_NULL_HANDLE};

    VkPipelineLayout prefixSumPipelineLayout{VK_NULL_HANDLE};
    VkPipeline prefixSumPipeline{VK_NULL_HANDLE};

    VkPipelineLayout sortPipelineLayout{VK_NULL_HANDLE};
    VkPipeline bitonicSortPipeline{VK_NULL_HANDLE};

    VkPipelineLayout pairGenPipelineLayout{VK_NULL_HANDLE};
    VkPipeline pairGenPipeline{VK_NULL_HANDLE};

    VkPipelineLayout narrowPhasePipelineLayout{VK_NULL_HANDLE};
    VkPipeline narrowPhasePipeline{VK_NULL_HANDLE};

    // Descriptor set layouts
    VkDescriptorSetLayout broadPhaseDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout prefixSumDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout sortDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout pairGenDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout narrowPhaseDescriptorLayout{VK_NULL_HANDLE};

    // Descriptor sets
    VkDescriptorSet broadPhaseDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet prefixSumDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet sortDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet pairGenDescriptorSet{VK_NULL_HANDLE};
    VkDescriptorSet narrowPhaseDescriptorSet{VK_NULL_HANDLE};

    // GPU buffers
    VkBuffer colliderBuffer{VK_NULL_HANDLE};
    VkDeviceMemory colliderMemory{VK_NULL_HANDLE};

    VkBuffer cellEntryBuffer{VK_NULL_HANDLE};
    VkDeviceMemory cellEntryMemory{VK_NULL_HANDLE};

    VkBuffer cellCountBuffer{VK_NULL_HANDLE};
    VkDeviceMemory cellCountMemory{VK_NULL_HANDLE};

    VkBuffer cellOffsetBuffer{VK_NULL_HANDLE};
    VkDeviceMemory cellOffsetMemory{VK_NULL_HANDLE};

    VkBuffer cellRangeBuffer{VK_NULL_HANDLE};
    VkDeviceMemory cellRangeMemory{VK_NULL_HANDLE};

    VkBuffer pairBuffer{VK_NULL_HANDLE};
    VkDeviceMemory pairMemory{VK_NULL_HANDLE};

    VkBuffer pairCountBuffer{VK_NULL_HANDLE};
    VkDeviceMemory pairCountMemory{VK_NULL_HANDLE};

    VkBuffer resultBuffer{VK_NULL_HANDLE};
    VkDeviceMemory resultMemory{VK_NULL_HANDLE};

    VkBuffer resultCountBuffer{VK_NULL_HANDLE};
    VkDeviceMemory resultCountMemory{VK_NULL_HANDLE};

    // Staging buffers for CPU-GPU transfers
    VkBuffer stagingBuffer{VK_NULL_HANDLE};
    VkDeviceMemory stagingMemory{VK_NULL_HANDLE};
    VkDeviceSize stagingBufferSize{0};

    // CPU-side data
    std::vector<GpuColliderAABB> cpuColliders;
    std::vector<std::uint32_t> objectIndexMap; // Maps GPU index to scene object index

    std::filesystem::path shaderDir;
};

} // namespace vkengine
