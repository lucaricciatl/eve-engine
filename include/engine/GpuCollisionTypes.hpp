#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace vkengine {

// GPU-compatible AABB structure for collision detection
// Matches the layout in collision compute shaders
struct alignas(16) GpuColliderAABB {
    glm::vec4 minPos{0.0f};  // xyz = min bounds, w = object index (as float)
    glm::vec4 maxPos{0.0f};  // xyz = max bounds, w = isStatic flag (1.0 or 0.0)
};

static_assert(sizeof(GpuColliderAABB) == 32, "GpuColliderAABB size mismatch");

// Cell entry for spatial hashing
struct alignas(8) GpuCellEntry {
    std::uint32_t objectIndex{0};
    std::uint32_t cellHash{0};
};

static_assert(sizeof(GpuCellEntry) == 8, "GpuCellEntry size mismatch");

// Collision pair from broad phase
struct alignas(16) GpuCollisionPair {
    std::uint32_t objectA{0};
    std::uint32_t objectB{0};
    std::uint32_t padding0{0};
    std::uint32_t padding1{0};
};

static_assert(sizeof(GpuCollisionPair) == 16, "GpuCollisionPair size mismatch");

// Detailed collision result from narrow phase
struct alignas(16) GpuCollisionResult {
    std::uint32_t objectA{0};
    std::uint32_t objectB{0};
    std::uint32_t padding0{0};
    std::uint32_t padding1{0};
    glm::vec4 normal{0.0f};        // xyz = normal (A to B), w = penetration depth
    glm::vec4 contactPoint{0.0f};  // xyz = contact point, w = validity (1.0 = valid)
};

static_assert(sizeof(GpuCollisionResult) == 48, "GpuCollisionResult size mismatch");

// Push constants for broad phase shader
struct alignas(16) BroadPhasePushConstants {
    float cellSize{1.0f};
    float invCellSize{1.0f};
    std::uint32_t objectCount{0};
    std::uint32_t gridResolution{256};
    glm::vec4 worldMin{-100.0f, -100.0f, -100.0f, 0.0f};
    glm::vec4 worldMax{100.0f, 100.0f, 100.0f, 0.0f};
};

static_assert(sizeof(BroadPhasePushConstants) == 48, "BroadPhasePushConstants size mismatch");

// Push constants for narrow phase shader
struct alignas(16) NarrowPhasePushConstants {
    std::uint32_t pairCount{0};
    std::uint32_t maxResults{0};
    float penetrationSlop{0.0005f};
    float padding{0.0f};
};

static_assert(sizeof(NarrowPhasePushConstants) == 16, "NarrowPhasePushConstants size mismatch");

// Push constants for pair generation shader
struct alignas(16) PairGenPushConstants {
    std::uint32_t totalEntries{0};
    std::uint32_t maxPairs{0};
    std::uint32_t uniqueCellCount{0};
    std::uint32_t padding{0};
};

static_assert(sizeof(PairGenPushConstants) == 16, "PairGenPushConstants size mismatch");

// Push constants for prefix sum shader
struct alignas(16) PrefixSumPushConstants {
    std::uint32_t elementCount{0};
    std::uint32_t stride{1};
    std::uint32_t passIndex{0};
    std::uint32_t padding{0};
};

static_assert(sizeof(PrefixSumPushConstants) == 16, "PrefixSumPushConstants size mismatch");

// Push constants for bitonic sort shader
struct alignas(16) BitonicSortPushConstants {
    std::uint32_t elementCount{0};
    std::uint32_t stage{0};
    std::uint32_t passIndex{0};
    std::uint32_t ascending{1};
};

static_assert(sizeof(BitonicSortPushConstants) == 16, "BitonicSortPushConstants size mismatch");

// Configuration for GPU collision detection
struct GpuCollisionConfig {
    float cellSize{2.0f};           // Spatial hash cell size (should be ~2x largest object)
    std::uint32_t gridResolution{256}; // Must be power of 2
    std::uint32_t maxObjects{4096};
    std::uint32_t maxPairs{65536};
    std::uint32_t maxResults{16384};
    glm::vec3 worldMin{-100.0f};
    glm::vec3 worldMax{100.0f};
    float penetrationSlop{0.0005f};
    bool enabled{true};
};

} // namespace vkengine
