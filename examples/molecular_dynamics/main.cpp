#include "engine/GameEngine.hpp"
#include "engine/MolecularDynamics.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

#include <imgui.h>

#include <algorithm>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cstdlib>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

class MolecularDynamicsEngine : public vkengine::GameEngine {
public:
    enum class AtomPreset {
        Argon = 0,
        Copper = 1
    };

    struct SimulationSettings {
        float timeStep{0.00025f};
        AtomPreset atomPreset{AtomPreset::Argon};
        float baseSigma{1.0f};
        float baseEpsilon{0.0104f};
        float sigma{1.0f};
        float epsilon{0.0104f};
        float damping{0.02f};
        float cutoffRadius{8.5f};
        float minDistance{0.5f};
        float spacing{3.6f};
        float initialVelocityStd{3.0f};
        float maxSpeed{150.0f};
        float temperatureK{298.15f};
        float pressureKPa{101.325f};
        bool useThermo{true};
        int atomsX{5};
        int atomsY{5};
        int atomsZ{4};
    };

    MolecularDynamicsEngine()
    {
        simulation.setBounds({glm::vec3{-7.5f}, glm::vec3{7.5f}});
        applySettingsToSimulation();
        resetSimulation();
    }

    void update(float deltaSeconds) override
    {
        if (running) {
            timeAccumulator += std::max(0.0f, deltaSeconds);
            const float fixedDt = settings.timeStep;
            simulation.setMaxTimeStep(fixedDt);

            std::uint32_t stepsThisFrame = 0;
            while (timeAccumulator >= fixedDt && stepsThisFrame < maxSubstepsPerFrame) {
                simulation.step(fixedDt);
                timeAccumulator -= fixedDt;
                ++stepsThisFrame;
            }
            lastSubsteps = stepsThisFrame;
        } else {
            lastSubsteps = 0;
        }

        syncAtomsToScene();
        vkengine::GameEngine::update(deltaSeconds);
    }

    void startSimulation()
    {
        resetSimulation();
        running = true;
    }

    void resetSimulation()
    {
        running = false;
        timeAccumulator = 0.0f;
        rebuildScene();
    }

    void applyArgonPreset()
    {
        settings.atomPreset = AtomPreset::Argon;
        settings.baseSigma = 3.4f;
        settings.baseEpsilon = 1.0f;
        settings.damping = 0.02f;
        settings.cutoffRadius = 8.5f;
        settings.minDistance = 0.5f;
        settings.spacing = 3.6f;
        settings.initialVelocityStd = 3.0f;
        settings.maxSpeed = 150.0f;
        clampSettings();
        rebuildScene();
    }

    void applyAtomPreset(AtomPreset preset)
    {
        if (running) {
            return;
        }

        const auto material = materialForPreset(preset);
        settings.atomPreset = preset;
        settings.baseSigma = material.sigma;
        settings.baseEpsilon = material.epsilon;
        settings.damping = material.damping;
        settings.cutoffRadius = material.cutoffRadius;
        clampSettings();
        rebuildScene();
    }

    [[nodiscard]] bool isRunning() const noexcept { return running; }
    [[nodiscard]] const auto& parameters() const noexcept { return settings; }

    void updateParameterControls(const std::function<void(SimulationSettings&)>& fn)
    {
        if (running) {
            return;
        }
        fn(settings);
        clampSettings();
        rebuildScene();
    }

    void setTimeStep(float stepSeconds) noexcept
    {
        settings.timeStep = std::clamp(stepSeconds, 1e-5f, 0.003f);
    }

    [[nodiscard]] float timeStep() const noexcept { return settings.timeStep; }

    [[nodiscard]] float currentKineticEnergy() const noexcept { return simulation.kineticEnergy(); }

    void injectEnergy(float energyJoules)
    {
        simulation.addKineticEnergy(std::max(0.0f, energyJoules));
    }

    [[nodiscard]] std::uint32_t substepsLastFrame() const noexcept { return lastSubsteps; }

private:
    static constexpr float kReferenceTemperatureK = 298.15f;

    static vkengine::MdMaterial materialForPreset(AtomPreset preset)
    {
        switch (preset) {
            case AtomPreset::Copper:
                return vkengine::MdMaterial::copper();
            case AtomPreset::Argon:
            default:
                return vkengine::MdMaterial::argon();
        }
    }

    void applyTemperatureAdjustedMaterial()
    {
        const float temperature = std::max(settings.temperatureK, 1.0f);
        const float tempScale = std::max(0.1f, temperature / kReferenceTemperatureK);
        const float sigmaScale = 1.0f + 0.05f * (tempScale - 1.0f);
        const float epsilonScale = 1.0f / tempScale;

        settings.sigma = std::clamp(settings.baseSigma * sigmaScale, 0.5f, 5.0f);
        settings.epsilon = std::clamp(settings.baseEpsilon * epsilonScale, 0.0001f, 0.1f);
    }

    void buildBounds()
    {
        const auto bounds = simulation.bounds();
        const glm::vec3 minB = bounds.minBounds;
        const glm::vec3 maxB = bounds.maxBounds;
        const glm::vec3 extents = maxB - minB;
        const glm::vec3 halfExtents = extents * 0.5f;
        const glm::vec3 center = minB + halfExtents;
        const float thickness = 0.02f;

        const auto addEdge = [this](const glm::vec3& edgeCenter, const glm::vec3& scale) {
            auto& edge = createObject("Bounds_" + std::to_string(boundaryObjects.size()), vkengine::MeshType::WireCubeLines);
            edge.transform().position = edgeCenter;
            edge.transform().scale = scale;
            edge.physics().simulate = false;
            edge.render().mesh = vkengine::MeshType::WireCubeLines;
            edge.setBaseColor(glm::vec3{0.25f});
            boundaryObjects.push_back(&edge);
        };

        const glm::vec3 xScale{halfExtents.x, thickness, thickness};
        for (float y : {minB.y, maxB.y}) {
            for (float z : {minB.z, maxB.z}) {
                addEdge({center.x, y, z}, xScale);
            }
        }

        const glm::vec3 yScale{thickness, halfExtents.y, thickness};
        for (float x : {minB.x, maxB.x}) {
            for (float z : {minB.z, maxB.z}) {
                addEdge({x, center.y, z}, yScale);
            }
        }

        const glm::vec3 zScale{thickness, thickness, halfExtents.z};
        for (float x : {minB.x, maxB.x}) {
            for (float y : {minB.y, maxB.y}) {
                addEdge({x, y, center.z}, zScale);
            }
        }
    }

    void buildAtoms()
    {
        const int atomsX = settings.atomsX;
        const int atomsY = settings.atomsY;
        const int atomsZ = settings.atomsZ;
        const std::size_t targetCount = static_cast<std::size_t>(atomsX * atomsY * atomsZ);

        const auto bounds = simulation.bounds();
        const glm::vec3 minB = bounds.minBounds;
        const glm::vec3 maxB = bounds.maxBounds;
        const glm::vec3 extent = maxB - minB;
        const float minMargin = std::max(0.1f, settings.minDistance);

        auto axisSpacing = [&](int count, float span) {
            if (count <= 1) {
                return 0.0f;
            }
            const float usable = std::max(0.0f, span - 2.0f * minMargin);
            const float maxSpacing = usable / static_cast<float>(count - 1);
            return std::max(0.1f, std::min(settings.spacing, maxSpacing));
        };

        const glm::vec3 spacing{
            axisSpacing(atomsX, extent.x),
            axisSpacing(atomsY, extent.y),
            axisSpacing(atomsZ, extent.z)};

        const glm::vec3 gridSize{
            spacing.x * std::max(0, atomsX - 1),
            spacing.y * std::max(0, atomsY - 1),
            spacing.z * std::max(0, atomsZ - 1)};

        const glm::vec3 center = (minB + maxB) * 0.5f;
        glm::vec3 start = center - 0.5f * gridSize;
        start = glm::max(start, minB + glm::vec3{minMargin});
        start = glm::min(start, maxB - glm::vec3{minMargin} - gridSize);
        const float atomAlpha = 0.8f;
        const glm::vec4 atomColor{0.9f, 0.1f, 0.1f, atomAlpha};

        std::mt19937 rng{std::random_device{}()};
        std::normal_distribution<float> velocityNoise{0.0f, settings.initialVelocityStd};

        for (int z = 0; z < atomsZ; ++z) {
            for (int y = 0; y < atomsY; ++y) {
                for (int x = 0; x < atomsX; ++x) {
                    const glm::vec3 position = start + spacing * glm::vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                    const glm::vec3 velocity{velocityNoise(rng), velocityNoise(rng), velocityNoise(rng)};
                    simulation.addAtom(position, velocity);

                    auto& atomObject = createObject("Cu_" + std::to_string(atomObjects.size()), vkengine::MeshType::Cube);
                    atomObject.transform().position = position;
                    atomObject.transform().scale = glm::vec3{0.32f};
                    atomObject.setMeshResource("assets/models/emitter_sphere.stl");
                    atomObject.setBaseColor(atomColor);
                    atomObject.physics().simulate = false;
                    atomObject.render().mesh = vkengine::MeshType::CustomMesh;
                    atomObjects.push_back(&atomObject);
                }
            }
        }

        if (atomObjects.size() != targetCount) {
            std::cerr << "Warning: expected " << targetCount << " atoms but created " << atomObjects.size() << "\n";
        }
    }

    void syncAtomsToScene()
    {
        const auto& simAtoms = simulation.atoms();
        const std::size_t count = std::min(atomObjects.size(), simAtoms.size());
        for (std::size_t i = 0; i < count; ++i) {
            atomObjects[i]->transform().position = simAtoms[i].position;
        }
    }

    void rebuildScene()
    {
        auto& scn = scene();
        scn.clear();
        atomObjects.clear();
        boundaryObjects.clear();
        simulation.clearAtoms();

        applySettingsToSimulation();
        buildBounds();
        buildAtoms();
        syncAtomsToScene();
    }

    void applySettingsToSimulation()
    {
        clampSettings();

        vkengine::MdMaterial mat = materialForPreset(settings.atomPreset);
        mat.sigma = settings.sigma;
        mat.epsilon = settings.epsilon;
        mat.damping = settings.damping;
        mat.cutoffRadius = settings.cutoffRadius;
        simulation.setMaterial(mat);
        simulation.setMaxTimeStep(settings.timeStep);
        simulation.setMinimumInteractionDistance(settings.minDistance);
        simulation.setMaxSubsteps(maxSubstepsPerFrame);
        simulation.setMaxSpeed(settings.maxSpeed);
    }

    void clampSettings()
    {
        settings.timeStep = std::clamp(settings.timeStep, 1e-5f, 0.003f);
        settings.baseSigma = std::clamp(settings.baseSigma, 0.5f, 5.0f);
        settings.baseEpsilon = std::clamp(settings.baseEpsilon, 0.0001f, 5.0f);
        settings.damping = std::clamp(settings.damping, 0.0f, 0.15f);
        settings.cutoffRadius = std::clamp(settings.cutoffRadius, 1.0f, 15.0f);
        settings.minDistance = std::clamp(settings.minDistance, 0.05f, 2.0f);
        settings.temperatureK = std::clamp(settings.temperatureK, 0.0f, 1200.0f);
        settings.pressureKPa = std::clamp(settings.pressureKPa, 0.0f, 1000.0f);
        if (settings.useThermo) {
            applyThermoDerivedSettings();
        }
        applyTemperatureAdjustedMaterial();
        settings.spacing = std::max(0.5f, settings.spacing);
        settings.initialVelocityStd = std::clamp(settings.initialVelocityStd, 0.0f, 20.0f);
        settings.maxSpeed = std::clamp(settings.maxSpeed, 1.0f, 500.0f);
        settings.atomsX = std::clamp(settings.atomsX, 2, 10);
        settings.atomsY = std::clamp(settings.atomsY, 2, 10);
        settings.atomsZ = std::clamp(settings.atomsZ, 2, 10);
    }

    void applyThermoDerivedSettings()
    {
        const float baseSpacing = 3.6f;
        const float baseVelocityStd = 3.0f;
        const float temperature = std::max(settings.temperatureK, 0.0f);
        const float pressure = std::max(settings.pressureKPa, 0.0f);

        const float tempFactor = std::sqrt(std::max(1.0f, temperature) / kReferenceTemperatureK);
        const float pressureFactor = 1.0f / (1.0f + pressure / 200.0f);

        settings.initialVelocityStd = baseVelocityStd * tempFactor;
        settings.spacing = baseSpacing * pressureFactor;
    }

private:
    SimulationSettings settings;

    vkengine::MolecularDynamicsSimulation simulation{};
    float timeAccumulator{0.0f};
    bool running{false};
    std::uint32_t lastSubsteps{0};
    const std::uint32_t maxSubstepsPerFrame{256};
    std::vector<vkengine::GameObject*> boundaryObjects;
    std::vector<vkengine::GameObject*> atomObjects;
};

} // namespace

int main(int argc, char** argv)
try {
    MolecularDynamicsEngine engine;

    auto& scene = engine.scene();

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 6.0f, 18.0f});
    camera.lookAt(glm::vec3{0.0f, 0.0f, 0.0f});
    camera.setMovementSpeed(10.0f);
    camera.setRotationSpeed(glm::radians(90.0f));

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{10.0f, 14.0f, 10.0f};
    keyLight.color = glm::vec3{1.0f, 0.96f, 0.92f};
    keyLight.intensity = 6.0f;
    scene.createLight(keyLight);

    vkengine::LightCreateInfo fillLight{};
    fillLight.name = "FillLight";
    fillLight.position = glm::vec3{-12.0f, 6.0f, 4.0f};
    fillLight.color = glm::vec3{0.5f, 0.6f, 1.0f};
    fillLight.intensity = 2.5f;
    scene.createLight(fillLight);

    vkengine::LightCreateInfo rimLight{};
    rimLight.name = "RimLight";
    rimLight.position = glm::vec3{0.0f, 8.0f, -12.0f};
    rimLight.color = glm::vec3{0.9f, 0.95f, 1.0f};
    rimLight.intensity = 2.0f;
    scene.createLight(rimLight);

    std::cout << "Molecular dynamics example: configurable Lennard-Jones atoms." << '\n';
    std::cout << "Controls: WASD/Space/Ctrl to move, Right Mouse to look." << '\n';

    VulkanRenderer renderer(engine);
    renderer.setLightAnimationEnabled(false);
    renderer.setSceneControlsEnabled(false);
    renderer.setSkyColor(glm::vec3{0.02f, 0.02f, 0.03f});

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "MolecularDynamicsExample");
    if (capture.enabled) {
        WindowConfig window{};
        window.width = capture.width;
        window.height = capture.height;
        window.headless = true;
        window.resizable = false;
        renderer.setWindowConfig(window);
        std::filesystem::create_directories(capture.outputPath.parent_path());
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    engine.physics().setGravity(glm::vec3{0.0f});

    renderer.setCustomUiCallback([&engine](float /*deltaSeconds*/) {
        auto params = engine.parameters();

        if (ImGui::Begin("Molecular Dynamics")) {
            ImGui::TextUnformatted(engine.isRunning() ? "Status: Running" : "Status: Stopped");
            ImGui::Text("Substeps last frame: %u", engine.substepsLastFrame());

            ImGui::SeparatorText("Atom grid");
            ImGui::BeginDisabled(engine.isRunning());
            if (ImGui::InputInt("Atoms X", &params.atomsX)) {
                engine.updateParameterControls([&](auto& s) { s.atomsX = params.atomsX; });
            }
            if (ImGui::InputInt("Atoms Y", &params.atomsY)) {
                engine.updateParameterControls([&](auto& s) { s.atomsY = params.atomsY; });
            }
            if (ImGui::InputInt("Atoms Z", &params.atomsZ)) {
                engine.updateParameterControls([&](auto& s) { s.atomsZ = params.atomsZ; });
            }
            if (ImGui::SliderFloat("Spacing", &params.spacing, 0.5f, 6.0f, "%.2f")) {
                engine.updateParameterControls([&](auto& s) { s.spacing = params.spacing; });
            }
            if (ImGui::SliderFloat("Init velocity std", &params.initialVelocityStd, 0.0f, 20.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                engine.updateParameterControls([&](auto& s) { s.initialVelocityStd = params.initialVelocityStd; });
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("Material");
            ImGui::BeginDisabled(engine.isRunning());
            int presetIndex = static_cast<int>(params.atomPreset);
            const char* presets[] = {"Argon", "Copper"};
            if (ImGui::Combo("Atom type", &presetIndex, presets, IM_ARRAYSIZE(presets))) {
                engine.applyAtomPreset(static_cast<MolecularDynamicsEngine::AtomPreset>(presetIndex));
                params = engine.parameters();
            }
            if (ImGui::Button("Apply Argon preset")) {
                engine.applyArgonPreset();
                // refresh params snapshot
                params = engine.parameters();
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("(noble gas chamber)");
            if (ImGui::SliderFloat("Sigma (base)", &params.baseSigma, 0.5f, 5.0f, "%.2f")) {
                engine.updateParameterControls([&](auto& s) { s.baseSigma = params.baseSigma; });
            }
            if (ImGui::SliderFloat("Epsilon (base)", &params.baseEpsilon, 0.0001f, 5.0f, "%.4f", ImGuiSliderFlags_Logarithmic)) {
                engine.updateParameterControls([&](auto& s) { s.baseEpsilon = params.baseEpsilon; });
            }
            {
                const float temperature = std::max(params.temperatureK, 1.0f);
                const float tempScale = std::max(0.1f, temperature / 298.15f);
                const float sigmaScale = 1.0f + 0.05f * (tempScale - 1.0f);
                const float epsilonScale = 1.0f / tempScale;
                const float sigmaEffective = std::clamp(params.baseSigma * sigmaScale, 0.5f, 5.0f);
                const float epsilonEffective = std::clamp(params.baseEpsilon * epsilonScale, 0.0001f, 5.0f);
                ImGui::Text("Effective sigma: %.3f", sigmaEffective);
                ImGui::Text("Effective epsilon: %.5f", epsilonEffective);
            }
            if (ImGui::SliderFloat("Damping", &params.damping, 0.0f, 0.15f, "%.3f")) {
                engine.updateParameterControls([&](auto& s) { s.damping = params.damping; });
            }
            if (ImGui::SliderFloat("Cutoff radius", &params.cutoffRadius, 1.0f, 15.0f, "%.2f")) {
                engine.updateParameterControls([&](auto& s) { s.cutoffRadius = params.cutoffRadius; });
            }
            if (ImGui::SliderFloat("Min distance", &params.minDistance, 0.05f, 2.0f, "%.3f")) {
                engine.updateParameterControls([&](auto& s) { s.minDistance = params.minDistance; });
            }
            if (ImGui::SliderFloat("Max speed", &params.maxSpeed, 1.0f, 500.0f, "%.1f", ImGuiSliderFlags_Logarithmic)) {
                engine.updateParameterControls([&](auto& s) { s.maxSpeed = params.maxSpeed; });
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("Thermodynamics");
            ImGui::BeginDisabled(engine.isRunning());
            bool useThermo = params.useThermo;
            if (ImGui::Checkbox("Derive from T/P", &useThermo)) {
                engine.updateParameterControls([&](auto& s) { s.useThermo = useThermo; });
            }
            if (ImGui::SliderFloat("Temperature (K)", &params.temperatureK, 0.0f, 1200.0f, "%.0f")) {
                engine.updateParameterControls([&](auto& s) { s.temperatureK = params.temperatureK; });
            }
            if (ImGui::SliderFloat("Pressure (kPa)", &params.pressureKPa, 0.0f, 1000.0f, "%.1f")) {
                engine.updateParameterControls([&](auto& s) { s.pressureKPa = params.pressureKPa; });
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("Energy");
            ImGui::Text("Kinetic energy: %.6e J", engine.currentKineticEnergy());
            static float energyToAdd = 0.1f;
            ImGui::SliderFloat("Energy to add (J)", &energyToAdd, 1e-6f, 10.0f, "%.6f", ImGuiSliderFlags_Logarithmic);
            if (ImGui::Button("Inject energy")) {
                engine.injectEnergy(energyToAdd);
            }

            ImGui::SeparatorText("Integration");
            float step = params.timeStep;
            if (ImGui::SliderFloat("Time step (s)", &step, 0.00001f, 0.003f, "%.6f", ImGuiSliderFlags_Logarithmic)) {
                engine.setTimeStep(step);
            }

            if (ImGui::Button("Start simulation")) {
                engine.startSimulation();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                engine.resetSimulation();
            }
        }
        ImGui::End();
    });
    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "MolecularDynamics example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
