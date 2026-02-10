/**
 * @file main.cpp
 * @brief Texture Generator – Interactive Demo
 *
 * A GUI application that lets the user choose a procedural-texture pattern,
 * tweak its parameters with ImGui sliders, see a live preview, and export
 * the result to a JPEG file.
 *
 * Run with --batch for headless batch export of all patterns (no GPU needed).
 */

#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "engine/assets/TextureGenerator.hpp"
#include "engine/assets/ImageWriter.hpp"
#include "core/VulkanRenderer.hpp"

#include <glm/glm.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Pattern names (must match vkengine::PatternType order)
// ---------------------------------------------------------------------------
static const char* kPatternNames[] = {
    "Voronoi",
    "Regular Grid",
    "Triangular",
    "Fiber",
    "Bricks",
    "Checkerboard",
    "Noise (Perlin)",
    "Herringbone",
    "Hexagonal",
    "Dots"
};
static constexpr int kPatternCount = static_cast<int>(sizeof(kPatternNames) / sizeof(kPatternNames[0]));

// ---------------------------------------------------------------------------
// Helper – convert Color4 <-> float[4] for ImGui
// ---------------------------------------------------------------------------
static void color4ToFloat(const vkengine::Color4& c, float out[4]) {
    out[0] = c.r / 255.0f;
    out[1] = c.g / 255.0f;
    out[2] = c.b / 255.0f;
    out[3] = c.a / 255.0f;
}

static vkengine::Color4 floatToColor4(const float in[4]) {
    auto cl = [](float v) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
    };
    return {cl(in[0]), cl(in[1]), cl(in[2]), cl(in[3])};
}

// ---------------------------------------------------------------------------
// UI State
// ---------------------------------------------------------------------------
struct AppState {
    vkengine::TextureParams params{};
    int  currentPattern    = 0;
    int  resIndex          = 1;       // 0=256 1=512 2=1024
    bool needsRegenerate   = true;
    bool previewRegistered = false;
    ImTextureID previewTex = 0;
    vkengine::TextureData lastTex{};

    // Export
    char exportPath[256] = "generated_texture.jpg";
    int  jpegQuality      = 95;
    bool exportOk         = false;
    bool exportFailed     = false;

    static constexpr int kResolutions[] = {256, 512, 1024};
};

// ---------------------------------------------------------------------------
// Draw the parameter panel for the selected pattern
// ---------------------------------------------------------------------------
static bool drawPatternParams(vkengine::TextureParams& p) {
    bool changed = false;
    float col[4];

    switch (p.pattern) {
    case vkengine::PatternType::Voronoi: {
        int cells = static_cast<int>(p.voronoiCellCount);
        if (ImGui::SliderInt("Cell Count", &cells, 4, 500)) { p.voronoiCellCount = cells; changed = true; }
        changed |= ImGui::SliderFloat("Jitter", &p.voronoiJitter, 0.0f, 1.0f);
        changed |= ImGui::Checkbox("Show Edges", &p.voronoiShowEdges);
        if (p.voronoiShowEdges) {
            changed |= ImGui::SliderFloat("Edge Width", &p.voronoiEdgeWidth, 0.5f, 10.0f);
            color4ToFloat(p.voronoiEdgeColor, col);
            if (ImGui::ColorEdit4("Edge Color", col)) { p.voronoiEdgeColor = floatToColor4(col); changed = true; }
        }
        color4ToFloat(p.voronoiColor1, col);
        if (ImGui::ColorEdit4("Color 1", col)) { p.voronoiColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.voronoiColor2, col);
        if (ImGui::ColorEdit4("Color 2", col)) { p.voronoiColor2 = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::RegularGrid: {
        int cols = static_cast<int>(p.gridCols), rows = static_cast<int>(p.gridRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 64)) { p.gridCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 64)) { p.gridRows = rows; changed = true; }
        changed |= ImGui::SliderFloat("Line Width", &p.gridLineWidth, 0.5f, 10.0f);
        color4ToFloat(p.gridLineColor, col);
        if (ImGui::ColorEdit4("Line Color", col)) { p.gridLineColor = floatToColor4(col); changed = true; }
        color4ToFloat(p.gridFillColor, col);
        if (ImGui::ColorEdit4("Fill Color", col)) { p.gridFillColor = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Triangular: {
        int cols = static_cast<int>(p.triCols), rows = static_cast<int>(p.triRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 64)) { p.triCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 64)) { p.triRows = rows; changed = true; }
        changed |= ImGui::SliderFloat("Line Width", &p.triLineWidth, 0.5f, 8.0f);
        color4ToFloat(p.triColor1, col);
        if (ImGui::ColorEdit4("Color 1", col)) { p.triColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.triColor2, col);
        if (ImGui::ColorEdit4("Color 2", col)) { p.triColor2 = floatToColor4(col); changed = true; }
        color4ToFloat(p.triLineColor, col);
        if (ImGui::ColorEdit4("Line Color", col)) { p.triLineColor = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Fiber: {
        int count = static_cast<int>(p.fiberCount);
        if (ImGui::SliderInt("Fiber Count", &count, 4, 200)) { p.fiberCount = count; changed = true; }
        changed |= ImGui::SliderFloat("Width", &p.fiberWidth, 0.5f, 12.0f);
        changed |= ImGui::SliderFloat("Twist (freq)", &p.fiberTwist, 0.0f, 20.0f);
        changed |= ImGui::SliderFloat("Amplitude", &p.fiberAmplitude, 0.0f, 80.0f);
        color4ToFloat(p.fiberColor1, col);
        if (ImGui::ColorEdit4("Fiber Color 1", col)) { p.fiberColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.fiberColor2, col);
        if (ImGui::ColorEdit4("Fiber Color 2", col)) { p.fiberColor2 = floatToColor4(col); changed = true; }
        color4ToFloat(p.fiberBgColor, col);
        if (ImGui::ColorEdit4("Background", col)) { p.fiberBgColor = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Bricks: {
        int cols = static_cast<int>(p.brickCols), rows = static_cast<int>(p.brickRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 32)) { p.brickCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 64)) { p.brickRows = rows; changed = true; }
        changed |= ImGui::SliderFloat("Mortar Size", &p.brickMortar, 0.0f, 15.0f);
        color4ToFloat(p.brickColor1, col);
        if (ImGui::ColorEdit4("Brick Color 1", col)) { p.brickColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.brickColor2, col);
        if (ImGui::ColorEdit4("Brick Color 2", col)) { p.brickColor2 = floatToColor4(col); changed = true; }
        color4ToFloat(p.brickMortarColor, col);
        if (ImGui::ColorEdit4("Mortar Color", col)) { p.brickMortarColor = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Checkerboard: {
        int cols = static_cast<int>(p.checkerCols), rows = static_cast<int>(p.checkerRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 64)) { p.checkerCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 64)) { p.checkerRows = rows; changed = true; }
        color4ToFloat(p.checkerColor1, col);
        if (ImGui::ColorEdit4("Color 1", col)) { p.checkerColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.checkerColor2, col);
        if (ImGui::ColorEdit4("Color 2", col)) { p.checkerColor2 = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Noise: {
        changed |= ImGui::SliderFloat("Scale", &p.noiseScale, 0.5f, 30.0f);
        int oct = static_cast<int>(p.noiseOctaves);
        if (ImGui::SliderInt("Octaves", &oct, 1, 10)) { p.noiseOctaves = oct; changed = true; }
        changed |= ImGui::SliderFloat("Lacunarity", &p.noiseLacunarity, 1.0f, 4.0f);
        changed |= ImGui::SliderFloat("Persistence", &p.noisePersistence, 0.1f, 1.0f);
        color4ToFloat(p.noiseColor1, col);
        if (ImGui::ColorEdit4("Color 1", col)) { p.noiseColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.noiseColor2, col);
        if (ImGui::ColorEdit4("Color 2", col)) { p.noiseColor2 = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Herringbone: {
        int cols = static_cast<int>(p.herringboneCols), rows = static_cast<int>(p.herringboneRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 32)) { p.herringboneCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 64)) { p.herringboneRows = rows; changed = true; }
        changed |= ImGui::SliderFloat("Mortar Size", &p.herringboneMortar, 0.0f, 10.0f);
        color4ToFloat(p.herringboneColor1, col);
        if (ImGui::ColorEdit4("Color 1", col)) { p.herringboneColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.herringboneColor2, col);
        if (ImGui::ColorEdit4("Color 2", col)) { p.herringboneColor2 = floatToColor4(col); changed = true; }
        color4ToFloat(p.herringboneMortarColor, col);
        if (ImGui::ColorEdit4("Mortar Color", col)) { p.herringboneMortarColor = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Hexagonal: {
        int cols = static_cast<int>(p.hexCols), rows = static_cast<int>(p.hexRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 32)) { p.hexCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 32)) { p.hexRows = rows; changed = true; }
        changed |= ImGui::SliderFloat("Line Width", &p.hexLineWidth, 0.5f, 8.0f);
        color4ToFloat(p.hexLineColor, col);
        if (ImGui::ColorEdit4("Line Color", col)) { p.hexLineColor = floatToColor4(col); changed = true; }
        color4ToFloat(p.hexFillColor1, col);
        if (ImGui::ColorEdit4("Fill Color 1", col)) { p.hexFillColor1 = floatToColor4(col); changed = true; }
        color4ToFloat(p.hexFillColor2, col);
        if (ImGui::ColorEdit4("Fill Color 2", col)) { p.hexFillColor2 = floatToColor4(col); changed = true; }
        break;
    }
    case vkengine::PatternType::Dots: {
        int cols = static_cast<int>(p.dotCols), rows = static_cast<int>(p.dotRows);
        if (ImGui::SliderInt("Columns", &cols, 1, 64)) { p.dotCols = cols; changed = true; }
        if (ImGui::SliderInt("Rows", &rows, 1, 64)) { p.dotRows = rows; changed = true; }
        changed |= ImGui::SliderFloat("Dot Radius", &p.dotRadius, 0.05f, 0.5f);
        color4ToFloat(p.dotColor, col);
        if (ImGui::ColorEdit4("Dot Color", col)) { p.dotColor = floatToColor4(col); changed = true; }
        color4ToFloat(p.dotBgColor, col);
        if (ImGui::ColorEdit4("Background", col)) { p.dotBgColor = floatToColor4(col); changed = true; }
        break;
    }
    }
    return changed;
}

// ---------------------------------------------------------------------------
// Main UI callback (runs every frame inside the ImGui render phase)
// ---------------------------------------------------------------------------
static void renderUI(VulkanRenderer& renderer, AppState& state) {

    // ---- Controls panel (left side) ----
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 680), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Texture Generator")) {
        // Pattern selector
        if (ImGui::Combo("Pattern", &state.currentPattern, kPatternNames, kPatternCount)) {
            state.params.pattern = static_cast<vkengine::PatternType>(state.currentPattern);
            state.needsRegenerate = true;
        }

        ImGui::Separator();

        // Resolution
        const char* resLabels[] = {"256x256", "512x512", "1024x1024"};
        if (ImGui::Combo("Resolution", &state.resIndex, resLabels, 3)) {
            int res = AppState::kResolutions[state.resIndex];
            state.params.width  = static_cast<uint32_t>(res);
            state.params.height = static_cast<uint32_t>(res);
            state.needsRegenerate = true;
        }

        // Seed
        int seed = static_cast<int>(state.params.seed);
        if (ImGui::InputInt("Seed", &seed)) {
            state.params.seed = static_cast<uint32_t>(seed);
            state.needsRegenerate = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Random")) {
            state.params.seed = static_cast<uint32_t>(rand());
            state.needsRegenerate = true;
        }

        ImGui::Separator();

        // Pattern-specific parameters
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (drawPatternParams(state.params)) {
                state.needsRegenerate = true;
            }
        }

        ImGui::Separator();

        // Generate button
        if (ImGui::Button("Regenerate", ImVec2(-1, 30))) {
            state.needsRegenerate = true;
        }

        ImGui::Spacing();

        // ---- Export section ----
        if (ImGui::CollapsingHeader("Export to JPEG", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("File Path", state.exportPath, sizeof(state.exportPath));
            ImGui::SliderInt("JPEG Quality", &state.jpegQuality, 10, 100);

            if (ImGui::Button("Export", ImVec2(-1, 28))) {
                fs::path outPath(state.exportPath);
                if (outPath.has_parent_path()) {
                    fs::create_directories(outPath.parent_path());
                }
                bool ok = vkengine::writeJpeg(outPath,
                    static_cast<int>(state.lastTex.width),
                    static_cast<int>(state.lastTex.height),
                    state.lastTex.pixels,
                    state.jpegQuality);
                state.exportOk     =  ok;
                state.exportFailed = !ok;
            }

            if (state.exportOk) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Exported successfully!");
            }
            if (state.exportFailed) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Export failed.");
            }
        }
    }
    ImGui::End();

    // ---- Regenerate texture if needed ----
    if (state.needsRegenerate) {
        state.needsRegenerate = false;
        state.exportOk = false;
        state.exportFailed = false;

        state.lastTex = vkengine::generateTexture(state.params);

        if (state.previewRegistered) {
            renderer.destroyImGuiTexture(state.previewTex);
        }
        state.previewTex = renderer.registerImGuiTexture(state.lastTex);
        state.previewRegistered = true;
    }

    // ---- Preview window (right side) ----
    ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(540, 560), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Preview")) {
        if (state.previewRegistered && state.previewTex) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float texW = static_cast<float>(state.lastTex.width);
            float texH = static_cast<float>(state.lastTex.height);
            float scale = std::min(avail.x / texW, avail.y / texH);
            if (scale > 0.0f) {
                ImVec2 imgSize(texW * scale, texH * scale);
                ImGui::Image(state.previewTex, imgSize);
            }
        } else {
            ImGui::Text("Generating...");
        }
        ImGui::Text("Size: %u x %u  |  Pattern: %s",
                     state.lastTex.width, state.lastTex.height,
                     kPatternNames[state.currentPattern]);
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Headless batch export (no GUI, just files)
// ---------------------------------------------------------------------------
static int runBatchExport() {
    const char* labels[] = {
        "voronoi", "regular_grid", "triangular", "fiber", "bricks",
        "checkerboard", "noise", "herringbone", "hexagonal", "dots"
    };

    fs::path outDir = fs::current_path() / "generated_textures";
    fs::create_directories(outDir);

    std::cout << "=== TextureGenExample (batch) ===\n"
              << "Output: " << outDir.string() << "\n\n";

    int ok = 0, fail = 0;
    for (int i = 0; i < kPatternCount; ++i) {
        vkengine::TextureParams p;
        p.width = 512; p.height = 512;
        p.pattern = static_cast<vkengine::PatternType>(i);
        fs::path path = outDir / (std::string(labels[i]) + ".jpg");
        std::cout << "  " << labels[i] << "... ";
        if (vkengine::generateTextureToJpeg(p, path)) { std::cout << "OK\n"; ++ok; }
        else                                           { std::cout << "FAIL\n"; ++fail; }
    }
    std::cout << "\nDone: " << ok << " ok, " << fail << " failed.\n";
    return fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // --batch: headless export of all patterns (no GPU needed beyond WIC).
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--batch") == 0)
            return runBatchExport();
    }

    try {
        // Minimal engine – we just need a window + ImGui overlay.
        vkengine::GameEngine engine{};
        auto& scene = engine.scene();
        scene.clear();

        auto& ground = engine.createObject("ground", vkengine::MeshType::Cube);
        ground.transform().position = glm::vec3{0.0f, -0.6f, 0.0f};
        ground.transform().scale    = glm::vec3{5.0f, 0.1f, 5.0f};

        auto& camera = scene.camera();
        camera.setPosition(glm::vec3{0.0f, 1.0f, 3.0f});
        camera.setYawPitch(glm::radians(-90.0f), glm::radians(-10.0f));

        vkengine::LightCreateInfo light{};
        light.name      = "Key";
        light.position  = glm::vec3{3.0f, 5.0f, 3.0f};
        light.color     = glm::vec3{1.0f, 0.95f, 0.9f};
        light.intensity = 1.5f;
        scene.createLight(light);

        VulkanRenderer renderer{engine};

        WindowConfig wc{};
        wc.title     = "EVE - Texture Generator";
        wc.width     = 920;
        wc.height    = 700;
        wc.resizable = true;
        renderer.setWindowConfig(wc);

        // Headless single-frame capture (for CI).
        const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "TextureGenExample");
        if (capture.enabled) {
            WindowConfig hw{};
            hw.width     = capture.width;
            hw.height    = capture.height;
            hw.headless  = true;
            hw.resizable = false;
            renderer.setWindowConfig(hw);
            renderer.setSceneControlsEnabled(false);
            renderer.setLightAnimationEnabled(false);
            fs::create_directories(capture.outputPath.parent_path());
            return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        // Shared app state.
        AppState state{};
        state.params.width  = 512;
        state.params.height = 512;

        renderer.setCustomUiCallback([&renderer, &state](float /*dt*/) {
            renderUI(renderer, state);
        });

        std::cout << "=== EVE Texture Generator ===\n"
                  << "Use the ImGui panels to select patterns and tweak parameters.\n"
                  << "Press ESC to exit.\n";

        renderer.run();

        if (state.previewRegistered) {
            renderer.destroyImGuiTexture(state.previewTex);
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
