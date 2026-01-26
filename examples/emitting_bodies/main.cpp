#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/ecs/Components.hpp"
#include "VulkanRenderer.hpp"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

struct EmitterHandle {
    vkengine::GameObject* object{nullptr};
    vkengine::Light* light{nullptr};
};

class EmittingBodiesManipulator {
public:
    void bind(VulkanCubeApp& appRef,
              vkengine::GameEngine& engineRef,
              std::shared_ptr<std::vector<EmitterHandle>> emittersRef)
    {
        app = &appRef;
        engine = &engineRef;
        emitters = std::move(emittersRef);
        keyLatch.fill(false);

        app->setCustomCursorCallback([this](double x, double y) {
            onCursor(x, y);
        });
        app->setCustomMouseButtonCallback([this](int button, int action, double x, double y) {
            onMouseButton(button, action, x, y);
        });
    }

    void update()
    {
        if (!engine || !emitters || emitters->empty()) {
            return;
        }

        updateSelectionFromKeyboard();

        glm::vec2 delta;
        bool draggingCopy = false;
        {
            std::scoped_lock lock(stateMutex);
            delta = pendingDelta;
            pendingDelta = glm::vec2(0.0f);
            draggingCopy = dragging;
        }

        if (!draggingCopy || (glm::length2(delta) <= std::numeric_limits<float>::epsilon())) {
            return;
        }

        auto& emitter = emitters->at(activeIndex);
        auto& transform = emitter.object->transform();

        const auto& camera = engine->scene().camera();
        glm::vec3 right = glm::normalize(camera.rightVector());
        glm::vec3 forward = camera.forwardVector();
        forward.y = 0.0f;
        if (glm::length2(forward) < 1e-5f) {
            forward = glm::vec3(0.0f, 0.0f, -1.0f);
        } else {
            forward = glm::normalize(forward);
        }

        glm::vec3 translation = (right * delta.x * mouseSensitivity) + (forward * (-delta.y) * mouseSensitivity);
        translation.y = 0.0f;
        transform.position += translation;
        transform.position.y = emitterHeight;
        clampToPlane(transform.position);

        if (emitter.light != nullptr) {
            emitter.light->setPosition(transform.position + glm::vec3(0.0f, lightOffset, 0.0f));
        }
    }

    std::shared_ptr<std::vector<EmitterHandle>> emittersShared() const { return emitters; }

private:
    static constexpr float mouseSensitivity = 0.01f;
    static constexpr float emitterHeight = 0.6f;
    static constexpr float lightOffset = 0.5f;
    static constexpr float planeHalfSize = 6.5f;

    void clampToPlane(glm::vec3& position) const
    {
        position.x = std::clamp(position.x, -planeHalfSize, planeHalfSize);
        position.z = std::clamp(position.z, -planeHalfSize, planeHalfSize);
    }

    void updateSelectionFromKeyboard()
    {
        auto& registry = engine->scene().registry();
        registry.view<vkengine::InputStateComponent>([&](auto, const vkengine::InputStateComponent& input) {
            const auto& keys = input.keyStates;
            for (std::size_t i = 0; i < emitters->size() && i < keyLatch.size(); ++i) {
                const int key = GLFW_KEY_1 + static_cast<int>(i);
                const bool pressed = key >= 0 && key < static_cast<int>(keys.size()) && keys[static_cast<std::size_t>(key)];
                if (pressed && !keyLatch[i]) {
                    activeIndex = i;
                }
                keyLatch[i] = pressed;
            }
        });
    }

    void onCursor(double x, double y)
    {
        std::scoped_lock lock(stateMutex);
        if (!dragging) {
            lastCursor = {x, y};
            return;
        }
        const glm::vec2 delta{static_cast<float>(x - lastCursor.x), static_cast<float>(y - lastCursor.y)};
        pendingDelta += delta;
        lastCursor = {x, y};
    }

    void onMouseButton(int button, int action, double x, double y)
    {
        if (button != GLFW_MOUSE_BUTTON_LEFT) {
            return;
        }
        std::scoped_lock lock(stateMutex);
        dragging = (action == GLFW_PRESS || action == GLFW_REPEAT);
        lastCursor = {x, y};
        if (!dragging) {
            pendingDelta = glm::vec2(0.0f);
        }
    }

private:
    VulkanCubeApp* app{nullptr};
    vkengine::GameEngine* engine{nullptr};
    std::shared_ptr<std::vector<EmitterHandle>> emitters{};

    std::array<bool, 9> keyLatch{};
    std::size_t activeIndex = 0;

    std::mutex stateMutex;
    glm::vec2 pendingDelta{0.0f};
    glm::dvec2 lastCursor{0.0, 0.0};
    bool dragging = false;
};

class EmittingBodiesEngine : public vkengine::GameEngine {
public:
    void setManipulator(std::shared_ptr<EmittingBodiesManipulator> manip)
    {
        manipulator = std::move(manip);
    }

    void update(float deltaSeconds) override
    {
        vkengine::GameEngine::update(deltaSeconds);
        if (manipulator) {
            manipulator->update();
        }
    }

private:
    std::shared_ptr<EmittingBodiesManipulator> manipulator;
};

std::shared_ptr<std::vector<EmitterHandle>> buildScene(EmittingBodiesEngine& engine)
{
    auto emitters = std::make_shared<std::vector<EmitterHandle>>();
    emitters->reserve(3);

    auto& scene = engine.scene();
    scene.clear();

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 5.0f, 12.0f});
    camera.lookAt(glm::vec3{0.0f, 0.0f, 0.0f});
    camera.setMovementSpeed(5.0f);

    auto& plane = engine.createObject("EmitterPlane", vkengine::MeshType::Cube);
    plane.transform().scale = glm::vec3{14.0f, 0.2f, 14.0f};
    plane.transform().position = glm::vec3{0.0f, -1.2f, 0.0f};
    plane.setBaseColor(glm::vec3{0.08f, 0.08f, 0.08f});
    plane.enableCollider(glm::vec3{7.0f, 0.1f, 7.0f}, true);

    const std::array<glm::vec3, 3> colors = {
        glm::vec3{1.0f, 0.65f, 0.25f},
        glm::vec3{0.45f, 0.85f, 1.0f},
        glm::vec3{0.9f, 0.35f, 1.0f}
    };
    const std::array<glm::vec3, 3> positions = {
        glm::vec3{-3.0f, 0.6f, -2.0f},
        glm::vec3{0.0f, 0.6f, 2.5f},
        glm::vec3{3.0f, 0.6f, -1.0f}
    };

    for (std::size_t i = 0; i < colors.size(); ++i) {
        auto& emitterObject = engine.createObject("Emitter" + std::to_string(i), vkengine::MeshType::Cube);
        emitterObject.setMeshResource("assets/models/emitter_sphere.stl");
        emitterObject.transform().position = positions[i];
        emitterObject.transform().scale = glm::vec3{1.2f};
        emitterObject.setBaseColor(colors[i]);

        vkengine::LightCreateInfo info{};
        info.name = "EmitterLight" + std::to_string(i);
        info.position = positions[i] + glm::vec3{0.0f, 0.5f, 0.0f};
        info.color = colors[i];
        info.intensity = 4.5f;
        auto& light = scene.createLight(info);

        emitters->push_back(EmitterHandle{&emitterObject, &light});
    }

    return emitters;
}

void printInstructions()
{
    std::cout << "\n=== Emitting Bodies Example ===\n";
    std::cout << "- Three emissive spheres illuminate a matte plane without any extra fill lights.\n";
    std::cout << "- Hold the RIGHT mouse button to look around (same as other samples).\n";
    std::cout << "- Use the LEFT mouse button to grab the active emitter and drag it across the plane.\n";
    std::cout << "- Press number keys 1-3 to choose which emitter is active.\n";
    std::cout << "- WASD/Space/Ctrl move the camera; Shift boosts speed.\n\n";
}

} // namespace

int main(int argc, char** argv)
try {
    EmittingBodiesEngine engine;
    auto manipulator = std::make_shared<EmittingBodiesManipulator>();
    auto emitters = buildScene(engine);

    VulkanCubeApp renderer(engine);
    renderer.setLightAnimationEnabled(false);
    manipulator->bind(renderer, engine, emitters);
    engine.setManipulator(manipulator);

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "EmittingBodiesExample");
    if (capture.enabled) {
        WindowConfig window{};
        window.width = capture.width;
        window.height = capture.height;
        window.headless = true;
        window.resizable = false;
        renderer.setWindowConfig(window);
        renderer.setSceneControlsEnabled(false);
        std::filesystem::create_directories(capture.outputPath.parent_path());
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    printInstructions();
    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "EmittingBodies example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
