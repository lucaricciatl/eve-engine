#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <string>

#include <glm/glm.hpp>

#include "core/VulkanRenderer.hpp"
#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"

namespace {

struct TimingResult {
    bool success{false};
    std::chrono::steady_clock::duration elapsed{};
};

struct MetricsState {
    std::mutex mutex;
    std::map<std::string, double> values;
};

MetricsState& metricsState() {
    static MetricsState state;
    return state;
}

std::filesystem::path metricsOutputPath() {
    const auto root = vkengine::resolveRepoRoot();
    return root / "tests" / "test_results" / "perf" / "performance.json";
}

void writeMetricsFileLocked(const std::map<std::string, double>& values) {
    const auto path = metricsOutputPath();
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return;
    }

    out << "{\n";
    out << "  \"metrics\": {\n";
    std::size_t index = 0;
    for (const auto& [key, value] : values) {
        out << "    \"" << key << "\": " << std::fixed << std::setprecision(3) << value;
        if (++index < values.size()) {
            out << ',';
        }
        out << "\n";
    }
    out << "  }\n";
    out << "}\n";
}

void recordMetric(const std::string& name, double value) {
    auto& state = metricsState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.values[name] = value;
    writeMetricsFileLocked(state.values);
}

float envFloatOrDefault(const char* name, float fallback) {
    const char* value = std::getenv(name);
    if (!value) {
        return fallback;
    }
    try {
        return std::stof(value);
    } catch (...) {
        return fallback;
    }
}

double measureMillis(const std::function<void()>& action) {
    const auto start = std::chrono::steady_clock::now();
    action();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start).count();
}

double averageMillis(std::size_t runs, const std::function<void()>& action) {
    if (runs == 0) {
        return 0.0;
    }
    double total = 0.0;
    for (std::size_t i = 0; i < runs; ++i) {
        total += measureMillis(action);
    }
    return total / static_cast<double>(runs);
}

void configureSceneWithCubes(vkengine::GameEngine& engine, std::size_t count) {
    auto& scene = engine.scene();
    scene.clear();

    vkengine::LightCreateInfo lightInfo{};
    lightInfo.name = "PerfLight";
    lightInfo.position = glm::vec3{5.0f, 6.0f, 5.0f};
    lightInfo.color = glm::vec3{1.0f, 0.95f, 0.9f};
    lightInfo.intensity = 1.2f;
    scene.createLight(lightInfo);

    const std::size_t grid = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
    const float spacing = 1.25f;

    std::size_t created = 0;
    for (std::size_t z = 0; z < grid && created < count; ++z) {
        for (std::size_t x = 0; x < grid && created < count; ++x) {
            auto& object = scene.createObject("", vkengine::MeshType::Cube);
            object.transform().position = glm::vec3{(static_cast<float>(x) - grid * 0.5f) * spacing,
                                                   0.0f,
                                                   (static_cast<float>(z) - grid * 0.5f) * spacing};
            ++created;
        }
    }

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 4.5f, 10.0f});
    camera.lookAt(glm::vec3{0.0f, 0.0f, 0.0f});

    return;
}

TimingResult measureFrames(VulkanRenderer& renderer,
                           const std::filesystem::path& outputPath,
                           uint32_t frames,
                           float fixedDelta) {
    const auto start = std::chrono::steady_clock::now();
    const bool success = renderer.renderFrameToJpegAt(outputPath, frames, fixedDelta);
    const auto end = std::chrono::steady_clock::now();
    return {success, end - start};
}

void configureRenderer(VulkanRenderer& renderer) {
    WindowConfig config{};
    config.width = 640;
    config.height = 360;
    config.headless = true;
    config.resizable = false;
    renderer.setWindowConfig(config);
    renderer.setUiEnabled(false);
    renderer.setSceneControlsEnabled(false);
    renderer.setLightAnimationEnabled(false);
    renderer.setBlurEnabled(false);
    renderer.setFogEnabled(false);
}

TEST(RenderPerformanceTests, HeadlessFrameTimingSmallScene) {
    vkengine::GameEngine engine;
    configureSceneWithCubes(engine, 1);

    const auto root = vkengine::resolveRepoRoot();
    const auto outDir = root / "tests" / "test_results" / "perf";
    std::filesystem::create_directories(outDir);

    const auto singleFramePath = outDir / "perf_single_small.jpg";
    const auto multiFramePath = outDir / "perf_multi_small.jpg";

    VulkanRenderer rendererSingle(engine);
    configureRenderer(rendererSingle);
    const TimingResult oneFrame = measureFrames(rendererSingle, singleFramePath, 1, 1.0f / 60.0f);
    ASSERT_TRUE(oneFrame.success) << "Failed to render single frame.";

    const uint32_t targetFrames = 30;
    VulkanRenderer rendererMulti(engine);
    configureRenderer(rendererMulti);
    const TimingResult manyFrames = measureFrames(rendererMulti, multiFramePath, targetFrames, 1.0f / 60.0f);
    ASSERT_TRUE(manyFrames.success) << "Failed to render multi-frame capture.";

    const auto elapsedOneMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(oneFrame.elapsed).count();
    const auto elapsedManyMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(manyFrames.elapsed).count();

    double avgFrameMs = elapsedManyMs / static_cast<double>(targetFrames);
    if (elapsedManyMs > elapsedOneMs && targetFrames > 1) {
        avgFrameMs = (elapsedManyMs - elapsedOneMs) / static_cast<double>(targetFrames - 1);
    }

    RecordProperty("avg_frame_ms_small", avgFrameMs);
    recordMetric("avg_frame_ms_small", avgFrameMs);

    const std::size_t runs = 3;
    double avgFrameMsOverRuns = 0.0;
    for (std::size_t i = 0; i < runs; ++i) {
        VulkanRenderer rendererRepeat(engine);
        configureRenderer(rendererRepeat);
        const auto repeatPath = outDir / ("perf_multi_small_run_" + std::to_string(i) + ".jpg");
        const TimingResult repeat = measureFrames(rendererRepeat, repeatPath, targetFrames, 1.0f / 60.0f);
        ASSERT_TRUE(repeat.success) << "Failed repeat render run.";
        const auto repeatMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(repeat.elapsed).count();
        double repeatAvg = repeatMs / static_cast<double>(targetFrames);
        avgFrameMsOverRuns += repeatAvg;
    }
    avgFrameMsOverRuns /= static_cast<double>(runs);
    RecordProperty("avg_frame_ms_small_runs", avgFrameMsOverRuns);
    recordMetric("avg_frame_ms_small_runs", avgFrameMsOverRuns);

    const float thresholdMs = envFloatOrDefault("VKENGINE_RENDER_FRAME_MS", 200.0f);
    EXPECT_LE(avgFrameMs, thresholdMs) << "Average frame time exceeded threshold."
                                       << " avg=" << avgFrameMs << "ms threshold=" << thresholdMs << "ms";
}

TEST(RenderPerformanceTests, HeadlessFrameTimingMediumScene) {
    vkengine::GameEngine engine;
    configureSceneWithCubes(engine, 128);

    const auto root = vkengine::resolveRepoRoot();
    const auto outDir = root / "tests" / "test_results" / "perf";
    std::filesystem::create_directories(outDir);

    const auto singleFramePath = outDir / "perf_single_medium.jpg";
    const auto multiFramePath = outDir / "perf_multi_medium.jpg";

    VulkanRenderer rendererSingle(engine);
    configureRenderer(rendererSingle);
    const TimingResult oneFrame = measureFrames(rendererSingle, singleFramePath, 1, 1.0f / 60.0f);
    ASSERT_TRUE(oneFrame.success) << "Failed to render single frame.";

    const uint32_t targetFrames = 30;
    VulkanRenderer rendererMulti(engine);
    configureRenderer(rendererMulti);
    const TimingResult manyFrames = measureFrames(rendererMulti, multiFramePath, targetFrames, 1.0f / 60.0f);
    ASSERT_TRUE(manyFrames.success) << "Failed to render multi-frame capture.";

    const auto elapsedOneMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(oneFrame.elapsed).count();
    const auto elapsedManyMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(manyFrames.elapsed).count();

    double avgFrameMs = elapsedManyMs / static_cast<double>(targetFrames);
    if (elapsedManyMs > elapsedOneMs && targetFrames > 1) {
        avgFrameMs = (elapsedManyMs - elapsedOneMs) / static_cast<double>(targetFrames - 1);
    }

    RecordProperty("avg_frame_ms_medium", avgFrameMs);
    recordMetric("avg_frame_ms_medium", avgFrameMs);

    const std::size_t runs = 3;
    double avgFrameMsOverRuns = 0.0;
    for (std::size_t i = 0; i < runs; ++i) {
        VulkanRenderer rendererRepeat(engine);
        configureRenderer(rendererRepeat);
        const auto repeatPath = outDir / ("perf_multi_medium_run_" + std::to_string(i) + ".jpg");
        const TimingResult repeat = measureFrames(rendererRepeat, repeatPath, targetFrames, 1.0f / 60.0f);
        ASSERT_TRUE(repeat.success) << "Failed repeat render run.";
        const auto repeatMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(repeat.elapsed).count();
        double repeatAvg = repeatMs / static_cast<double>(targetFrames);
        avgFrameMsOverRuns += repeatAvg;
    }
    avgFrameMsOverRuns /= static_cast<double>(runs);
    RecordProperty("avg_frame_ms_medium_runs", avgFrameMsOverRuns);
    recordMetric("avg_frame_ms_medium_runs", avgFrameMsOverRuns);

    const float thresholdMs = envFloatOrDefault("VKENGINE_RENDER_FRAME_MS_MEDIUM", 350.0f);
    EXPECT_LE(avgFrameMs, thresholdMs) << "Average frame time exceeded threshold."
                                       << " avg=" << avgFrameMs << "ms threshold=" << thresholdMs << "ms";
}

TEST(PerformanceTests, ObjectInstantiationPipeline) {
    vkengine::GameEngine engine;
    auto& scene = engine.scene();
    scene.clear();

    constexpr std::size_t kObjectCount = 2000;
    const double elapsedMs = measureMillis([&]() {
        scene.createObjects(kObjectCount, vkengine::MeshType::Cube);
    });

    constexpr std::size_t runs = 5;
    const double averageMs = averageMillis(runs, [&]() {
        scene.clear();
        scene.createObjects(kObjectCount, vkengine::MeshType::Cube);
    });

    const double perSecond = (kObjectCount / std::max(1.0, elapsedMs)) * 1000.0;
    RecordProperty("object_instantiation_ms", elapsedMs);
    RecordProperty("object_instantiation_per_sec", perSecond);
    RecordProperty("object_instantiation_avg_ms", averageMs);
    recordMetric("object_instantiation_ms", elapsedMs);
    recordMetric("object_instantiation_per_sec", perSecond);
    recordMetric("object_instantiation_avg_ms", averageMs);

    const float thresholdMs = envFloatOrDefault("VKENGINE_OBJECT_INSTANTIATION_MS", 1000.0f);
    EXPECT_LE(elapsedMs, thresholdMs) << "Object instantiation exceeded threshold."
                                      << " ms=" << elapsedMs << " threshold=" << thresholdMs;
}

TEST(PerformanceTests, ObjectInstantiationMultiSize) {
    vkengine::GameEngine engine;
    auto& scene = engine.scene();
    scene.clear();

    const std::array<std::size_t, 3> counts = {500, 2000, 5000};
    for (auto count : counts) {
        const double averageMs = averageMillis(3, [&]() {
            scene.clear();
            scene.createObjects(count, vkengine::MeshType::Cube);
        });

        const double perSecond = (count / std::max(1.0, averageMs)) * 1000.0;
        const std::string keyMs = "object_instantiation_avg_ms_" + std::to_string(count);
        const std::string keyRate = "object_instantiation_per_sec_" + std::to_string(count);
        RecordProperty(keyMs, averageMs);
        RecordProperty(keyRate, perSecond);
        recordMetric(keyMs, averageMs);
        recordMetric(keyRate, perSecond);
    }
}

TEST(RenderPerformanceTests, HeadlessFrameTimingLargeScene) {
    vkengine::GameEngine engine;
    configureSceneWithCubes(engine, 512);
    VulkanRenderer rendererSingle(engine);
    configureRenderer(rendererSingle);

    const auto root = vkengine::resolveRepoRoot();
    const auto outDir = root / "tests" / "test_results" / "perf";
    std::filesystem::create_directories(outDir);

    const auto singleFramePath = outDir / "perf_single_large.jpg";
    const auto multiFramePath = outDir / "perf_multi_large.jpg";

    const TimingResult oneFrame = measureFrames(rendererSingle, singleFramePath, 1, 1.0f / 60.0f);
    ASSERT_TRUE(oneFrame.success) << "Failed to render single frame.";

    const uint32_t targetFrames = 30;
    VulkanRenderer rendererMulti(engine);
    configureRenderer(rendererMulti);
    const TimingResult manyFrames = measureFrames(rendererMulti, multiFramePath, targetFrames, 1.0f / 60.0f);
    ASSERT_TRUE(manyFrames.success) << "Failed to render multi-frame capture.";

    const auto elapsedManyMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(manyFrames.elapsed).count();
    double avgFrameMs = elapsedManyMs / static_cast<double>(targetFrames);

    RecordProperty("avg_frame_ms_large", avgFrameMs);
    recordMetric("avg_frame_ms_large", avgFrameMs);

    const float thresholdMs = envFloatOrDefault("VKENGINE_RENDER_FRAME_MS_LARGE", 600.0f);
    EXPECT_LE(avgFrameMs, thresholdMs) << "Average frame time exceeded threshold."
                                       << " avg=" << avgFrameMs << "ms threshold=" << thresholdMs << "ms";
}

TEST(PerformanceTests, LightCreationPipeline) {
    vkengine::GameEngine engine;
    auto& scene = engine.scene();
    scene.clear();

    constexpr std::size_t kLightCount = 1000;
    const double elapsedMs = measureMillis([&]() {
        for (std::size_t i = 0; i < kLightCount; ++i) {
            vkengine::LightCreateInfo info{};
            info.name = "PerfLight_" + std::to_string(i);
            info.position = glm::vec3{static_cast<float>(i % 32), 3.0f, static_cast<float>(i / 32)};
            info.intensity = 1.0f + static_cast<float>(i % 5) * 0.1f;
            scene.createLight(info);
        }
    });

    const double perSecond = (kLightCount / std::max(1.0, elapsedMs)) * 1000.0;
    RecordProperty("light_creation_ms", elapsedMs);
    RecordProperty("light_creation_per_sec", perSecond);
    recordMetric("light_creation_ms", elapsedMs);
    recordMetric("light_creation_per_sec", perSecond);

    const float thresholdMs = envFloatOrDefault("VKENGINE_LIGHT_CREATION_MS", 1000.0f);
    EXPECT_LE(elapsedMs, thresholdMs) << "Light creation exceeded threshold."
                                      << " ms=" << elapsedMs << " threshold=" << thresholdMs;
}

TEST(PerformanceTests, MaterialPipeline) {
    vkengine::GameEngine engine;
    auto& scene = engine.scene();
    scene.clear();

    constexpr std::size_t kMaterialCount = 500;
    constexpr std::size_t kObjectCount = 500;

    const double elapsedMs = measureMillis([&]() {
        for (std::size_t i = 0; i < kMaterialCount; ++i) {
            vkengine::Material material{};
            material.name = "PerfMat_" + std::to_string(i);
            material.baseColor = glm::vec4{0.1f + 0.001f * static_cast<float>(i), 0.2f, 0.3f, 1.0f};
            material.metallic = 0.2f;
            material.roughness = 0.6f;
            scene.materials().add(material);
        }

        for (std::size_t i = 0; i < kObjectCount; ++i) {
            auto& object = scene.createObject("", vkengine::MeshType::Cube);
            const std::string materialName = "PerfMat_" + std::to_string(i % kMaterialCount);
            object.setMaterial(materialName);
        }
    });

    const double perSecond = ((kMaterialCount + kObjectCount) / std::max(1.0, elapsedMs)) * 1000.0;
    RecordProperty("material_pipeline_ms", elapsedMs);
    RecordProperty("material_pipeline_ops_per_sec", perSecond);
    recordMetric("material_pipeline_ms", elapsedMs);
    recordMetric("material_pipeline_ops_per_sec", perSecond);

    const float thresholdMs = envFloatOrDefault("VKENGINE_MATERIAL_PIPELINE_MS", 1200.0f);
    EXPECT_LE(elapsedMs, thresholdMs) << "Material pipeline exceeded threshold."
                                      << " ms=" << elapsedMs << " threshold=" << thresholdMs;
}

} // namespace
