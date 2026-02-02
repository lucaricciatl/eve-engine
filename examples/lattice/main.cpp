#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/ecs/Components.hpp"
#include "core/VulkanRenderer.hpp"
#include "ui/Ui.hpp"
#include "ui/Mouse.hpp"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Lattice configuration limits
constexpr int MIN_LATTICE_SIZE = 1;
constexpr int MAX_LATTICE_SIZE = 10;
constexpr float DEFAULT_SPHERE_RADIUS = 0.15f;
constexpr float LATTICE_SPACING = 1.0f;
constexpr float LAYER_SPACING = 1.2f;

// Camera control parameters
constexpr float ZOOM_SPEED = 0.15f;
constexpr float ROTATE_SPEED = 0.005f;
constexpr float MIN_ZOOM = 2.0f;
constexpr float MAX_ZOOM = 30.0f;
constexpr float SPHERE_SIZE_STEP = 0.005f;
constexpr float MIN_SPHERE_SIZE = 0.05f;
constexpr float MAX_SPHERE_SIZE = 0.4f;

// 3000K warm white light color (approximation)
// Color temperature 3000K roughly corresponds to RGB: (255, 180, 107) normalized
constexpr glm::vec3 LIGHT_COLOR_3000K{1.0f, 0.71f, 0.42f};

// Menu selection
enum class MenuSelection { X_Axis, Y_Axis, Z_Axis, SphereSize, COUNT };

class LatticeManipulator {
public:
    using RebuildCallback = std::function<void()>;

    void bind(VulkanRenderer& appRef, vkengine::GameEngine& engineRef)
    {
        app = &appRef;
        engine = &engineRef;
    }

    void setRebuildCallback(RebuildCallback callback)
    {
        rebuildCallback = std::move(callback);
    }

    void setSpheres(std::vector<vkengine::GameObject*> spheres)
    {
        sphereObjects = std::move(spheres);
    }

    void setLatticeDimensions(int x, int y, int z)
    {
        latticeX = x;
        latticeY = y;
        latticeZ = z;
    }

    void rebuild()
    {
        if (rebuildCallback) {
            rebuildCallback();
        }
    }

    void setSphereRadius(float radius)
    {
        sphereRadius = glm::clamp(radius, MIN_SPHERE_SIZE, MAX_SPHERE_SIZE);
        updateSphereSize();
        updateWindowTitle();
    }

    int getLatticeX() const { return latticeX; }
    int getLatticeY() const { return latticeY; }
    int getLatticeZ() const { return latticeZ; }
    float getSphereRadius() const { return sphereRadius; }
    MenuSelection getMenuSelection() const { return menuSelection; }

    void update()
    {
        if (!engine) return;

        // Mouse-driven rotation/zoom via ImGui IO
        const auto mouse = vkui::GetMouseState();
        if (!mouse.wantCaptureMouse && mouse.hasMouse) {
            if (mouse.buttons[1] && (std::abs(mouse.delta.x) > 0.001f || std::abs(mouse.delta.y) > 0.001f)) { // Right button
                cameraYaw -= mouse.delta.x * ROTATE_SPEED;
                cameraPitch += mouse.delta.y * ROTATE_SPEED;
                cameraPitch = glm::clamp(cameraPitch, glm::radians(-85.0f), glm::radians(85.0f));
                updateCameraPosition();
            }

            if (mouse.scroll.y != 0.0f) {
                cameraDistance -= mouse.scroll.y * 0.75f;
                cameraDistance = glm::clamp(cameraDistance, MIN_ZOOM, MAX_ZOOM);
                updateCameraPosition();
            }
        }

        // Process keyboard input
        processKeyboardInput();
    }

    void initCamera()
    {
        updateCameraPosition();
    }

    void updateWindowTitle()
    {
        if (!app || !app->getWindow()) return;

        std::ostringstream title;
        title << "Lattice  |  ";
        
        title << (menuSelection == MenuSelection::X_Axis ? "[" : " ");
        title << "X:" << latticeX;
        title << (menuSelection == MenuSelection::X_Axis ? "]" : " ");
        
        title << "  ";
        
        title << (menuSelection == MenuSelection::Y_Axis ? "[" : " ");
        title << "Y:" << latticeY;
        title << (menuSelection == MenuSelection::Y_Axis ? "]" : " ");
        
        title << "  ";
        
        title << (menuSelection == MenuSelection::Z_Axis ? "[" : " ");
        title << "Z:" << latticeZ;
        title << (menuSelection == MenuSelection::Z_Axis ? "]" : " ");
        
        title << "  ";
        
        title << (menuSelection == MenuSelection::SphereSize ? "[" : " ");
        title << "Size:" << std::fixed << std::setprecision(2) << sphereRadius;
        title << (menuSelection == MenuSelection::SphereSize ? "]" : " ");
        
        title << "  |  Total: " << (latticeX * latticeY * latticeZ) << " spheres";
        title << "  |  Arrow Keys: Select/Change  |  RMB+Drag: Rotate";

        glfwSetWindowTitle(app->getWindow(), title.str().c_str());
    }

private:
    VulkanRenderer* app{nullptr};
    vkengine::GameEngine* engine{nullptr};
    std::vector<vkengine::GameObject*> sphereObjects;
    RebuildCallback rebuildCallback;

    // Lattice dimensions
    int latticeX = 5;
    int latticeY = 2;
    int latticeZ = 5;

    // Camera orbit parameters
    float cameraDistance = 12.0f;
    float cameraYaw = glm::radians(45.0f);
    float cameraPitch = glm::radians(25.0f);
    float sphereRadius = DEFAULT_SPHERE_RADIUS;

    // Menu state
    MenuSelection menuSelection = MenuSelection::X_Axis;

    // Key latch for single-press detection
    std::array<bool, 512> keyLatch{};

    void updateCameraPosition()
    {
        auto& camera = engine->scene().camera();

        float x = cameraDistance * std::cos(cameraPitch) * std::sin(cameraYaw);
        float y = cameraDistance * std::sin(cameraPitch);
        float z = cameraDistance * std::cos(cameraPitch) * std::cos(cameraYaw);

        camera.setPosition(glm::vec3{x, y, z});
        camera.lookAt(glm::vec3{0.0f, 0.0f, 0.0f});
    }

    void updateSphereSize()
    {
        for (auto* sphere : sphereObjects) {
            sphere->transform().scale = glm::vec3{sphereRadius * 2.0f};
        }
        updateWindowTitle();
    }

    void processKeyboardInput()
    {
        auto& registry = engine->scene().registry();
        registry.view<vkengine::InputStateComponent>([&](auto, const vkengine::InputStateComponent& input) {
            const auto& keys = input.keyStates;

            // Camera zoom (W/S)
            if (keys[GLFW_KEY_W]) {
                cameraDistance -= ZOOM_SPEED;
                cameraDistance = glm::clamp(cameraDistance, MIN_ZOOM, MAX_ZOOM);
                updateCameraPosition();
            }
            if (keys[GLFW_KEY_S]) {
                cameraDistance += ZOOM_SPEED;
                cameraDistance = glm::clamp(cameraDistance, MIN_ZOOM, MAX_ZOOM);
                updateCameraPosition();
            }

            // Camera rotation (A/D)
            if (keys[GLFW_KEY_A]) {
                cameraYaw += ROTATE_SPEED * 5.0f;
                updateCameraPosition();
            }
            if (keys[GLFW_KEY_D]) {
                cameraYaw -= ROTATE_SPEED * 5.0f;
                updateCameraPosition();
            }

            // Menu navigation with Tab (cycle through options)
            bool tabPressed = keys[GLFW_KEY_TAB];
            if (tabPressed && !keyLatch[GLFW_KEY_TAB]) {
                int sel = static_cast<int>(menuSelection);
                sel = (sel + 1) % 4;
                menuSelection = static_cast<MenuSelection>(sel);
                updateWindowTitle();
            }
            keyLatch[GLFW_KEY_TAB] = tabPressed;

            // Left/Right arrows to change menu selection
            bool leftPressed = keys[GLFW_KEY_LEFT];
            if (leftPressed && !keyLatch[GLFW_KEY_LEFT]) {
                int sel = static_cast<int>(menuSelection);
                sel = (sel + 3) % 4;  // +3 is same as -1 mod 4
                menuSelection = static_cast<MenuSelection>(sel);
                updateWindowTitle();
            }
            keyLatch[GLFW_KEY_LEFT] = leftPressed;

            bool rightPressed = keys[GLFW_KEY_RIGHT];
            if (rightPressed && !keyLatch[GLFW_KEY_RIGHT]) {
                int sel = static_cast<int>(menuSelection);
                sel = (sel + 1) % 4;
                menuSelection = static_cast<MenuSelection>(sel);
                updateWindowTitle();
            }
            keyLatch[GLFW_KEY_RIGHT] = rightPressed;

            // Up/Down arrows to change selected value
            bool upPressed = keys[GLFW_KEY_UP];
            if (upPressed && !keyLatch[GLFW_KEY_UP]) {
                handleValueChange(1);
            }
            keyLatch[GLFW_KEY_UP] = upPressed;

            bool downPressed = keys[GLFW_KEY_DOWN];
            if (downPressed && !keyLatch[GLFW_KEY_DOWN]) {
                handleValueChange(-1);
            }
            keyLatch[GLFW_KEY_DOWN] = downPressed;

            // Q/E for quick sphere size adjustment (continuous)
            if (keys[GLFW_KEY_Q]) {
                sphereRadius = glm::clamp(sphereRadius - SPHERE_SIZE_STEP, MIN_SPHERE_SIZE, MAX_SPHERE_SIZE);
                updateSphereSize();
            }
            if (keys[GLFW_KEY_E]) {
                sphereRadius = glm::clamp(sphereRadius + SPHERE_SIZE_STEP, MIN_SPHERE_SIZE, MAX_SPHERE_SIZE);
                updateSphereSize();
            }

            // Enter to apply lattice changes
            bool enterPressed = keys[GLFW_KEY_ENTER];
            if (enterPressed && !keyLatch[GLFW_KEY_ENTER]) {
                if (rebuildCallback) {
                    rebuildCallback();
                }
            }
            keyLatch[GLFW_KEY_ENTER] = enterPressed;
        });
    }

    void handleValueChange(int delta)
    {
        bool needsRebuild = false;

        switch (menuSelection) {
        case MenuSelection::X_Axis:
            latticeX = glm::clamp(latticeX + delta, MIN_LATTICE_SIZE, MAX_LATTICE_SIZE);
            needsRebuild = true;
            break;
        case MenuSelection::Y_Axis:
            latticeY = glm::clamp(latticeY + delta, MIN_LATTICE_SIZE, MAX_LATTICE_SIZE);
            needsRebuild = true;
            break;
        case MenuSelection::Z_Axis:
            latticeZ = glm::clamp(latticeZ + delta, MIN_LATTICE_SIZE, MAX_LATTICE_SIZE);
            needsRebuild = true;
            break;
        case MenuSelection::SphereSize:
            sphereRadius = glm::clamp(sphereRadius + delta * 0.02f, MIN_SPHERE_SIZE, MAX_SPHERE_SIZE);
            updateSphereSize();
            break;
        }

        updateWindowTitle();

        if (needsRebuild && rebuildCallback) {
            rebuildCallback();
        }
    }
};

class LatticeEngine : public vkengine::GameEngine {
public:
    void setManipulator(std::shared_ptr<LatticeManipulator> manip)
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
    std::shared_ptr<LatticeManipulator> manipulator;
};

std::vector<vkengine::GameObject*> buildLattice(LatticeEngine& engine, int sizeX, int sizeY, int sizeZ, float sphereRadius)
{
    std::vector<vkengine::GameObject*> sphereObjects;

    auto& scene = engine.scene();
    scene.clear();

    // Calculate lattice offsets to center it
    const float offsetX = (sizeX - 1) * LATTICE_SPACING * 0.5f;
    const float offsetZ = (sizeZ - 1) * LATTICE_SPACING * 0.5f;
    const float baseY = -0.5f;

    // Metallic silver/chrome color for spheres
    const glm::vec3 metallicColor{0.88f, 0.89f, 0.92f};

    // Create lattice of spheres
    for (int y = 0; y < sizeY; ++y) {
        for (int x = 0; x < sizeX; ++x) {
            for (int z = 0; z < sizeZ; ++z) {
                std::string name = "Sphere_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
                auto& sphere = engine.createObject(name, vkengine::MeshType::CustomMesh);

                sphere.transform().position = glm::vec3{
                    x * LATTICE_SPACING - offsetX,
                    baseY + y * LAYER_SPACING,
                    z * LATTICE_SPACING - offsetZ
                };
                sphere.transform().scale = glm::vec3{sphereRadius * 2.0f};
                sphere.setMeshResource("assets/models/emitter_sphere.stl");

                // Metallic color with subtle variation per layer
                float layerTint = 1.0f - y * 0.05f;
                sphere.setBaseColor(metallicColor * layerTint);

                sphereObjects.push_back(&sphere);
            }
        }
    }

    // Create horizontal connections (along X axis) for each layer
    for (int y = 0; y < sizeY; ++y) {
        for (int x = 0; x < sizeX - 1; ++x) {
            for (int z = 0; z < sizeZ; ++z) {
                std::string name = "ConnX_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
                auto& conn = engine.createObject(name, vkengine::MeshType::Cube);

                conn.transform().position = glm::vec3{
                    (x + 0.5f) * LATTICE_SPACING - offsetX,
                    baseY + y * LAYER_SPACING,
                    z * LATTICE_SPACING - offsetZ
                };
                conn.transform().scale = glm::vec3{LATTICE_SPACING * 0.4f, 0.02f, 0.02f};
                conn.setBaseColor(glm::vec3{0.55f, 0.57f, 0.6f});
            }
        }
    }

    // Create horizontal connections (along Z axis) for each layer
    for (int y = 0; y < sizeY; ++y) {
        for (int x = 0; x < sizeX; ++x) {
            for (int z = 0; z < sizeZ - 1; ++z) {
                std::string name = "ConnZ_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
                auto& conn = engine.createObject(name, vkengine::MeshType::Cube);

                conn.transform().position = glm::vec3{
                    x * LATTICE_SPACING - offsetX,
                    baseY + y * LAYER_SPACING,
                    (z + 0.5f) * LATTICE_SPACING - offsetZ
                };
                conn.transform().scale = glm::vec3{0.02f, 0.02f, LATTICE_SPACING * 0.4f};
                conn.setBaseColor(glm::vec3{0.55f, 0.57f, 0.6f});
            }
        }
    }

    // Create vertical connections (along Y axis) between layers
    if (sizeY > 1) {
        for (int y = 0; y < sizeY - 1; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                for (int z = 0; z < sizeZ; ++z) {
                    std::string name = "ConnY_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
                    auto& conn = engine.createObject(name, vkengine::MeshType::Cube);

                    conn.transform().position = glm::vec3{
                        x * LATTICE_SPACING - offsetX,
                        baseY + y * LAYER_SPACING + LAYER_SPACING * 0.5f,
                        z * LATTICE_SPACING - offsetZ
                    };
                    conn.transform().scale = glm::vec3{0.02f, LAYER_SPACING * 0.4f, 0.02f};
                    conn.setBaseColor(glm::vec3{0.55f, 0.57f, 0.6f});
                }
            }
        }
    }

    // Setup fixed lighting - far away with 3000K warm white for smooth shadows
    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{25.0f, 35.0f, 25.0f};  // Far away for softer shadows
    keyLightInfo.color = LIGHT_COLOR_3000K;                   // 3000K warm white
    keyLightInfo.intensity = 2.25f;                            // Adjusted for warm light
    scene.createLight(keyLightInfo);

    return sphereObjects;
}

void printControls()
{
    std::cout << "\n=== Lattice Example ===\n";
    std::cout << "A dynamic lattice of connected metallic spheres.\n";
    std::cout << "Light: 3000K warm white\n\n";
    std::cout << "Controls:\n";
    std::cout << "  Right Mouse + Drag : Rotate camera around lattice\n";
    std::cout << "  W / S              : Zoom in / out\n";
    std::cout << "  A / D              : Rotate camera left / right\n";
    std::cout << "  Q / E              : Decrease / increase sphere size\n";
    std::cout << "\n";
    std::cout << "Menu (shown in window title bar):\n";
    std::cout << "  Left/Right Arrows  : Select option (X, Y, Z, Size)\n";
    std::cout << "  Up/Down Arrows     : Change selected value\n";
    std::cout << "  Tab                : Cycle through options\n";
    std::cout << "\n";
    std::cout << "  ESC                : Exit\n\n";
}

} // namespace

int main(int argc, char** argv)
try {
    LatticeEngine engine;

    auto manipulator = std::make_shared<LatticeManipulator>();
    manipulator->setLatticeDimensions(5, 2, 5);  // Initial 5x2x5 lattice

    // Build initial scene
    auto sphereObjects = buildLattice(engine, 
        manipulator->getLatticeX(), 
        manipulator->getLatticeY(), 
        manipulator->getLatticeZ(),
        manipulator->getSphereRadius());

    engine.setManipulator(manipulator);

    VulkanRenderer renderer(engine);
    renderer.setLightAnimationEnabled(false);  // Fixed light position
    manipulator->bind(renderer, engine);
    manipulator->setSpheres(std::move(sphereObjects));

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "LatticeExample");
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

    // Set up rebuild callback for when dimensions change
    manipulator->setRebuildCallback([&]() {
        auto newSpheres = buildLattice(engine,
            manipulator->getLatticeX(),
            manipulator->getLatticeY(),
            manipulator->getLatticeZ(),
            manipulator->getSphereRadius());
        manipulator->setSpheres(std::move(newSpheres));
    });

    renderer.setCustomUiCallback([manipulator](float /*deltaSeconds*/) {
        bool dirty = false;
        int x = manipulator->getLatticeX();
        int y = manipulator->getLatticeY();
        int z = manipulator->getLatticeZ();
        float radius = manipulator->getSphereRadius();

        if (vkui::BeginWindow("Lattice Controls")) {
            vkui::Text("Dimensions");
            dirty |= vkui::SliderInt("X", &x, MIN_LATTICE_SIZE, MAX_LATTICE_SIZE);
            dirty |= vkui::SliderInt("Y", &y, MIN_LATTICE_SIZE, MAX_LATTICE_SIZE);
            dirty |= vkui::SliderInt("Z", &z, MIN_LATTICE_SIZE, MAX_LATTICE_SIZE);
            vkui::Separator();
            vkui::Text("Sphere size");
            if (vkui::SliderFloat("Radius", &radius, MIN_SPHERE_SIZE, MAX_SPHERE_SIZE, "%.3f")) {
                manipulator->setSphereRadius(radius);
                dirty = true;
            }
            vkui::Separator();
            vkui::Text("Total spheres: " + std::to_string(x * y * z));
            if (vkui::Button("Rebuild")) {
                dirty = true;
            }
        }
        vkui::EndWindow();

        if (dirty) {
            manipulator->setLatticeDimensions(x, y, z);
            manipulator->rebuild();
        }
    });

    manipulator->initCamera();

    printControls();
    manipulator->updateWindowTitle();
    std::cout << std::endl;

    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "\nLattice example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
