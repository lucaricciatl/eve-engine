#include "engine/DebugTools.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace vkengine {

// ============================================================================
// DebugDraw Implementation
// ============================================================================

DebugDraw& DebugDraw::instance() {
    static DebugDraw draw;
    return draw;
}

void DebugDraw::line(const glm::vec3& start, const glm::vec3& end, 
                     const glm::vec4& color, float duration, bool depthTest) {
    if (!debugEnabled) return;
    DebugLine l;
    l.start = start;
    l.end = end;
    l.color = color;
    l.duration = duration;
    l.depthTest = depthTest;
    debugLines.push_back(l);
}

void DebugDraw::ray(const glm::vec3& origin, const glm::vec3& direction, float length,
                    const glm::vec4& color, float duration, bool depthTest) {
    line(origin, origin + direction * length, color, duration, depthTest);
}

void DebugDraw::sphere(const glm::vec3& center, float radius, const glm::vec4& color,
                       int segments, float duration, bool depthTest) {
    DebugSphere s;
    s.center = center;
    s.radius = radius;
    s.color = color;
    s.segments = segments;
    s.duration = duration;
    s.depthTest = depthTest;
    s.wireframe = false;
    debugSpheres.push_back(s);
}

void DebugDraw::wireSphere(const glm::vec3& center, float radius, const glm::vec4& color,
                           int segments, float duration, bool depthTest) {
    DebugSphere s;
    s.center = center;
    s.radius = radius;
    s.color = color;
    s.segments = segments;
    s.duration = duration;
    s.depthTest = depthTest;
    s.wireframe = true;
    debugSpheres.push_back(s);
}

void DebugDraw::box(const glm::vec3& center, const glm::vec3& halfExtents, 
                    const glm::vec4& color, float duration, bool depthTest) {
    DebugBox b;
    b.center = center;
    b.halfExtents = halfExtents;
    b.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    b.color = color;
    b.duration = duration;
    b.depthTest = depthTest;
    b.wireframe = false;
    debugBoxes.push_back(b);
}

void DebugDraw::box(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation,
                    const glm::vec4& color, float duration, bool depthTest) {
    DebugBox b;
    b.center = center;
    b.halfExtents = halfExtents;
    b.rotation = rotation;
    b.color = color;
    b.duration = duration;
    b.depthTest = depthTest;
    b.wireframe = false;
    debugBoxes.push_back(b);
}

void DebugDraw::wireBox(const glm::vec3& center, const glm::vec3& halfExtents, 
                        const glm::vec4& color, float duration, bool depthTest) {
    DebugBox b;
    b.center = center;
    b.halfExtents = halfExtents;
    b.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    b.color = color;
    b.duration = duration;
    b.depthTest = depthTest;
    b.wireframe = true;
    debugBoxes.push_back(b);
}

void DebugDraw::wireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation,
                        const glm::vec4& color, float duration, bool depthTest) {
    DebugBox b;
    b.center = center;
    b.halfExtents = halfExtents;
    b.rotation = rotation;
    b.color = color;
    b.duration = duration;
    b.depthTest = depthTest;
    b.wireframe = true;
    debugBoxes.push_back(b);
}

void DebugDraw::capsule(const glm::vec3& point1, const glm::vec3& point2, float radius,
                        const glm::vec4& color, int segments, float duration, bool depthTest) {
    DebugCapsule c;
    c.point1 = point1;
    c.point2 = point2;
    c.radius = radius;
    c.color = color;
    c.segments = segments;
    c.duration = duration;
    c.depthTest = depthTest;
    debugCapsules.push_back(c);
}

void DebugDraw::cylinder(const glm::vec3& point1, const glm::vec3& point2, float radius,
                         const glm::vec4& color, int segments, float duration, bool depthTest) {
    const float PI = 3.14159265359f;
    glm::vec3 axis = point2 - point1;
    float length = glm::length(axis);
    if (length < 0.0001f) return;
    
    glm::vec3 dir = axis / length;
    glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
    if (glm::length(right) < 0.001f) {
        right = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
    }
    glm::vec3 up = glm::normalize(glm::cross(right, dir));
    
    for (int i = 0; i < segments; ++i) {
        float angle1 = (float(i) / segments) * 2.0f * PI;
        float angle2 = (float(i + 1) / segments) * 2.0f * PI;
        
        glm::vec3 offset1 = (right * std::cos(angle1) + up * std::sin(angle1)) * radius;
        glm::vec3 offset2 = (right * std::cos(angle2) + up * std::sin(angle2)) * radius;
        
        line(point1 + offset1, point1 + offset2, color, duration, depthTest);
        line(point2 + offset1, point2 + offset2, color, duration, depthTest);
        line(point1 + offset1, point2 + offset1, color, duration, depthTest);
    }
}

void DebugDraw::cone(const glm::vec3& apex, const glm::vec3& base, float radius,
                     const glm::vec4& color, int segments, float duration, bool depthTest) {
    const float PI = 3.14159265359f;
    glm::vec3 axis = base - apex;
    float length = glm::length(axis);
    if (length < 0.0001f) return;
    
    glm::vec3 dir = axis / length;
    glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
    if (glm::length(right) < 0.001f) {
        right = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
    }
    glm::vec3 up = glm::normalize(glm::cross(right, dir));
    
    for (int i = 0; i < segments; ++i) {
        float angle1 = (float(i) / segments) * 2.0f * PI;
        float angle2 = (float(i + 1) / segments) * 2.0f * PI;
        
        glm::vec3 offset1 = (right * std::cos(angle1) + up * std::sin(angle1)) * radius;
        glm::vec3 offset2 = (right * std::cos(angle2) + up * std::sin(angle2)) * radius;
        
        line(base + offset1, base + offset2, color, duration, depthTest);
        line(apex, base + offset1, color, duration, depthTest);
    }
}

void DebugDraw::arrow(const glm::vec3& start, const glm::vec3& end, float headSize,
                      const glm::vec4& color, float duration, bool depthTest) {
    DebugArrow a;
    a.start = start;
    a.end = end;
    a.headSize = headSize;
    a.color = color;
    a.duration = duration;
    a.depthTest = depthTest;
    debugArrows.push_back(a);
    
    line(start, end, color, duration, depthTest);
    
    glm::vec3 dir = glm::normalize(end - start);
    glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
    if (glm::length(right) < 0.001f) {
        right = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
    }
    glm::vec3 up = glm::normalize(glm::cross(right, dir));
    
    glm::vec3 arrowBase = end - dir * headSize;
    line(end, arrowBase + right * headSize * 0.5f, color, duration, depthTest);
    line(end, arrowBase - right * headSize * 0.5f, color, duration, depthTest);
    line(end, arrowBase + up * headSize * 0.5f, color, duration, depthTest);
    line(end, arrowBase - up * headSize * 0.5f, color, duration, depthTest);
}

void DebugDraw::axes(const glm::vec3& position, const glm::quat& rotation, float size,
                     float duration, bool depthTest) {
    glm::vec3 right = rotation * glm::vec3(1, 0, 0);
    glm::vec3 up = rotation * glm::vec3(0, 1, 0);
    glm::vec3 forward = rotation * glm::vec3(0, 0, 1);
    
    arrow(position, position + right * size, size * 0.1f, glm::vec4(1, 0, 0, 1), duration, depthTest);
    arrow(position, position + up * size, size * 0.1f, glm::vec4(0, 1, 0, 1), duration, depthTest);
    arrow(position, position + forward * size, size * 0.1f, glm::vec4(0, 0, 1, 1), duration, depthTest);
}

void DebugDraw::grid(const glm::vec3& center, const glm::vec3& normal, float size, int divisions,
                     const glm::vec4& color, float duration, bool depthTest) {
    glm::vec3 right = glm::normalize(glm::cross(normal, glm::vec3(0, 0, 1)));
    if (glm::length(right) < 0.001f) {
        right = glm::normalize(glm::cross(normal, glm::vec3(1, 0, 0)));
    }
    glm::vec3 forward = glm::normalize(glm::cross(right, normal));
    
    float step = size / divisions;
    float halfSize = size * 0.5f;
    
    for (int i = 0; i <= divisions; ++i) {
        float offset = -halfSize + i * step;
        line(center + forward * offset - right * halfSize,
             center + forward * offset + right * halfSize, color, duration, depthTest);
        line(center + right * offset - forward * halfSize,
             center + right * offset + forward * halfSize, color, duration, depthTest);
    }
}

void DebugDraw::circle(const glm::vec3& center, const glm::vec3& normal, float radius,
                       const glm::vec4& color, int segments, float duration, bool depthTest) {
    const float PI = 3.14159265359f;
    glm::vec3 right = glm::normalize(glm::cross(normal, glm::vec3(0, 0, 1)));
    if (glm::length(right) < 0.001f) {
        right = glm::normalize(glm::cross(normal, glm::vec3(1, 0, 0)));
    }
    glm::vec3 up = glm::normalize(glm::cross(right, normal));
    
    for (int i = 0; i < segments; ++i) {
        float angle1 = (float(i) / segments) * 2.0f * PI;
        float angle2 = (float(i + 1) / segments) * 2.0f * PI;
        
        glm::vec3 p1 = center + (right * std::cos(angle1) + up * std::sin(angle1)) * radius;
        glm::vec3 p2 = center + (right * std::cos(angle2) + up * std::sin(angle2)) * radius;
        line(p1, p2, color, duration, depthTest);
    }
}

void DebugDraw::arc(const glm::vec3& center, const glm::vec3& normal, const glm::vec3& from,
                    float angle, float radius, const glm::vec4& color, int segments, 
                    float duration, bool depthTest) {
    glm::vec3 right = glm::normalize(from - center);
    glm::vec3 up = glm::normalize(glm::cross(normal, right));
    
    float angleStep = angle / segments;
    
    for (int i = 0; i < segments; ++i) {
        float a1 = i * angleStep;
        float a2 = (i + 1) * angleStep;
        
        glm::vec3 p1 = center + (right * std::cos(a1) + up * std::sin(a1)) * radius;
        glm::vec3 p2 = center + (right * std::cos(a2) + up * std::sin(a2)) * radius;
        line(p1, p2, color, duration, depthTest);
    }
}

void DebugDraw::frustum(const glm::mat4& viewProjection, const glm::vec4& color,
                        float duration, bool depthTest) {
    glm::mat4 inv = glm::inverse(viewProjection);
    
    glm::vec4 ndcCorners[8] = {
        {-1, -1, -1, 1}, {1, -1, -1, 1}, {1, 1, -1, 1}, {-1, 1, -1, 1},
        {-1, -1,  1, 1}, {1, -1,  1, 1}, {1, 1,  1, 1}, {-1, 1,  1, 1}
    };
    
    glm::vec3 worldCorners[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec4 world = inv * ndcCorners[i];
        worldCorners[i] = glm::vec3(world) / world.w;
    }
    
    for (int i = 0; i < 4; ++i) {
        line(worldCorners[i], worldCorners[(i + 1) % 4], color, duration, depthTest);
        line(worldCorners[i + 4], worldCorners[((i + 1) % 4) + 4], color, duration, depthTest);
        line(worldCorners[i], worldCorners[i + 4], color, duration, depthTest);
    }
}

void DebugDraw::bounds(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color,
                       float duration, bool depthTest) {
    glm::vec3 center = (min + max) * 0.5f;
    glm::vec3 halfExtents = (max - min) * 0.5f;
    wireBox(center, halfExtents, color, duration, depthTest);
}

void DebugDraw::point(const glm::vec3& position, float size, const glm::vec4& color,
                      float duration, bool depthTest) {
    line(position - glm::vec3(size, 0, 0), position + glm::vec3(size, 0, 0), color, duration, depthTest);
    line(position - glm::vec3(0, size, 0), position + glm::vec3(0, size, 0), color, duration, depthTest);
    line(position - glm::vec3(0, 0, size), position + glm::vec3(0, 0, size), color, duration, depthTest);
}

void DebugDraw::text3D(const glm::vec3& position, const std::string& text, const glm::vec4& color,
                       float scale, float duration) {
    DebugText3D t;
    t.position = position;
    t.text = text;
    t.color = color;
    t.scale = scale;
    t.duration = duration;
    debugTexts.push_back(t);
}

void DebugDraw::collider(const GameObject& object, const glm::vec4& color, float duration) {
    (void)object; (void)color; (void)duration;
}

void DebugDraw::contactPoint(const glm::vec3& pt, const glm::vec3& normal, float penetration, float duration) {
    point(pt, 0.05f, glm::vec4(1, 0, 0, 1), duration, true);
    ray(pt, normal, penetration * 10.0f, glm::vec4(0, 1, 0, 1), duration, true);
}

void DebugDraw::velocity(const GameObject& object, float scale, float duration) {
    (void)object; (void)scale; (void)duration;
}

void DebugDraw::forces(const GameObject& object, float scale, float duration) {
    (void)object; (void)scale; (void)duration;
}

void DebugDraw::update(float deltaSeconds) {
    auto removeOldLines = [deltaSeconds](DebugLine& l) {
        if (l.duration > 0) { l.duration -= deltaSeconds; return l.duration <= 0; }
        return true;
    };
    debugLines.erase(std::remove_if(debugLines.begin(), debugLines.end(), removeOldLines), debugLines.end());
    
    auto removeOldSpheres = [deltaSeconds](DebugSphere& s) {
        if (s.duration > 0) { s.duration -= deltaSeconds; return s.duration <= 0; }
        return true;
    };
    debugSpheres.erase(std::remove_if(debugSpheres.begin(), debugSpheres.end(), removeOldSpheres), debugSpheres.end());
    
    auto removeOldBoxes = [deltaSeconds](DebugBox& b) {
        if (b.duration > 0) { b.duration -= deltaSeconds; return b.duration <= 0; }
        return true;
    };
    debugBoxes.erase(std::remove_if(debugBoxes.begin(), debugBoxes.end(), removeOldBoxes), debugBoxes.end());
    
    auto removeOldCapsules = [deltaSeconds](DebugCapsule& c) {
        if (c.duration > 0) { c.duration -= deltaSeconds; return c.duration <= 0; }
        return true;
    };
    debugCapsules.erase(std::remove_if(debugCapsules.begin(), debugCapsules.end(), removeOldCapsules), debugCapsules.end());
    
    auto removeOldArrows = [deltaSeconds](DebugArrow& a) {
        if (a.duration > 0) { a.duration -= deltaSeconds; return a.duration <= 0; }
        return true;
    };
    debugArrows.erase(std::remove_if(debugArrows.begin(), debugArrows.end(), removeOldArrows), debugArrows.end());
    
    auto removeOldTexts = [deltaSeconds](DebugText3D& t) {
        if (t.duration > 0) { t.duration -= deltaSeconds; return t.duration <= 0; }
        return true;
    };
    debugTexts.erase(std::remove_if(debugTexts.begin(), debugTexts.end(), removeOldTexts), debugTexts.end());
}

void DebugDraw::clear() {
    debugLines.clear();
    debugSpheres.clear();
    debugBoxes.clear();
    debugCapsules.clear();
    debugArrows.clear();
    debugTexts.clear();
}

// ============================================================================
// Profiler Implementation
// ============================================================================

Profiler& Profiler::instance() {
    static Profiler profiler;
    return profiler;
}

void Profiler::beginFrame() {
    if (!profilerEnabled) return;
    
    // Move current frame to history
    if (frameHistory.size() >= historySize) {
        frameHistory.erase(frameHistory.begin());
    }
    
    ProfileFrame frame;
    frame.frameNumber = currentFrameIndex++;
    frame.frameStart = std::chrono::high_resolution_clock::now();
    frameHistory.push_back(frame);
}

void Profiler::endFrame() {
    if (!profilerEnabled || frameHistory.empty()) return;
    
    ProfileFrame& frame = frameHistory.back();
    frame.frameEnd = std::chrono::high_resolution_clock::now();
    frame.cpuTimeMs = std::chrono::duration<float, std::milli>(frame.frameEnd - frame.frameStart).count();
    
    cpuFrameTime = frame.cpuTimeMs;
    
    // Update frame time history
    frameTimeHistoryBuffer.push_back(frame.cpuTimeMs);
    if (frameTimeHistoryBuffer.size() > historySize) {
        frameTimeHistoryBuffer.erase(frameTimeHistoryBuffer.begin());
    }
    
    // Calculate FPS
    if (frame.cpuTimeMs > 0) {
        currentFps = 1000.0f / frame.cpuTimeMs;
    }
}

void Profiler::beginSample(const std::string& name) {
    if (!profilerEnabled) return;
    sampleStack.push_back({name, std::chrono::high_resolution_clock::now()});
    currentDepth++;
}

void Profiler::endSample() {
    if (!profilerEnabled || sampleStack.empty()) return;
    
    auto [name, startTime] = sampleStack.back();
    sampleStack.pop_back();
    currentDepth--;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    // Record sample
    if (!frameHistory.empty()) {
        ProfileSample sample;
        sample.name = name;
        sample.startTime = startTime;
        sample.endTime = endTime;
        sample.depth = currentDepth;
        frameHistory.back().samples.push_back(sample);
    }
    
    // Update sample history
    auto& history = sampleHistory[name];
    history.push_back(ms);
    if (history.size() > historySize) {
        history.erase(history.begin());
    }
}

void Profiler::beginGpuSample(const std::string& name) {
    (void)name;
    // GPU timing requires Vulkan timestamp queries - placeholder
}

void Profiler::endGpuSample() {
    // GPU timing requires Vulkan timestamp queries - placeholder
}

const ProfileFrame& Profiler::currentFrame() const {
    static ProfileFrame empty;
    if (frameHistory.empty()) return empty;
    return frameHistory.back();
}

const ProfileFrame& Profiler::previousFrame() const {
    static ProfileFrame empty;
    if (frameHistory.size() < 2) return empty;
    return frameHistory[frameHistory.size() - 2];
}

ProfileStats Profiler::getStats(const std::string& name) const {
    ProfileStats stats;
    auto it = sampleHistory.find(name);
    if (it == sampleHistory.end() || it->second.empty()) return stats;
    
    const auto& history = it->second;
    stats.callCount = static_cast<std::uint32_t>(history.size());
    stats.lastTime = history.back();
    
    float sum = 0;
    stats.minTime = history[0];
    stats.maxTime = history[0];
    
    for (float t : history) {
        sum += t;
        stats.minTime = (std::min)(stats.minTime, t);
        stats.maxTime = (std::max)(stats.maxTime, t);
    }
    
    stats.avgTime = sum / static_cast<float>(history.size());
    return stats;
}

void Profiler::setHistorySize(std::size_t size) {
    historySize = size;
    if (frameHistory.size() > size) {
        frameHistory.erase(frameHistory.begin(), frameHistory.begin() + (frameHistory.size() - size));
    }
}

// ============================================================================
// MemoryProfiler Implementation
// ============================================================================

MemoryProfiler& MemoryProfiler::instance() {
    static MemoryProfiler profiler;
    return profiler;
}

void MemoryProfiler::trackAllocation(void* ptr, std::size_t size, const std::string& tag, 
                                      const char* file, int lineNum) {
    if (!trackingEnabled) return;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    AllocationInfo info;
    info.address = ptr;
    info.size = size;
    info.tag = tag;
    info.file = file ? file : "";
    info.line = lineNum;
    info.timestamp = std::chrono::steady_clock::now();
    
    allocations[ptr] = info;
    totalAllocatedBytes += size;
    peakAllocatedBytes = (std::max)(peakAllocatedBytes, totalAllocatedBytes);
    allocationCount++;
}

void MemoryProfiler::trackDeallocation(void* ptr) {
    if (!trackingEnabled) return;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = allocations.find(ptr);
    if (it != allocations.end()) {
        totalAllocatedBytes -= it->second.size;
        allocations.erase(it);
    }
}

std::vector<AllocationInfo> MemoryProfiler::getAllocations() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<AllocationInfo> result;
    result.reserve(allocations.size());
    for (const auto& [ptr, info] : allocations) {
        result.push_back(info);
    }
    return result;
}

std::vector<AllocationInfo> MemoryProfiler::getAllocationsByTag(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<AllocationInfo> result;
    for (const auto& [ptr, info] : allocations) {
        if (info.tag == tag) {
            result.push_back(info);
        }
    }
    return result;
}

void MemoryProfiler::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    allocations.clear();
    totalAllocatedBytes = 0;
    peakAllocatedBytes = 0;
    allocationCount = 0;
}

void MemoryProfiler::dumpLeaks() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (allocations.empty()) {
        std::cout << "No memory leaks detected.\n";
        return;
    }
    
    std::cout << "Memory leaks detected (" << allocations.size() << " allocations):\n";
    for (const auto& [ptr, info] : allocations) {
        std::cout << "  " << ptr << ": " << info.size << " bytes";
        if (!info.tag.empty()) std::cout << " [" << info.tag << "]";
        if (!info.file.empty()) std::cout << " at " << info.file << ":" << info.line;
        std::cout << "\n";
    }
}

// ============================================================================
// EntityInspector Implementation
// ============================================================================

void EntityInspector::select(GameObject* object) {
    selectedObject = object;
}

void EntityInspector::select(const std::string& name) {
    (void)name;
    // Would search scene for object by name
}

void EntityInspector::deselect() {
    selectedObject = nullptr;
}

std::vector<EntityInspector::HierarchyNode> EntityInspector::buildHierarchy() const {
    // Would build hierarchy from scene
    return {};
}

std::vector<EntityInspector::ComponentInfo> EntityInspector::getComponents(GameObject* object) const {
    (void)object;
    // Would inspect object's components
    return {};
}

void EntityInspector::drawHierarchyPanel() {
    // Would draw ImGui hierarchy panel
}

void EntityInspector::drawInspectorPanel() {
    // Would draw ImGui inspector panel
}

void EntityInspector::drawSceneStats() {
    // Would draw ImGui scene stats
}

// ============================================================================
// Logger Implementation
// ============================================================================

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::log(LogLevel level, const std::string& message, const std::string& category) {
    log(level, message, nullptr, 0, category);
}

void Logger::log(LogLevel level, const std::string& message, const char* file, int lineNum, 
                 const std::string& category) {
    if (level < minLevel) return;
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.category = category;
    entry.file = file ? file : "";
    entry.line = lineNum;
    entry.timestamp = std::chrono::system_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(mutex);
        logEntries.push_back(entry);
        if (logEntries.size() > maxEntries) {
            logEntries.erase(logEntries.begin());
        }
    }
    
    // Console output
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S") << " ";
    
    switch (level) {
        case LogLevel::Trace:   oss << "[TRACE] "; break;
        case LogLevel::Debug:   oss << "[DEBUG] "; break;
        case LogLevel::Info:    oss << "[INFO]  "; break;
        case LogLevel::Warning: oss << "[WARN]  "; break;
        case LogLevel::Error:   oss << "[ERROR] "; break;
        case LogLevel::Fatal:   oss << "[FATAL] "; break;
    }
    
    if (!category.empty()) oss << "[" << category << "] ";
    oss << message;
    if (file) oss << " (" << file << ":" << lineNum << ")";
    
    std::cout << oss.str() << std::endl;
    
    // File output
    if (logFile && logFile->is_open()) {
        *logFile << oss.str() << "\n";
        logFile->flush();
    }
    
    // Callbacks
    for (const auto& [name, callback] : callbacks) {
        callback(entry);
    }
}

void Logger::trace(const std::string& message, const std::string& category) {
    log(LogLevel::Trace, message, category);
}

void Logger::debug(const std::string& message, const std::string& category) {
    log(LogLevel::Debug, message, category);
}

void Logger::info(const std::string& message, const std::string& category) {
    log(LogLevel::Info, message, category);
}

void Logger::warning(const std::string& message, const std::string& category) {
    log(LogLevel::Warning, message, category);
}

void Logger::error(const std::string& message, const std::string& category) {
    log(LogLevel::Error, message, category);
}

void Logger::fatal(const std::string& message, const std::string& category) {
    log(LogLevel::Fatal, message, category);
}

void Logger::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    logEntries.clear();
}

void Logger::setLogFile(const std::string& path) {
    closeLogFile();
    logFile = new std::ofstream(path, std::ios::app);
}

void Logger::closeLogFile() {
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
}

void Logger::addCallback(const std::string& name, LogCallback callback) {
    callbacks[name] = std::move(callback);
}

void Logger::removeCallback(const std::string& name) {
    callbacks.erase(name);
}

// ============================================================================
// DebugConsole Implementation
// ============================================================================

DebugConsole& DebugConsole::instance() {
    static DebugConsole console;
    return console;
}

DebugConsole::DebugConsole() {
    registerBuiltInCommands();
}

void DebugConsole::registerCommand(const std::string& name, CommandHandler handler, 
                                    const std::string& description) {
    CommandInfo info;
    info.handler = std::move(handler);
    info.description = description;
    commands[name] = info;
}

void DebugConsole::unregisterCommand(const std::string& name) {
    commands.erase(name);
}

std::string DebugConsole::execute(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string commandName;
    iss >> commandName;
    
    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    auto it = commands.find(commandName);
    if (it != commands.end()) {
        return it->second.handler(args);
    }
    
    return "Unknown command: " + commandName;
}

std::vector<std::string> DebugConsole::getCompletions(const std::string& partial) const {
    std::vector<std::string> completions;
    for (const auto& [name, info] : commands) {
        if (name.find(partial) == 0) {
            completions.push_back(name);
        }
    }
    return completions;
}

std::vector<std::pair<std::string, std::string>> DebugConsole::getCommands() const {
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(commands.size());
    for (const auto& [name, info] : commands) {
        result.emplace_back(name, info.description);
    }
    return result;
}

void DebugConsole::registerBuiltInCommands() {
    registerCommand("help", [this](const std::vector<std::string>&) {
        std::ostringstream oss;
        oss << "Available commands:\n";
        for (const auto& [name, desc] : getCommands()) {
            oss << "  " << name << " - " << desc << "\n";
        }
        return oss.str();
    }, "Show available commands");
    
    registerCommand("echo", [](const std::vector<std::string>& args) {
        std::ostringstream oss;
        for (const auto& arg : args) oss << arg << " ";
        return oss.str();
    }, "Echo arguments");
    
    registerCommand("fps", [](const std::vector<std::string>&) {
        return "FPS: " + std::to_string(static_cast<int>(Profiler::instance().fps()));
    }, "Show current FPS");
    
    registerCommand("memory", [](const std::vector<std::string>&) {
        auto& mem = MemoryProfiler::instance();
        return "Allocated: " + std::to_string(mem.totalAllocated() / 1024) + " KB, Peak: " + 
               std::to_string(mem.peakMemory() / 1024) + " KB";
    }, "Show memory usage");
}

} // namespace vkengine
