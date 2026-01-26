# Vulkan Cube Engine Sample

A modern C++20 Vulkan sandbox whose gameplay logic now lives in a unified **core module**. That module hosts a header-only Entity–Component–System (ECS) registry, the gameplay/physics layers, and the Vulkan renderer abstraction. All examples link against `core`, so the same ECS-powered scene graph drives rigid bodies, soft bodies, particles, and the compute-driven N-body showcase.

## Prerequisites

- [Vulkan SDK](https://vulkan.lunarg.com/) 1.3 or newer installed and added to `PATH` (provides the Vulkan loader, validation layers, and `glslc`).
- CMake 3.24+
- A C++20-capable compiler (MSVC 2022, Clang 15, or GCC 11+).
- Git is required at configure time so CMake's `FetchContent` can download GLFW and GLM.

> **Note:** The build scripts automatically fail if `glslc` is not found. Ensure `%VULKAN_SDK%/Bin` (or `/bin` on Unix) is on your `PATH` before configuring.

## Building

```bash
cmake -S . -B build
cmake --build build --config Release
```

The shaders are compiled into SPIR-V binaries under `build/shaders/`. The build now emits multiple executables:

- `build/Debug/VulkanCubeExample.exe` (or Release) – the original rigid-body cube scene
- `build/Debug/ParticleExample.exe` – a particle fountain demo rendered with GPU billboards sourced from the particle system
- `build/Debug/DeformableExample.exe` – showcases both the suspended cloth and the volumetric "jelly cube" driven by the soft-body simulation stack
- `build/Debug/NBodyExample.exe` – a fully GPU-driven N-body simulation that updates and renders tens of thousands of particles every frame via Vulkan compute shaders
- `build/Debug/LightsExample.exe` – quick-start showcase for the `LightCreateInfo` workflow, highlighting multiple animated lights
- `build/Debug/GlassExample.exe` – glass-like shading demo using fresnel-tinted transparency
- `build/Debug/PhysicsExample.exe` – rigid-body stress test where a wrecking ball topples a cube tower with rotational, elastic collisions and Coulomb friction
- `build/Debug/EmittingBodiesExample.exe` – interactive scene where emissive spheres act as the only light sources, and you can drag them across a matte plane with the mouse
- `build/Debug/DrawOverlayExample.exe` – minimal overlay demo that exercises the new ImGui draw helpers (lines, polylines, filled polygons, rounded rectangles, and custom UI widgets)
- `build/Debug/WindowFeaturesExample.exe` – showcases window modes (windowed, borderless, fullscreen, fixed-size) and transparent framebuffer flag
- `build/Debug/SkyBackgroundExample.exe` – shows how to set a sky/background color via `setSkyColor`
- `build/Debug/SkyImageBackgroundExample.exe` – loads `assets/skys/sky.jpg`, averages its color, and uses it as the background
- `build/Debug/VolumetricFogExample.exe` – height-based volumetric fog with adjustable density, color, and falloff

## Running

After a successful build, launch the sample from the build directory so it can locate the compiled shaders:

```bash
./build/VulkanCubeExample.exe
./build/ParticleExample.exe
./build/NBodyExample.exe
./build/LightsExample.exe
./build/GlassExample.exe
./build/PhysicsExample.exe
./build/EmittingBodiesExample.exe
./build/DrawOverlayExample.exe
./build/WindowFeaturesExample.exe --borderless --transparent
./build/VolumetricFogExample.exe
```

The cube example renders a static ground slab, a falling cube affected by gravity, and a drifting cube with an initial sideways velocity. Lighting combines diffuse and specular terms, feeds from the shared camera buffer, and now includes a shadow map rendered from the active light each frame so objects cast soft shadows. Use the F1–F4 hotkeys to toggle shadows/specular highlights, freeze or animate the light, and visualize individual shading stages (albedo, normals, and shadow factor) directly on screen.

Window features demo flags:

- `--fullscreen` / `--borderless` / `--fixed` – choose mode (default windowed)
- `--transparent` – request a transparent framebuffer
- `--no-resize` – disable resizing (unless fixed-size already forces it)

### Custom meshes & textures

- Drop STL files under `assets/models/` (a sample `demo_cube.stl` is included).
- Drop PPM textures under `assets/textures/` (see the bundled `checker.ppm`).
- Point any `GameObject` at those files via `setMeshResource("assets/models/your_model.stl")` and `setAlbedoTexture("assets/textures/your_texture.ppm")`.
- Optionally tint vertices by calling `setBaseColor(glm::vec3{r, g, b})`.

The renderer caches meshes/textures on first use, uploads them to GPU buffers/images, and binds a per-material descriptor set so every object can carry its own imported geometry and albedo map without touching the Vulkan plumbing.

The particle example instantiates a fountain-style emitter: a `ParticleSystem` spawns and updates ~250 particles per second and the renderer now uploads those particles into a dynamic vertex buffer, drawing additive GPU billboards with a dedicated Vulkan pipeline. You can use the same camera controls to fly through the effect. Validation layers are enabled in Debug builds for both executables; ensure the Vulkan SDK layers are installed when running with `ENABLE_VALIDATION_LAYERS=ON` (default for non-release builds).

### Fonts

- Preferred: place `assets/fonts/Montserrat-Regular.ttf` (also checked at `../assets/...` and `../../assets/...` from the run directory). If missing, it will try `assets/fonts/roboto/Roboto_Condensed-Bold.ttf`, then `assets/fonts/Roboto-Regular.ttf`, before falling back to the default ImGui font. The app logs which font was loaded (or that it fell back) to stderr at startup.

The deformable example now features two soft bodies rendered side-by-side. A suspended cloth panel demonstrates the classic mass–spring grid, while a volumetric "jelly cube" (`SoftBodyVolume`) wobbles, squashes, and bounces on the ground plane using a 3D spring lattice. Both meshes stream their vertices and normals to the renderer each frame so you can inspect different soft-body behaviors under matching lighting and shadow conditions.

The N-body example now spins up a specialized `NBodyEngine` that seeds a galactic disk and hands the renderer both the initial particle states and simulation constants. Vulkan compute shaders integrate softened gravity, apply mild confinement forces, and stream the resulting billboards straight into a GPU-only vertex buffer each frame, enabling far denser particle counts than the previous CPU integrator while keeping the camera fully interactive.

The physics tower example piles 45 lightweight cubes into a 5-story stack, then hurls a high-speed wrecking ball into the structure. The updated physics system now performs sub-stepped integration, computes angular momentum, applies per-body restitution, and now solves static/dynamic friction impulses through a sequential impulse solver so fast impacts transfer spin, stick when necessary, and remain stable without interpenetration even at high velocities.

The emitting bodies example removes the traditional key/fill lights entirely and instead brightens the scene with three emissive spheres hovering above the ground plane. Hold the right mouse button to look around, press the number keys (1–3) to pick an emitter, and drag with the left mouse button to reposition it. Each orb pushes a dynamic point light so you can explore how local lighting affects the plane and nearby geometry in real time.

## Controls

- **W / A / S / D** – Move forward, left, back, right relative to the camera.
- **Space / Left Ctrl** – Move up or down.
- **Shift** – Temporarily boost movement speed.
- **Hold Right Mouse Button** – Lock the cursor and look around with the mouse.
- **F1** – Cycle visualization modes (final lighting → albedo → normals → shadow factor).
- **F2** – Toggle shadowing on/off to compare lit vs. unshadowed shading.
- **F3** – Toggle specular highlights.
- **F4** – Toggle the animated key-light orbit to freeze or resume moving light cues.

### Window configuration

`VulkanRenderer` now exposes a `WindowManager` (`core/WindowManager.hpp`) so you can pick borderless, fullscreen, fixed-size, or transparent windows before starting the render loop:

```cpp
VulkanRenderer renderer(engine);

WindowConfig window{};
window.title = "Particles";
window.mode = WindowMode::Borderless;       // or WindowMode::Fullscreen / FixedSize / Windowed
window.transparentFramebuffer = true;       // request a transparent background
window.resizable = false;                    // fixed size when not fullscreen

renderer.setWindowConfig(window);
renderer.run();
```

Press **Esc** while not in mouse-look to cleanly exit (or call `renderer.requestExit()` from your own code).


## Engine & ECS API

All high-level gameplay state lives inside `vkengine::GameEngine` (`include/engine/GameEngine.hpp`). Under the hood it talks to the new `core::ecs::Registry`, so every `GameObject`, `Light`, and system is backed by components stored in the registry. The concrete engine implements `vkengine::IGameEngine` and exposes:

- `Scene` – wraps the ECS registry, exposes the active `Camera`, and hands out lightweight `GameObject` / `Light` handles that simply reference their entity IDs.
- `GameObject` – manipulates ECS components (`Transform`, `PhysicsProperties`, `ColliderComponent`, `RenderComponent`, etc.) so you can mark objects static/dynamic/collidable while keeping the data oriented for systems.
- `PhysicsSystem` – performs force integration with adaptive sub-stepping, maintains both linear and angular momentum, and resolves collisions via a sequential impulse solver that applies restitution, Coulomb friction, Baumgarte biasing, and positional correction for stacked bodies.
- `Camera` – stores yaw/pitch orientation, perspective parameters, configurable movement/rotation speeds, and accepts smoothed `CameraInput` commands for free-fly control.
- `Light` – another ECS-backed handle that controls position, color, intensity, and enable state via the shared `LightComponent`.
- `LightCreateInfo` – a lightweight struct you can fill out and pass to `Scene::createLight` for one-line point lights with custom names, colors, and intensities.
- `DeformableBody` / `SoftBodyVolume` – optional soft-body components that can be attached to `GameObject` entities so the renderer can stream their vertices each frame.
- `ParticleEmitter` – ECS component that stores emitter simulation state so gameplay code can spawn/drive particle effects via `GameObject::enableParticleEmitter` while the renderer reads back GPU billboards from the shared registry.
- `InputManager` & `InputStateComponent` – a background worker thread consumes GLFW callbacks asynchronously, normalizes keyboard/mouse deltas into `CameraInput`, and publishes every snapshot into the registry so gameplay systems can read low-latency input directly from ECS data.

> Because everything now flows through the ECS registry you can add your own systems/components without touching the renderer—just register new component types, write a system that iterates via `registry.view<YourComponent, Transform>(...)`, and the renderer will already see the updated transforms/meshes.

Create and configure an engine, then pass it to the renderer abstraction:

```cpp
vkengine::GameEngine engine;
auto& scene = engine.scene();
scene.clear();

auto& camera = scene.camera();
camera.setPosition({3.0f, 2.5f, 3.0f});
camera.setYawPitch(glm::radians(-135.0f), glm::radians(-20.0f));
camera.setMovementSpeed(4.0f);

auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
ground.transform().scale = {6.0f, 0.25f, 6.0f};
ground.enableCollider({3.0f, 0.125f, 3.0f}, true);

auto& falling = engine.createObject("Falling", vkengine::MeshType::Cube);
falling.physics().simulate = true;
falling.physics().mass = 2.0f;
falling.physics().staticFriction = 0.7f;
falling.physics().dynamicFriction = 0.55f;
falling.enableCollider({0.5f, 0.5f, 0.5f});

auto& drifter = engine.createObject("Drifter", vkengine::MeshType::Cube);
drifter.transform().position = {-1.5f, 0.0f, 0.0f};
drifter.physics().simulate = true;
drifter.physics().staticFriction = 0.45f;
drifter.physics().dynamicFriction = 0.35f;
drifter.enableCollider({0.5f, 0.5f, 0.5f});

auto& emitter = drifter.enableParticleEmitter("DrifterTrail");
emitter.setOrigin(drifter.transform().position);
emitter.setEmissionRate(60.0f);

vkengine::LightCreateInfo keyLight{};
keyLight.name = "KeyLight";
keyLight.position = {2.0f, 4.0f, 2.0f};
keyLight.color = {1.0f, 0.95f, 0.85f};
keyLight.intensity = 4.0f;
auto& light = scene.createLight(keyLight);

auto& imported = engine.createObject("Imported", vkengine::MeshType::CustomMesh);
imported.setMeshResource("assets/models/demo_cube.stl");
imported.setAlbedoTexture("assets/textures/checker.ppm");
imported.setBaseColor({0.85f, 0.9f, 1.0f});
imported.transform().position = {1.5f, 0.25f, -1.5f};

VulkanCubeApp renderer(engine);
renderer.run();
```

During `drawFrame`, the renderer advances the engine by the frame delta, updates the shared camera uniform buffer, and renders each object by pushing its model matrix to the GPU.
## Project Structure

- `CMakeLists.txt` – Build configuration, dependency fetching, shader compilation, and the new `core` static library that aggregates the ECS, engine, and renderer layers.
- `include/core/ecs/` – Header-only ECS registry (`Entity.hpp`, `Registry.hpp`) plus shared component definitions used by every gameplay system.
- `include/VulkanCubeApp.hpp` – Vulkan renderer declaration that implements `vkengine::IRenderer` and now resides inside the core module.
- `include/engine/*.hpp` – ECS-backed gameplay API (scene, objects, physics, particles, deformables, renderer abstractions).
- `src/engine/*.cpp` – Implementation of the engine, transforms, and systems; all component reads/writes go through the ECS registry.
- `examples/vulkan_cube/VulkanCubeApp.cpp` – Full Vulkan setup, render loop, and integration with the ECS-backed engine.
- `examples/*/main.cpp` – Entry points for the cube, particle, deformable, and N-body demos—all link against the `core` module.
- `shaders/` – GLSL vertex, fragment, and compute shaders compiled at build time (including `shadow.vert` for the depth-only pass).
- `.github/copilot-instructions.md` – Workspace automation checklist.

## Troubleshooting

- **`glslc` not found** – Confirm the Vulkan SDK is installed and `%VULKAN_SDK%/Bin` is available in the environment where you run CMake.
- **Missing validation layers** – Install the SDK's optional components or turn off validation by configuring with `-DENABLE_VALIDATION_LAYERS=OFF`.
- **Link errors for Vulkan** – Ensure the Vulkan SDK's `Lib` directory is visible to your compiler toolchain.

Happy hacking!
