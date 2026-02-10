#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "engine/HeadlessCapture.hpp"

namespace {

TEST(RendererTests, HeadlessCaptureAllExamples)
{
#if defined(_WIN32)
    _putenv_s("VKENGINE_HEADLESS_TEST", "1");
#else
    setenv("VKENGINE_HEADLESS_TEST", "1", 1);
#endif
    const char* enable = std::getenv("VKENGINE_HEADLESS_TEST");
    if (!enable || std::string(enable) != "1") {
        GTEST_SKIP() << "Set VKENGINE_HEADLESS_TEST=1 to enable headless capture.";
    }

    const auto root = vkengine::resolveRepoRoot();
    auto buildDir = std::filesystem::current_path();
    if (buildDir.filename() == "tests") {
        buildDir = buildDir.parent_path();
    }
    const auto debugDir = buildDir / "Debug";
    const auto releaseDir = buildDir / "Release";
    const auto exeDir = std::filesystem::exists(debugDir) ? debugDir : releaseDir;

    const std::vector<std::string> examples = {
        "CubeExample",
        "DeformableExample",
        "DrawOverlayExample",
        "EmittingBodiesExample",
        "LatticeExample",
        "LightsExample",
        "ReflectionsExample",
        "MolecularDynamicsExample",
        "NBodyExample",
        "ParticleExample",
        "PhysicsExample",
        "SkyImageBackgroundExample",
        "WindowFeaturesExample"
    };

    const auto outputDir = root / "tests" / "test_results" / "renders";
    std::filesystem::create_directories(outputDir);

    for (const auto& name : examples) {
        const auto exePath = exeDir / (name + ".exe");
        ASSERT_TRUE(std::filesystem::exists(exePath)) << "Missing executable: " << exePath.string();
        const auto outputPath = outputDir / (name + "_first_frame.jpg");
    #if defined(_WIN32)
        const std::wstring inner = L"cd /d \"" + root.wstring() + L"\" && \"" + exePath.wstring() + L"\" --headless --capture \"" + outputPath.wstring() + L"\"";
        const std::wstring command = L"cmd.exe /c \"" + inner + L"\"";
        const int code = _wsystem(command.c_str());
    #else
        const std::string command = "cd \"" + root.string() + "\" && \"" + exePath.string() + "\" --headless --capture \"" + outputPath.string() + "\"";
        const int code = std::system(command.c_str());
    #endif
        EXPECT_EQ(code, 0) << "Capture failed for " << name;
        EXPECT_TRUE(std::filesystem::exists(outputPath)) << "Missing output: " << outputPath.string();
    }
}

} // namespace
