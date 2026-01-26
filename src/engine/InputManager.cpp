#include "engine/InputManager.hpp"

#include "core/ecs/Components.hpp"
#include "core/ecs/Registry.hpp"

#include <glm/geometric.hpp>

#include <utility>

namespace vkengine {

namespace {
bool isValidKey(int key) noexcept
{
    return key >= 0 && key < kMaxInputKeys;
}
} // namespace

InputManager::InputManager()
{
    startWorker();
}

InputManager::~InputManager()
{
    stopWorker();
}

void InputManager::attachRegistry(core::ecs::Registry* registry, core::ecs::Entity entity)
{
    attachedRegistry = registry;
    attachedEntity = entity;
}

void InputManager::detachRegistry()
{
    attachedRegistry = nullptr;
    attachedEntity = {};
}

void InputManager::setKeyState(int key, bool pressed)
{
    if (!isValidKey(key)) {
        return;
    }
    enqueueEvent(InputEvent{InputEvent::Kind::Key, key, pressed});
}

void InputManager::enableMouseLook(bool enabled)
{
    enqueueEvent(InputEvent{InputEvent::Kind::MouseLook, 0, enabled});
}

void InputManager::handleMousePosition(double xpos, double ypos)
{
    InputEvent event{};
    event.kind = InputEvent::Kind::MouseMove;
    event.xpos = xpos;
    event.ypos = ypos;
    enqueueEvent(event);
}

void InputManager::addMouseScroll(float delta)
{
    InputEvent event{};
    event.kind = InputEvent::Kind::Scroll;
    event.scrollDelta = delta;
    enqueueEvent(event);
}

CameraInput InputManager::consumeCameraInput()
{
    std::array<bool, MaxKeys> keySnapshot{};
    glm::vec2 mouseDelta{0.0f};
    glm::vec2 scrollDelta{0.0f};
    bool mouseLook = false;

    {
        std::scoped_lock lock(stateMutex);
        keySnapshot = keyStates;
        mouseDelta = pendingMouseDelta;
        scrollDelta = pendingScrollDelta;
        pendingMouseDelta = glm::vec2(0.0f);
        pendingScrollDelta = glm::vec2(0.0f);
        mouseLook = mouseLookActive;
    }

    auto pressed = [&](int key) {
        return keyPressed(keySnapshot, key);
    };

    CameraInput input{};
    glm::vec3 translation{0.0f};
    if (pressed(GLFW_KEY_W)) {
        translation.z += 1.0f;
    }
    if (pressed(GLFW_KEY_S)) {
        translation.z -= 1.0f;
    }
    if (pressed(GLFW_KEY_A)) {
        translation.x -= 1.0f;
    }
    if (pressed(GLFW_KEY_D)) {
        translation.x += 1.0f;
    }
    if (pressed(GLFW_KEY_SPACE)) {
        translation.y += 1.0f;
    }
    if (pressed(GLFW_KEY_LEFT_CONTROL) || pressed(GLFW_KEY_RIGHT_CONTROL)) {
        translation.y -= 1.0f;
    }

    const float len = glm::length(translation);
    if (len > 0.0f) {
        translation /= len;
    }
    input.translation = translation;
    input.speedMultiplier = (pressed(GLFW_KEY_LEFT_SHIFT) || pressed(GLFW_KEY_RIGHT_SHIFT)) ? 3.0f : 1.0f;
    input.rotation = mouseLook ? mouseDelta * mouseSensitivity : glm::vec2(0.0f);

    publishInputSnapshot(input, keySnapshot, mouseDelta, mouseLook, scrollDelta);

    return input;
}

bool InputManager::isMouseLookActive() const
{
    std::scoped_lock lock(stateMutex);
    return mouseLookActive;
}

void InputManager::enqueueEvent(InputEvent event)
{
    {
        std::scoped_lock lock(queueMutex);
        eventQueue.push_back(std::move(event));
    }
    queueCondition.notify_one();
}

void InputManager::workerLoop()
{
    while (true) {
        InputEvent event{};
        {
            std::unique_lock lock(queueMutex);
            queueCondition.wait(lock, [&] {
                return !eventQueue.empty() || !running.load();
            });

            if (eventQueue.empty()) {
                if (!running.load()) {
                    break;
                }
                continue;
            }

            event = std::move(eventQueue.front());
            eventQueue.pop_front();
        }

        applyEvent(event);
    }
}

void InputManager::applyEvent(const InputEvent& event)
{
    std::scoped_lock lock(stateMutex);
    switch (event.kind) {
    case InputEvent::Kind::Key:
        if (isValidKey(event.key)) {
            keyStates[static_cast<size_t>(event.key)] = event.pressed;
        }
        break;
    case InputEvent::Kind::MouseMove:
        if (!mouseLookActive) {
            return;
        }
        if (firstMouseEvent) {
            lastCursorX = event.xpos;
            lastCursorY = event.ypos;
            firstMouseEvent = false;
            return;
        }
        pendingMouseDelta.x += static_cast<float>(event.xpos - lastCursorX);
        pendingMouseDelta.y += static_cast<float>(event.ypos - lastCursorY);
        lastCursorX = event.xpos;
        lastCursorY = event.ypos;
        break;
    case InputEvent::Kind::MouseLook:
        mouseLookActive = event.pressed;
        firstMouseEvent = true;
        pendingMouseDelta = glm::vec2(0.0f);
        break;
    case InputEvent::Kind::Scroll:
        pendingScrollDelta.y += event.scrollDelta;
        break;
    }
}

bool InputManager::keyPressed(const std::array<bool, MaxKeys>& snapshot, int key) const
{
    if (!isValidKey(key)) {
        return false;
    }
    return snapshot[static_cast<size_t>(key)];
}

void InputManager::publishInputSnapshot(const CameraInput& input,
                                        const std::array<bool, MaxKeys>& keys,
                                        const glm::vec2& mouseDelta,
                                        bool mouseLook,
                                        const glm::vec2& scrollDelta)
{
    if (attachedRegistry == nullptr) {
        return;
    }

    auto entity = ensureInputEntity();
    if (!entity) {
        return;
    }

    auto& component = attachedRegistry->getOrEmplace<InputStateComponent>(entity);
    component.keyStates = keys;
    component.pendingMouseDelta = mouseDelta;
    component.scrollDelta = scrollDelta;
    component.mouseLookActive = mouseLook;
    component.latestCameraInput = input;
    component.latestSample = ++snapshotCounter;
}

core::ecs::Entity InputManager::ensureInputEntity()
{
    if (attachedRegistry == nullptr) {
        return {};
    }

    if (!attachedEntity || !attachedRegistry->contains(attachedEntity)) {
        attachedEntity = attachedRegistry->createEntity();
    }
    return attachedEntity;
}

void InputManager::startWorker()
{
    running.store(true);
    workerThread = std::thread([this] { workerLoop(); });
}

void InputManager::stopWorker()
{
    running.store(false);
    queueCondition.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

} // namespace vkengine
