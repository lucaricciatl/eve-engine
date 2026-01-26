#pragma once

#include "engine/InputTypes.hpp"

#include "core/ecs/Entity.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

namespace core::ecs {
class Registry;
}

namespace vkengine {
class InputManager {
public:
    InputManager();
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    void attachRegistry(core::ecs::Registry* registry, core::ecs::Entity entity = {});
    void detachRegistry();

    void setKeyState(int key, bool pressed);
    void enableMouseLook(bool enabled);
    void handleMousePosition(double xpos, double ypos);

    void addMouseScroll(float delta);

    CameraInput consumeCameraInput();

    void setMouseSensitivity(float sensitivity) noexcept { mouseSensitivity = sensitivity; }
    [[nodiscard]] bool isMouseLookActive() const;

private:
    struct InputEvent {
        enum class Kind {
            Key,
            MouseMove,
            MouseLook,
            Scroll
        };

        Kind kind{Kind::Key};
        int key{0};
        bool pressed{false};
        double xpos{0.0};
        double ypos{0.0};
        float scrollDelta{0.0f};
    };

    static constexpr int MaxKeys = kMaxInputKeys;

    void enqueueEvent(InputEvent event);
    void workerLoop();
    void applyEvent(const InputEvent& event);
    [[nodiscard]] bool keyPressed(const std::array<bool, MaxKeys>& snapshot, int key) const;
    void publishInputSnapshot(const CameraInput& input,
                              const std::array<bool, MaxKeys>& keys,
                              const glm::vec2& mouseDelta,
                              bool mouseLookActive,
                              const glm::vec2& scrollDelta);
    core::ecs::Entity ensureInputEntity();
    void startWorker();
    void stopWorker();

    std::array<bool, MaxKeys> keyStates{};
    glm::vec2 pendingMouseDelta{0.0f};
    glm::vec2 pendingScrollDelta{0.0f};
    bool mouseLookActive{false};
    bool firstMouseEvent{true};
    double lastCursorX{0.0};
    double lastCursorY{0.0};
    float mouseSensitivity{0.0025f};

    mutable std::mutex stateMutex;

    std::deque<InputEvent> eventQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::atomic<bool> running{false};
    std::thread workerThread;

    core::ecs::Registry* attachedRegistry{nullptr};
    core::ecs::Entity attachedEntity{};
    std::uint64_t snapshotCounter{0};
};

} // namespace vkengine
