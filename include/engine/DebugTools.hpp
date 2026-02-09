#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// Debug Draw Types
// ============================================================================

struct DebugLine {
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    glm::vec4 color{1.0f};
    float duration{0.0f};  // 0 = single frame
    bool depthTest{true};
};

struct DebugSphere {
    glm::vec3 center{0.0f};
    float radius{1.0f};
    glm::vec4 color{1.0f};
    int segments{16};
    float duration{0.0f};
    bool depthTest{true};
    bool wireframe{true};
};

struct DebugBox {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{0.5f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f};
    float duration{0.0f};
    bool depthTest{true};
    bool wireframe{true};
};

struct DebugCapsule {
    glm::vec3 point1{0.0f};
    glm::vec3 point2{0.0f, 1.0f, 0.0f};
    float radius{0.5f};
    glm::vec4 color{1.0f};
    int segments{12};
    float duration{0.0f};
    bool depthTest{true};
};

struct DebugArrow {
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f, 1.0f, 0.0f};
    float headSize{0.1f};
    glm::vec4 color{1.0f};
    float duration{0.0f};
    bool depthTest{true};
};

struct DebugText3D {
    glm::vec3 position{0.0f};
    std::string text;
    glm::vec4 color{1.0f};
    float scale{1.0f};
    float duration{0.0f};
    bool billboard{true};  // Always face camera
};

// ============================================================================
// Debug Draw System
// ============================================================================

class DebugDraw {
public:
    static DebugDraw& instance();

    // Drawing primitives
    void line(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void ray(const glm::vec3& origin, const glm::vec3& direction, float length, const glm::vec4& color = {1.0f, 1.0f, 0.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void sphere(const glm::vec3& center, float radius, const glm::vec4& color = {0.0f, 1.0f, 0.0f, 1.0f}, int segments = 16, float duration = 0.0f, bool depthTest = true);
    void wireSphere(const glm::vec3& center, float radius, const glm::vec4& color = {0.0f, 1.0f, 0.0f, 1.0f}, int segments = 16, float duration = 0.0f, bool depthTest = true);
    void box(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color = {0.0f, 0.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void box(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec4& color = {0.0f, 0.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void wireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color = {0.0f, 0.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void wireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec4& color = {0.0f, 0.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void capsule(const glm::vec3& point1, const glm::vec3& point2, float radius, const glm::vec4& color = {1.0f, 0.0f, 1.0f, 1.0f}, int segments = 12, float duration = 0.0f, bool depthTest = true);
    void cylinder(const glm::vec3& point1, const glm::vec3& point2, float radius, const glm::vec4& color = {0.5f, 0.5f, 0.0f, 1.0f}, int segments = 16, float duration = 0.0f, bool depthTest = true);
    void cone(const glm::vec3& apex, const glm::vec3& base, float radius, const glm::vec4& color = {1.0f, 0.5f, 0.0f, 1.0f}, int segments = 16, float duration = 0.0f, bool depthTest = true);
    void arrow(const glm::vec3& start, const glm::vec3& end, float headSize = 0.1f, const glm::vec4& color = {1.0f, 1.0f, 0.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void axes(const glm::vec3& position, const glm::quat& rotation = {1.0f, 0.0f, 0.0f, 0.0f}, float size = 1.0f, float duration = 0.0f, bool depthTest = true);
    void grid(const glm::vec3& center, const glm::vec3& normal, float size, int divisions, const glm::vec4& color = {0.5f, 0.5f, 0.5f, 0.5f}, float duration = 0.0f, bool depthTest = true);
    void circle(const glm::vec3& center, const glm::vec3& normal, float radius, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, int segments = 32, float duration = 0.0f, bool depthTest = true);
    void arc(const glm::vec3& center, const glm::vec3& normal, const glm::vec3& from, float angle, float radius, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, int segments = 16, float duration = 0.0f, bool depthTest = true);
    void frustum(const glm::mat4& viewProjection, const glm::vec4& color = {1.0f, 1.0f, 0.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void bounds(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void point(const glm::vec3& position, float size = 0.1f, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, float duration = 0.0f, bool depthTest = true);
    void text3D(const glm::vec3& position, const std::string& text, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, float scale = 1.0f, float duration = 0.0f);

    // Physics debug visualization
    void collider(const GameObject& object, const glm::vec4& color = {0.0f, 1.0f, 0.0f, 0.5f}, float duration = 0.0f);
    void contactPoint(const glm::vec3& point, const glm::vec3& normal, float penetration, float duration = 0.0f);
    void velocity(const GameObject& object, float scale = 1.0f, float duration = 0.0f);
    void forces(const GameObject& object, float scale = 0.1f, float duration = 0.0f);

    // Update and clear
    void update(float deltaSeconds);
    void clear();
    void setEnabled(bool enabled) { debugEnabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return debugEnabled; }

    // Get geometry for rendering
    [[nodiscard]] const std::vector<DebugLine>& lines() const { return debugLines; }
    [[nodiscard]] const std::vector<DebugSphere>& spheres() const { return debugSpheres; }
    [[nodiscard]] const std::vector<DebugBox>& boxes() const { return debugBoxes; }
    [[nodiscard]] const std::vector<DebugText3D>& texts() const { return debugTexts; }

private:
    DebugDraw() = default;

    bool debugEnabled{true};
    std::vector<DebugLine> debugLines;
    std::vector<DebugSphere> debugSpheres;
    std::vector<DebugBox> debugBoxes;
    std::vector<DebugCapsule> debugCapsules;
    std::vector<DebugArrow> debugArrows;
    std::vector<DebugText3D> debugTexts;
};

// ============================================================================
// Profiler Types
// ============================================================================

struct ProfileSample {
    std::string name;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    std::uint32_t depth{0};
    std::uint64_t threadId{0};
};

struct ProfileFrame {
    std::uint64_t frameNumber{0};
    std::chrono::high_resolution_clock::time_point frameStart;
    std::chrono::high_resolution_clock::time_point frameEnd;
    std::vector<ProfileSample> samples;
    float cpuTimeMs{0.0f};
    float gpuTimeMs{0.0f};
};

struct ProfileStats {
    float minTime{0.0f};
    float maxTime{0.0f};
    float avgTime{0.0f};
    float lastTime{0.0f};
    std::uint32_t callCount{0};
};

// ============================================================================
// Profiler
// ============================================================================

class Profiler {
public:
    static Profiler& instance();

    void beginFrame();
    void endFrame();

    void beginSample(const std::string& name);
    void endSample();

    // GPU timing (requires Vulkan timestamp queries)
    void beginGpuSample(const std::string& name);
    void endGpuSample();

    // Statistics
    [[nodiscard]] const ProfileFrame& currentFrame() const;
    [[nodiscard]] const ProfileFrame& previousFrame() const;
    [[nodiscard]] ProfileStats getStats(const std::string& name) const;
    [[nodiscard]] float fps() const { return currentFps; }
    [[nodiscard]] float frameTime() const { return 1000.0f / currentFps; }
    [[nodiscard]] float cpuTime() const { return cpuFrameTime; }
    [[nodiscard]] float gpuTime() const { return gpuFrameTime; }

    // History for graphs
    [[nodiscard]] const std::vector<float>& frameTimeHistory() const { return frameTimeHistoryBuffer; }
    [[nodiscard]] const std::vector<float>& cpuTimeHistory() const { return cpuTimeHistoryBuffer; }
    [[nodiscard]] const std::vector<float>& gpuTimeHistory() const { return gpuTimeHistoryBuffer; }

    void setEnabled(bool enabled) { profilerEnabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return profilerEnabled; }

    void setHistorySize(std::size_t size);

private:
    Profiler() = default;

    bool profilerEnabled{true};
    std::vector<ProfileFrame> frameHistory;
    std::size_t currentFrameIndex{0};
    std::vector<std::pair<std::string, std::chrono::high_resolution_clock::time_point>> sampleStack;
    std::uint32_t currentDepth{0};

    std::unordered_map<std::string, std::vector<float>> sampleHistory;

    float currentFps{0.0f};
    float cpuFrameTime{0.0f};
    float gpuFrameTime{0.0f};

    std::vector<float> frameTimeHistoryBuffer;
    std::vector<float> cpuTimeHistoryBuffer;
    std::vector<float> gpuTimeHistoryBuffer;
    std::size_t historySize{120};
};

// RAII scope profiler
class ProfileScope {
public:
    explicit ProfileScope(const std::string& name) : scopeName(name) {
        Profiler::instance().beginSample(name);
    }
    ~ProfileScope() {
        Profiler::instance().endSample();
    }

private:
    std::string scopeName;
};

#define PROFILE_SCOPE(name) vkengine::ProfileScope _profileScope##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

// ============================================================================
// Memory Profiler
// ============================================================================

struct AllocationInfo {
    void* address{nullptr};
    std::size_t size{0};
    std::string tag;
    std::string file;
    int line{0};
    std::chrono::steady_clock::time_point timestamp;
};

class MemoryProfiler {
public:
    static MemoryProfiler& instance();

    void trackAllocation(void* ptr, std::size_t size, const std::string& tag = "", const char* file = nullptr, int line = 0);
    void trackDeallocation(void* ptr);

    [[nodiscard]] std::size_t totalAllocated() const { return totalAllocatedBytes; }
    [[nodiscard]] std::size_t totalAllocations() const { return allocationCount; }
    [[nodiscard]] std::size_t currentAllocations() const { return allocations.size(); }
    [[nodiscard]] std::size_t peakMemory() const { return peakAllocatedBytes; }

    [[nodiscard]] std::vector<AllocationInfo> getAllocations() const;
    [[nodiscard]] std::vector<AllocationInfo> getAllocationsByTag(const std::string& tag) const;

    void setEnabled(bool enabled) { trackingEnabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return trackingEnabled; }

    void reset();
    void dumpLeaks() const;

private:
    MemoryProfiler() = default;

    bool trackingEnabled{false};
    std::unordered_map<void*, AllocationInfo> allocations;
    std::size_t totalAllocatedBytes{0};
    std::size_t peakAllocatedBytes{0};
    std::size_t allocationCount{0};
    mutable std::mutex mutex;
};

// ============================================================================
// Entity Inspector
// ============================================================================

class EntityInspector {
public:
    EntityInspector() = default;

    void setScene(Scene* scene) { currentScene = scene; }

    // Selection
    void select(GameObject* object);
    void select(const std::string& name);
    void deselect();
    [[nodiscard]] GameObject* selected() const { return selectedObject; }
    [[nodiscard]] bool hasSelection() const { return selectedObject != nullptr; }

    // Hierarchy view data
    struct HierarchyNode {
        std::string name;
        GameObject* object{nullptr};
        bool expanded{false};
        std::vector<HierarchyNode> children;
    };
    [[nodiscard]] std::vector<HierarchyNode> buildHierarchy() const;

    // Component inspection
    struct ComponentInfo {
        std::string typeName;
        bool enabled{true};
        std::function<void()> drawUI;
    };
    [[nodiscard]] std::vector<ComponentInfo> getComponents(GameObject* object) const;

    // Property modification callbacks
    using PropertyChangeCallback = std::function<void(const std::string& property, const std::string& value)>;
    void setOnPropertyChanged(PropertyChangeCallback callback) { onPropertyChanged = std::move(callback); }

    // Draw ImGui inspector UI
    void drawHierarchyPanel();
    void drawInspectorPanel();
    void drawSceneStats();

private:
    Scene* currentScene{nullptr};
    GameObject* selectedObject{nullptr};
    PropertyChangeCallback onPropertyChanged;
    std::string searchFilter;
    bool showHidden{false};
};

// ============================================================================
// Console / Log System
// ============================================================================

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string category;
    std::string file;
    int line{0};
    std::chrono::system_clock::time_point timestamp;
};

class Logger {
public:
    static Logger& instance();

    void log(LogLevel level, const std::string& message, const std::string& category = "");
    void log(LogLevel level, const std::string& message, const char* file, int line, const std::string& category = "");

    void trace(const std::string& message, const std::string& category = "");
    void debug(const std::string& message, const std::string& category = "");
    void info(const std::string& message, const std::string& category = "");
    void warning(const std::string& message, const std::string& category = "");
    void error(const std::string& message, const std::string& category = "");
    void fatal(const std::string& message, const std::string& category = "");

    void setMinLevel(LogLevel level) { minLevel = level; }
    [[nodiscard]] LogLevel getMinLevel() const { return minLevel; }

    void setMaxEntries(std::size_t max) { maxEntries = max; }
    [[nodiscard]] const std::vector<LogEntry>& entries() const { return logEntries; }

    void clear();

    // File output
    void setLogFile(const std::string& path);
    void closeLogFile();

    // Callbacks
    using LogCallback = std::function<void(const LogEntry&)>;
    void addCallback(const std::string& name, LogCallback callback);
    void removeCallback(const std::string& name);

private:
    Logger() = default;

    LogLevel minLevel{LogLevel::Info};
    std::size_t maxEntries{1000};
    std::vector<LogEntry> logEntries;
    std::unordered_map<std::string, LogCallback> callbacks;
    std::ofstream* logFile{nullptr};
    mutable std::mutex mutex;
};

#define EVE_LOG_TRACE(msg) vkengine::Logger::instance().log(vkengine::LogLevel::Trace, msg, __FILE__, __LINE__)
#define EVE_LOG_DEBUG(msg) vkengine::Logger::instance().log(vkengine::LogLevel::Debug, msg, __FILE__, __LINE__)
#define EVE_LOG_INFO(msg) vkengine::Logger::instance().log(vkengine::LogLevel::Info, msg, __FILE__, __LINE__)
#define EVE_LOG_WARNING(msg) vkengine::Logger::instance().log(vkengine::LogLevel::Warning, msg, __FILE__, __LINE__)
#define EVE_LOG_ERROR(msg) vkengine::Logger::instance().log(vkengine::LogLevel::Error, msg, __FILE__, __LINE__)
#define EVE_LOG_FATAL(msg) vkengine::Logger::instance().log(vkengine::LogLevel::Fatal, msg, __FILE__, __LINE__)

// ============================================================================
// Debug Console Commands
// ============================================================================

class DebugConsole {
public:
    static DebugConsole& instance();

    using CommandHandler = std::function<std::string(const std::vector<std::string>&)>;

    void registerCommand(const std::string& name, CommandHandler handler, const std::string& description = "");
    void unregisterCommand(const std::string& name);

    std::string execute(const std::string& commandLine);
    [[nodiscard]] std::vector<std::string> getCompletions(const std::string& partial) const;
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> getCommands() const;

    // Built-in commands
    void registerBuiltInCommands();

private:
    DebugConsole();

    struct CommandInfo {
        CommandHandler handler;
        std::string description;
    };

    std::unordered_map<std::string, CommandInfo> commands;
};

} // namespace vkengine
