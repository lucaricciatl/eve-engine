#include "engine/JobSystem.hpp"

#include <algorithm>
#include <chrono>

namespace vkengine {

// ============================================================================
// Linear Allocator Implementation
// ============================================================================

LinearAllocator::LinearAllocator(std::size_t size)
    : memory(new std::uint8_t[size])
    , totalSize(size)
    , currentOffset(0)
{
}

LinearAllocator::~LinearAllocator() {
    delete[] memory;
}

void* LinearAllocator::allocate(std::size_t size, std::size_t alignment) {
    std::size_t alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
    if (alignedOffset + size > totalSize) {
        return nullptr; // Out of memory
    }
    void* ptr = memory + alignedOffset;
    currentOffset = alignedOffset + size;
    return ptr;
}

void LinearAllocator::reset() {
    currentOffset = 0;
}

// ============================================================================
// Stack Allocator Implementation
// ============================================================================

StackAllocator::StackAllocator(std::size_t size)
    : memory(new std::uint8_t[size])
    , totalSize(size)
    , currentOffset(0)
{
}

StackAllocator::~StackAllocator() {
    delete[] memory;
}

void* StackAllocator::allocate(std::size_t size, std::size_t alignment) {
    std::size_t alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
    if (alignedOffset + size > totalSize) {
        return nullptr;
    }
    void* ptr = memory + alignedOffset;
    currentOffset = alignedOffset + size;
    return ptr;
}

void StackAllocator::freeToMarker(Marker marker) {
    currentOffset = marker;
}

void StackAllocator::reset() {
    currentOffset = 0;
}

// ============================================================================
// Pool Allocator Implementation - Template class, defined in header
// Explicit instantiations for common types if needed go here
// ============================================================================

// ============================================================================
// Free List Allocator Implementation
// ============================================================================

FreeListAllocator::FreeListAllocator(std::size_t size)
    : memory(new std::uint8_t[size])
    , totalSize(size)
    , usedMemory(0)
{
    // Initialize with single free block
    freeList = reinterpret_cast<BlockHeader*>(memory);
    freeList->size = size;
    freeList->free = true;
    freeList->next = nullptr;
    freeList->prev = nullptr;
}

FreeListAllocator::~FreeListAllocator() {
    delete[] memory;
}

FreeListAllocator::BlockHeader* FreeListAllocator::findBestFit(std::size_t size) {
    BlockHeader* bestFit = nullptr;
    BlockHeader* current = freeList;
    
    while (current) {
        if (current->free && current->size >= size) {
            if (!bestFit || current->size < bestFit->size) {
                bestFit = current;
            }
        }
        current = current->next;
    }
    
    return bestFit;
}

void FreeListAllocator::coalesce(BlockHeader* block) {
    if (!block || !block->free) return;
    
    // Try to merge with next block
    if (block->next && block->next->free) {
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    
    // Try to merge with previous block
    if (block->prev && block->prev->free) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void* FreeListAllocator::allocate(std::size_t size, std::size_t alignment) {
    std::size_t totalNeeded = size + sizeof(BlockHeader);
    totalNeeded = (totalNeeded + alignment - 1) & ~(alignment - 1);

    BlockHeader* block = findBestFit(totalNeeded);
    if (!block) {
        return nullptr; // No suitable block found
    }

    std::size_t remaining = block->size - totalNeeded;

    if (remaining >= sizeof(BlockHeader) + 16) {
        // Split block
        BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(
            reinterpret_cast<std::uint8_t*>(block) + totalNeeded);
        newBlock->size = remaining;
        newBlock->free = true;
        newBlock->next = block->next;
        newBlock->prev = block;
        
        if (block->next) {
            block->next->prev = newBlock;
        }
        block->next = newBlock;
        block->size = totalNeeded;
    }

    block->free = false;
    usedMemory += block->size;

    return reinterpret_cast<std::uint8_t*>(block) + sizeof(BlockHeader);
}

void FreeListAllocator::deallocate(void* ptr) {
    if (!ptr) return;

    BlockHeader* block = reinterpret_cast<BlockHeader*>(
        reinterpret_cast<std::uint8_t*>(ptr) - sizeof(BlockHeader));

    block->free = true;
    usedMemory -= block->size;
    
    coalesce(block);
}

void FreeListAllocator::reset() {
    freeList = reinterpret_cast<BlockHeader*>(memory);
    freeList->size = totalSize;
    freeList->free = true;
    freeList->next = nullptr;
    freeList->prev = nullptr;
    usedMemory = 0;
}

// ============================================================================
// Job System Implementation
// ============================================================================

JobSystem& JobSystem::instance() {
    static JobSystem inst;
    return inst;
}

JobSystem::~JobSystem() {
    shutdown();
}

void JobSystem::initialize(std::size_t threadCount) {
    if (running.load()) {
        return; // Already initialized
    }
    
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 4; // Fallback
    }
    
    running = true;
    
    workers.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back(&JobSystem::workerThread, this, i);
    }
}

void JobSystem::shutdown() {
    if (!running.load()) {
        return;
    }
    
    running = false;
    queueCondition.notify_all();
    
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers.clear();
}

JobHandle JobSystem::submit(JobFunction function) {
    auto handle = std::make_shared<std::atomic<bool>>(false);
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        jobQueue.push_back({std::move(function), handle, {}});
    }
    
    queueCondition.notify_one();
    return handle;
}

JobHandle JobSystem::submit(JobFunction function, JobHandle dependency) {
    auto handle = std::make_shared<std::atomic<bool>>(false);
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        jobQueue.push_back({std::move(function), handle, {dependency}});
    }
    
    queueCondition.notify_one();
    return handle;
}

JobHandle JobSystem::submit(JobFunction function, const std::vector<JobHandle>& dependencies) {
    auto handle = std::make_shared<std::atomic<bool>>(false);
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        jobQueue.push_back({std::move(function), handle, dependencies});
    }
    
    queueCondition.notify_one();
    return handle;
}

void JobSystem::wait(JobHandle handle) {
    while (handle && !handle->load()) {
        std::this_thread::yield();
    }
}

void JobSystem::waitAll() {
    while (pendingJobs() > 0) {
        std::this_thread::yield();
    }
}

bool JobSystem::isComplete(JobHandle handle) const {
    return !handle || handle->load();
}

std::size_t JobSystem::pendingJobs() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
    return jobQueue.size();
}

void JobSystem::workerThread(std::size_t /*threadIndex*/) {
    while (running.load()) {
        JobData job;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [this] {
                return !running.load() || !jobQueue.empty();
            });
            
            if (!running.load() && jobQueue.empty()) {
                return;
            }
            
            // Find a job with satisfied dependencies
            auto it = jobQueue.begin();
            while (it != jobQueue.end()) {
                bool dependenciesMet = true;
                for (const auto& dep : it->dependencies) {
                    if (dep && !dep->load()) {
                        dependenciesMet = false;
                        break;
                    }
                }
                
                if (dependenciesMet) {
                    job = std::move(*it);
                    jobQueue.erase(it);
                    break;
                }
                ++it;
            }
            
            if (!job.function) {
                continue; // No ready job found
            }
        }
        
        // Execute the job
        if (job.function) {
            job.function();
            if (job.completionFlag) {
                job.completionFlag->store(true);
            }
            ++completedJobCount;
        }
    }
}

// ============================================================================
// Frustum Implementation
// ============================================================================

void Frustum::extractFromMatrix(const glm::mat4& vp) {
    // Left plane
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], 
                          vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right plane
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], 
                          vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom plane
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], 
                          vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top plane
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], 
                          vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near plane
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], 
                          vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far plane
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], 
                          vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    
    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float length = glm::length(glm::vec3(planes[i]));
        if (length > 0.0f) {
            planes[i] /= length;
        }
    }
}

bool Frustum::containsPoint(const glm::vec3& point) const {
    for (const auto& plane : planes) {
        if (glm::dot(glm::vec3(plane), point) + plane.w < 0) {
            return false;
        }
    }
    return true;
}

bool Frustum::containsSphere(const glm::vec3& center, float radius) const {
    for (const auto& plane : planes) {
        float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool Frustum::containsAABB(const glm::vec3& min, const glm::vec3& max) const {
    for (const auto& plane : planes) {
        glm::vec3 p;
        p.x = plane.x > 0 ? max.x : min.x;
        p.y = plane.y > 0 ? max.y : min.y;
        p.z = plane.z > 0 ? max.z : min.z;
        
        if (glm::dot(glm::vec3(plane), p) + plane.w < 0) {
            return false;
        }
    }
    return true;
}

// Note: BVH, Octree, SpatialHashGrid, ObjectPool, and RingBuffer are template classes
// Their implementations must be in the header file (JobSystem.hpp)

} // namespace vkengine
