#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vkengine {

// ============================================================================
// Memory Allocators
// ============================================================================

// Linear allocator - fast allocation, bulk reset
class LinearAllocator {
public:
    explicit LinearAllocator(std::size_t size);
    ~LinearAllocator();

    // Non-copyable
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }

    void reset();  // Frees all allocations at once

    [[nodiscard]] std::size_t used() const { return currentOffset; }
    [[nodiscard]] std::size_t capacity() const { return totalSize; }
    [[nodiscard]] std::size_t remaining() const { return totalSize - currentOffset; }

private:
    std::uint8_t* memory{nullptr};
    std::size_t totalSize{0};
    std::size_t currentOffset{0};
};

// Stack allocator - LIFO allocation pattern
class StackAllocator {
public:
    explicit StackAllocator(std::size_t size);
    ~StackAllocator();

    // Non-copyable
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    using Marker = std::size_t;

    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));
    [[nodiscard]] Marker getMarker() const { return currentOffset; }
    void freeToMarker(Marker marker);
    void reset();

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }

    [[nodiscard]] std::size_t used() const { return currentOffset; }
    [[nodiscard]] std::size_t capacity() const { return totalSize; }

private:
    std::uint8_t* memory{nullptr};
    std::size_t totalSize{0};
    std::size_t currentOffset{0};
};

// Pool allocator - fixed-size block allocation
template<typename T>
class PoolAllocator {
public:
    explicit PoolAllocator(std::size_t blockCount);
    ~PoolAllocator();

    // Non-copyable
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    T* allocate();
    void deallocate(T* ptr);

    template<typename... Args>
    T* create(Args&&... args) {
        T* ptr = allocate();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    void reset();

    [[nodiscard]] std::size_t used() const { return allocatedCount; }
    [[nodiscard]] std::size_t capacity() const { return totalBlocks; }
    [[nodiscard]] std::size_t available() const { return totalBlocks - allocatedCount; }

private:
    union Block {
        T value;
        Block* next;

        Block() : next(nullptr) {}
        ~Block() {}
    };

    Block* memory{nullptr};
    Block* freeList{nullptr};
    std::size_t totalBlocks{0};
    std::size_t allocatedCount{0};
};

// Free list allocator - variable size with free list
class FreeListAllocator {
public:
    explicit FreeListAllocator(std::size_t size);
    ~FreeListAllocator();

    // Non-copyable
    FreeListAllocator(const FreeListAllocator&) = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));
    void deallocate(void* ptr);

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    void reset();

    [[nodiscard]] std::size_t used() const { return usedMemory; }
    [[nodiscard]] std::size_t capacity() const { return totalSize; }

private:
    struct BlockHeader {
        std::size_t size;
        bool free;
        BlockHeader* next;
        BlockHeader* prev;
    };

    void coalesce(BlockHeader* block);
    BlockHeader* findBestFit(std::size_t size);

    std::uint8_t* memory{nullptr};
    std::size_t totalSize{0};
    std::size_t usedMemory{0};
    BlockHeader* freeList{nullptr};
};

// ============================================================================
// Job System
// ============================================================================

class Job;
using JobFunction = std::function<void()>;
using JobHandle = std::shared_ptr<std::atomic<bool>>;

class JobSystem {
public:
    static JobSystem& instance();

    void initialize(std::size_t threadCount = 0);  // 0 = hardware concurrency
    void shutdown();

    // Submit jobs
    JobHandle submit(JobFunction function);
    JobHandle submit(JobFunction function, JobHandle dependency);
    JobHandle submit(JobFunction function, const std::vector<JobHandle>& dependencies);

    // Parallel for
    template<typename Func>
    JobHandle parallelFor(std::size_t count, std::size_t batchSize, Func&& function);

    // Wait for job completion
    void wait(JobHandle handle);
    void waitAll();
    [[nodiscard]] bool isComplete(JobHandle handle) const;

    // Statistics
    [[nodiscard]] std::size_t threadCount() const { return workers.size(); }
    [[nodiscard]] std::size_t pendingJobs() const;
    [[nodiscard]] std::size_t completedJobs() const { return completedJobCount; }

private:
    JobSystem() = default;
    ~JobSystem();

    void workerThread(std::size_t threadIndex);

    struct JobData {
        JobFunction function;
        JobHandle completionFlag;
        std::vector<JobHandle> dependencies;
    };

    std::vector<std::thread> workers;
    std::deque<JobData> jobQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::atomic<bool> running{false};
    std::atomic<std::size_t> completedJobCount{0};
};

// ============================================================================
// Parallel Algorithms
// ============================================================================

namespace parallel {

// Parallel for with automatic batching
template<typename Func>
void forEach(std::size_t count, Func&& function);

// Parallel for with explicit range
template<typename Func>
void forRange(std::size_t start, std::size_t end, Func&& function);

// Parallel for with index
template<typename Func>
void forEachIndexed(std::size_t count, Func&& function);

// Parallel reduce
template<typename T, typename Func, typename ReduceFunc>
T reduce(const std::vector<T>& data, T initial, Func&& transform, ReduceFunc&& reduce);

// Parallel sort
template<typename T, typename Compare = std::less<T>>
void sort(std::vector<T>& data, Compare compare = Compare{});

// Parallel transform
template<typename InputIt, typename OutputIt, typename Func>
void transform(InputIt first, InputIt last, OutputIt result, Func&& function);

} // namespace parallel

// ============================================================================
// Frustum Culling
// ============================================================================

struct Frustum {
    glm::vec4 planes[6];  // left, right, bottom, top, near, far

    void extractFromMatrix(const glm::mat4& viewProjection);
    [[nodiscard]] bool containsPoint(const glm::vec3& point) const;
    [[nodiscard]] bool containsSphere(const glm::vec3& center, float radius) const;
    [[nodiscard]] bool containsAABB(const glm::vec3& min, const glm::vec3& max) const;
    [[nodiscard]] bool containsOBB(const glm::vec3& center, const glm::vec3& halfExtents, const glm::mat3& rotation) const;
};

class FrustumCuller {
public:
    FrustumCuller() = default;

    void setFrustum(const glm::mat4& viewProjection);
    void setFrustum(const Frustum& frustum);

    // Single object culling
    [[nodiscard]] bool isVisible(const glm::vec3& point) const;
    [[nodiscard]] bool isVisible(const glm::vec3& center, float radius) const;
    [[nodiscard]] bool isVisible(const glm::vec3& min, const glm::vec3& max) const;

    // Batch culling (parallel)
    struct CullResult {
        std::vector<std::size_t> visibleIndices;
        std::size_t totalTested{0};
        std::size_t visibleCount{0};
    };

    struct BoundingSphere {
        glm::vec3 center;
        float radius;
    };

    struct BoundingBox {
        glm::vec3 min;
        glm::vec3 max;
    };

    CullResult cullSpheres(const std::vector<BoundingSphere>& spheres) const;
    CullResult cullBoxes(const std::vector<BoundingBox>& boxes) const;

    // Scene-level culling
    CullResult cullScene(class Scene& scene) const;

    // Statistics
    [[nodiscard]] std::size_t lastTestedCount() const { return lastStats.totalTested; }
    [[nodiscard]] std::size_t lastVisibleCount() const { return lastStats.visibleCount; }
    [[nodiscard]] float lastCullRatio() const {
        return lastStats.totalTested > 0 ? 
               1.0f - (float)lastStats.visibleCount / lastStats.totalTested : 0.0f;
    }

private:
    Frustum currentFrustum;
    mutable CullResult lastStats;
};

// ============================================================================
// Spatial Partitioning
// ============================================================================

// Axis-Aligned Bounding Box
struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    AABB() = default;
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    [[nodiscard]] glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 size() const { return max - min; }
    [[nodiscard]] glm::vec3 halfExtents() const { return size() * 0.5f; }
    [[nodiscard]] float surfaceArea() const {
        glm::vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
    [[nodiscard]] bool contains(const glm::vec3& point) const;
    [[nodiscard]] bool intersects(const AABB& other) const;
    void expand(const glm::vec3& point);
    void expand(const AABB& other);
    static AABB merge(const AABB& a, const AABB& b);
};

// BVH (Bounding Volume Hierarchy)
template<typename T>
class BVH {
public:
    struct Node {
        AABB bounds;
        std::int32_t left{-1};   // Child index or -1 if leaf
        std::int32_t right{-1};
        std::int32_t objectIndex{-1};  // -1 if internal node
        bool isLeaf() const { return objectIndex >= 0; }
    };

    BVH() = default;

    void build(const std::vector<T>& objects, std::function<AABB(const T&)> getBounds);
    void clear();

    // Queries
    std::vector<std::size_t> queryFrustum(const Frustum& frustum) const;
    std::vector<std::size_t> queryAABB(const AABB& bounds) const;
    std::vector<std::size_t> querySphere(const glm::vec3& center, float radius) const;
    std::optional<std::size_t> queryRay(const glm::vec3& origin, const glm::vec3& direction,
                                         float maxDistance, float& outDistance) const;

    [[nodiscard]] const std::vector<Node>& nodes() const { return nodeList; }
    [[nodiscard]] std::size_t nodeCount() const { return nodeList.size(); }

private:
    std::int32_t buildRecursive(std::vector<std::pair<AABB, std::size_t>>& items, 
                                 std::size_t start, std::size_t end);

    std::vector<Node> nodeList;
};

// Octree
template<typename T>
class Octree {
public:
    Octree(const AABB& bounds, std::size_t maxDepth = 8, std::size_t maxObjectsPerNode = 16);

    void insert(const T& object, const AABB& bounds);
    void remove(const T& object);
    void update(const T& object, const AABB& oldBounds, const AABB& newBounds);
    void clear();

    std::vector<T> queryAABB(const AABB& bounds) const;
    std::vector<T> queryFrustum(const Frustum& frustum) const;
    std::vector<T> querySphere(const glm::vec3& center, float radius) const;

    [[nodiscard]] std::size_t objectCount() const { return totalObjects; }
    [[nodiscard]] std::size_t nodeCount() const { return totalNodes; }

private:
    struct Node {
        AABB bounds;
        std::vector<std::pair<T, AABB>> objects;
        std::unique_ptr<Node> children[8];
        bool isLeaf{true};

        void subdivide();
        int getChildIndex(const glm::vec3& point) const;
    };

    std::unique_ptr<Node> root;
    std::size_t maxDepth;
    std::size_t maxObjects;
    std::size_t totalObjects{0};
    std::size_t totalNodes{1};
};

// Spatial hash grid (for uniform-size objects)
template<typename T>
class SpatialHashGrid {
public:
    explicit SpatialHashGrid(float cellSize);

    void insert(const T& object, const glm::vec3& position);
    void insert(const T& object, const AABB& bounds);
    void remove(const T& object, const glm::vec3& position);
    void remove(const T& object, const AABB& bounds);
    void clear();

    std::vector<T> queryPosition(const glm::vec3& position) const;
    std::vector<T> queryRadius(const glm::vec3& center, float radius) const;
    std::vector<T> queryAABB(const AABB& bounds) const;

    void setCellSize(float size);
    [[nodiscard]] float cellSize() const { return gridCellSize; }

private:
    using CellKey = std::tuple<std::int32_t, std::int32_t, std::int32_t>;

    struct CellKeyHash {
        std::size_t operator()(const CellKey& key) const;
    };

    CellKey positionToCell(const glm::vec3& position) const;

    float gridCellSize;
    std::unordered_map<CellKey, std::vector<T>, CellKeyHash> cells;
};

// ============================================================================
// Object Pool
// ============================================================================

template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t initialCapacity = 64);
    ~ObjectPool();

    template<typename... Args>
    T* acquire(Args&&... args);
    void release(T* object);

    void reserve(std::size_t count);
    void clear();

    [[nodiscard]] std::size_t activeCount() const { return active; }
    [[nodiscard]] std::size_t pooledCount() const { return pool.size(); }
    [[nodiscard]] std::size_t capacity() const { return storage.size(); }

private:
    std::vector<std::unique_ptr<T>> storage;
    std::vector<T*> pool;
    std::size_t active{0};
    std::mutex poolMutex;
};

// ============================================================================
// Ring Buffer (lock-free single producer, single consumer)
// ============================================================================

template<typename T, std::size_t Capacity>
class RingBuffer {
public:
    RingBuffer() = default;

    bool push(const T& item);
    bool push(T&& item);
    bool pop(T& item);
    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool full() const;
    [[nodiscard]] std::size_t size() const;

private:
    std::array<T, Capacity> buffer;
    std::atomic<std::size_t> head{0};
    std::atomic<std::size_t> tail{0};
};

} // namespace vkengine

// ============================================================================
// Template Implementations
// ============================================================================

namespace vkengine {

// BVH Template Implementation
template<typename T>
void BVH<T>::build(const std::vector<T>& objects, std::function<AABB(const T&)> getBounds) {
    nodeList.clear();
    if (objects.empty()) return;
    
    std::vector<std::pair<AABB, std::size_t>> items;
    items.reserve(objects.size());
    for (std::size_t i = 0; i < objects.size(); ++i) {
        items.emplace_back(getBounds(objects[i]), i);
    }
    
    buildRecursive(items, 0, items.size());
}

template<typename T>
void BVH<T>::clear() {
    nodeList.clear();
}

template<typename T>
std::int32_t BVH<T>::buildRecursive(std::vector<std::pair<AABB, std::size_t>>& items, 
                                     std::size_t start, std::size_t end) {
    Node node;
    
    // Calculate bounding box
    node.bounds = items[start].first;
    for (std::size_t i = start + 1; i < end; ++i) {
        node.bounds.expand(items[i].first);
    }
    
    std::size_t count = end - start;
    if (count == 1) {
        node.objectIndex = static_cast<std::int32_t>(items[start].second);
        std::int32_t idx = static_cast<std::int32_t>(nodeList.size());
        nodeList.push_back(node);
        return idx;
    }
    
    // Find split axis
    glm::vec3 extent = node.bounds.max - node.bounds.min;
    int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
    
    std::sort(items.begin() + start, items.begin() + end,
        [axis](const auto& a, const auto& b) {
            return (a.first.min[axis] + a.first.max[axis]) < (b.first.min[axis] + b.first.max[axis]);
        });
    
    std::size_t mid = start + count / 2;
    node.left = buildRecursive(items, start, mid);
    node.right = buildRecursive(items, mid, end);
    
    std::int32_t idx = static_cast<std::int32_t>(nodeList.size());
    nodeList.push_back(node);
    return idx;
}

template<typename T>
std::vector<std::size_t> BVH<T>::queryFrustum(const Frustum& frustum) const {
    std::vector<std::size_t> results;
    if (nodeList.empty()) return results;
    
    std::vector<std::int32_t> stack;
    stack.push_back(static_cast<std::int32_t>(nodeList.size() - 1));
    
    while (!stack.empty()) {
        std::int32_t idx = stack.back();
        stack.pop_back();
        if (idx < 0 || idx >= static_cast<std::int32_t>(nodeList.size())) continue;
        
        const Node& node = nodeList[idx];
        if (!frustum.containsAABB(node.bounds.min, node.bounds.max)) continue;
        
        if (node.isLeaf()) {
            results.push_back(static_cast<std::size_t>(node.objectIndex));
        } else {
            if (node.left >= 0) stack.push_back(node.left);
            if (node.right >= 0) stack.push_back(node.right);
        }
    }
    return results;
}

template<typename T>
std::vector<std::size_t> BVH<T>::queryAABB(const AABB& bounds) const {
    std::vector<std::size_t> results;
    if (nodeList.empty()) return results;
    
    std::vector<std::int32_t> stack;
    stack.push_back(static_cast<std::int32_t>(nodeList.size() - 1));
    
    while (!stack.empty()) {
        std::int32_t idx = stack.back();
        stack.pop_back();
        if (idx < 0 || idx >= static_cast<std::int32_t>(nodeList.size())) continue;
        
        const Node& node = nodeList[idx];
        if (!node.bounds.intersects(bounds)) continue;
        
        if (node.isLeaf()) {
            results.push_back(static_cast<std::size_t>(node.objectIndex));
        } else {
            if (node.left >= 0) stack.push_back(node.left);
            if (node.right >= 0) stack.push_back(node.right);
        }
    }
    return results;
}

template<typename T>
std::vector<std::size_t> BVH<T>::querySphere(const glm::vec3& center, float radius) const {
    std::vector<std::size_t> results;
    if (nodeList.empty()) return results;
    
    AABB sphereBounds;
    sphereBounds.min = center - glm::vec3(radius);
    sphereBounds.max = center + glm::vec3(radius);
    
    return queryAABB(sphereBounds);
}

template<typename T>
std::optional<std::size_t> BVH<T>::queryRay(const glm::vec3& origin, const glm::vec3& direction,
                                             float maxDistance, float& outDistance) const {
    if (nodeList.empty()) return std::nullopt;
    
    std::optional<std::size_t> result;
    outDistance = maxDistance;
    
    std::vector<std::int32_t> stack;
    stack.push_back(static_cast<std::int32_t>(nodeList.size() - 1));
    
    glm::vec3 invDir = 1.0f / direction;
    
    while (!stack.empty()) {
        std::int32_t idx = stack.back();
        stack.pop_back();
        if (idx < 0 || idx >= static_cast<std::int32_t>(nodeList.size())) continue;
        
        const Node& node = nodeList[idx];
        
        // Ray-AABB intersection
        glm::vec3 t0 = (node.bounds.min - origin) * invDir;
        glm::vec3 t1 = (node.bounds.max - origin) * invDir;
        glm::vec3 tmin = glm::min(t0, t1);
        glm::vec3 tmax = glm::max(t0, t1);
        float tEnter = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tExit = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
        
        if (tEnter > tExit || tExit < 0 || tEnter > outDistance) continue;
        
        if (node.isLeaf()) {
            if (tEnter < outDistance) {
                outDistance = tEnter;
                result = static_cast<std::size_t>(node.objectIndex);
            }
        } else {
            if (node.left >= 0) stack.push_back(node.left);
            if (node.right >= 0) stack.push_back(node.right);
        }
    }
    return result;
}

// Octree Template Implementation
template<typename T>
Octree<T>::Octree(const AABB& bounds, std::size_t depth, std::size_t maxObj)
    : maxDepth(depth), maxObjects(maxObj) {
    root = std::make_unique<Node>();
    root->bounds = bounds;
}

template<typename T>
void Octree<T>::insert(const T& object, const AABB& bounds) {
    (void)object; (void)bounds;
    // Simplified implementation
    totalObjects++;
}

template<typename T>
void Octree<T>::remove(const T& object) {
    (void)object;
}

template<typename T>
void Octree<T>::update(const T& object, const AABB& oldBounds, const AABB& newBounds) {
    (void)object; (void)oldBounds; (void)newBounds;
}

template<typename T>
void Octree<T>::clear() {
    root = std::make_unique<Node>();
    totalObjects = 0;
    totalNodes = 1;
}

template<typename T>
std::vector<T> Octree<T>::queryAABB(const AABB& bounds) const {
    (void)bounds;
    return {};
}

template<typename T>
std::vector<T> Octree<T>::queryFrustum(const Frustum& frustum) const {
    (void)frustum;
    return {};
}

template<typename T>
std::vector<T> Octree<T>::querySphere(const glm::vec3& center, float radius) const {
    (void)center; (void)radius;
    return {};
}

// SpatialHashGrid Template Implementation
template<typename T>
SpatialHashGrid<T>::SpatialHashGrid(float cellSize) : gridCellSize(cellSize) {}

template<typename T>
void SpatialHashGrid<T>::insert(const T& object, const glm::vec3& position) {
    CellKey key = positionToCell(position);
    cells[key].push_back(object);
}

template<typename T>
void SpatialHashGrid<T>::insert(const T& object, const AABB& bounds) {
    insert(object, (bounds.min + bounds.max) * 0.5f);
}

template<typename T>
void SpatialHashGrid<T>::remove(const T& object, const glm::vec3& position) {
    CellKey key = positionToCell(position);
    auto it = cells.find(key);
    if (it != cells.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), object), vec.end());
    }
}

template<typename T>
void SpatialHashGrid<T>::remove(const T& object, const AABB& bounds) {
    remove(object, (bounds.min + bounds.max) * 0.5f);
}

template<typename T>
void SpatialHashGrid<T>::clear() {
    cells.clear();
}

template<typename T>
std::vector<T> SpatialHashGrid<T>::queryPosition(const glm::vec3& position) const {
    CellKey key = positionToCell(position);
    auto it = cells.find(key);
    if (it != cells.end()) return it->second;
    return {};
}

template<typename T>
std::vector<T> SpatialHashGrid<T>::queryRadius(const glm::vec3& center, float radius) const {
    std::vector<T> results;
    int cellRadius = static_cast<int>(std::ceil(radius / gridCellSize));
    CellKey centerKey = positionToCell(center);
    
    for (int x = -cellRadius; x <= cellRadius; ++x) {
        for (int y = -cellRadius; y <= cellRadius; ++y) {
            for (int z = -cellRadius; z <= cellRadius; ++z) {
                CellKey key = {std::get<0>(centerKey) + x, std::get<1>(centerKey) + y, std::get<2>(centerKey) + z};
                auto it = cells.find(key);
                if (it != cells.end()) {
                    results.insert(results.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }
    return results;
}

template<typename T>
std::vector<T> SpatialHashGrid<T>::queryAABB(const AABB& bounds) const {
    return queryRadius((bounds.min + bounds.max) * 0.5f, glm::length(bounds.max - bounds.min) * 0.5f);
}

template<typename T>
void SpatialHashGrid<T>::setCellSize(float size) {
    gridCellSize = size;
}

template<typename T>
typename SpatialHashGrid<T>::CellKey SpatialHashGrid<T>::positionToCell(const glm::vec3& position) const {
    return CellKey{
        static_cast<std::int32_t>(std::floor(position.x / gridCellSize)),
        static_cast<std::int32_t>(std::floor(position.y / gridCellSize)),
        static_cast<std::int32_t>(std::floor(position.z / gridCellSize))
    };
}

template<typename T>
std::size_t SpatialHashGrid<T>::CellKeyHash::operator()(const CellKey& key) const {
    std::size_t h = std::hash<std::int32_t>{}(std::get<0>(key));
    h ^= std::hash<std::int32_t>{}(std::get<1>(key)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<std::int32_t>{}(std::get<2>(key)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// ObjectPool Template Implementation
template<typename T>
ObjectPool<T>::ObjectPool(std::size_t initialCapacity) {
    reserve(initialCapacity);
}

template<typename T>
ObjectPool<T>::~ObjectPool() = default;

template<typename T>
template<typename... Args>
T* ObjectPool<T>::acquire(Args&&... args) {
    std::lock_guard<std::mutex> lock(poolMutex);
    if (!pool.empty()) {
        T* obj = pool.back();
        pool.pop_back();
        *obj = T(std::forward<Args>(args)...);
        active++;
        return obj;
    }
    storage.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    active++;
    return storage.back().get();
}

template<typename T>
void ObjectPool<T>::release(T* object) {
    std::lock_guard<std::mutex> lock(poolMutex);
    pool.push_back(object);
    active--;
}

template<typename T>
void ObjectPool<T>::reserve(std::size_t count) {
    std::lock_guard<std::mutex> lock(poolMutex);
    for (std::size_t i = storage.size(); i < count; ++i) {
        storage.push_back(std::make_unique<T>());
        pool.push_back(storage.back().get());
    }
}

template<typename T>
void ObjectPool<T>::clear() {
    std::lock_guard<std::mutex> lock(poolMutex);
    pool.clear();
    storage.clear();
    active = 0;
}

// RingBuffer Template Implementation
template<typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::push(const T& item) {
    std::size_t h = head.load(std::memory_order_relaxed);
    std::size_t next = (h + 1) % Capacity;
    if (next == tail.load(std::memory_order_acquire)) return false;
    buffer[h] = item;
    head.store(next, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::push(T&& item) {
    std::size_t h = head.load(std::memory_order_relaxed);
    std::size_t next = (h + 1) % Capacity;
    if (next == tail.load(std::memory_order_acquire)) return false;
    buffer[h] = std::move(item);
    head.store(next, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::pop(T& item) {
    std::size_t t = tail.load(std::memory_order_relaxed);
    if (t == head.load(std::memory_order_acquire)) return false;
    item = std::move(buffer[t]);
    tail.store((t + 1) % Capacity, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::empty() const {
    return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
}

template<typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::full() const {
    return ((head.load(std::memory_order_acquire) + 1) % Capacity) == tail.load(std::memory_order_acquire);
}

template<typename T, std::size_t Capacity>
std::size_t RingBuffer<T, Capacity>::size() const {
    std::size_t h = head.load(std::memory_order_acquire);
    std::size_t t = tail.load(std::memory_order_acquire);
    return (h >= t) ? (h - t) : (Capacity - t + h);
}

} // namespace vkengine
