#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <excpt.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "engine/HeadlessCapture.hpp"
#include "engine/assets/ImageWriter.hpp"
#include "engine/assets/TextureLoader.hpp"

namespace {

struct ImageDiffStats {
    double meanError{0.0};
    double maxError{0.0};
    std::uint32_t width{0};
    std::uint32_t height{0};
};

ImageDiffStats computeImageDiff(const vkengine::TextureData& baseline,
                                const vkengine::TextureData& actual,
                                std::vector<std::uint8_t>& diffPixels)
{
    if (baseline.width != actual.width || baseline.height != actual.height) {
        throw std::runtime_error("Image sizes do not match for comparison");
    }

    const std::uint32_t width = baseline.width;
    const std::uint32_t height = baseline.height;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    const std::size_t expectedSize = pixelCount * 4;
    if (baseline.pixels.size() < expectedSize || actual.pixels.size() < expectedSize) {
        throw std::runtime_error("Image pixel buffers are incomplete");
    }
    diffPixels.resize(pixelCount * 4);

    double totalError = 0.0;
    double maxError = 0.0;
    constexpr double noiseFloor = 1.0;

    for (std::size_t i = 0; i < pixelCount; ++i) {
        const std::size_t offset = i * 4;
        const int dr = std::abs(static_cast<int>(baseline.pixels[offset + 0]) - static_cast<int>(actual.pixels[offset + 0]));
        const int dg = std::abs(static_cast<int>(baseline.pixels[offset + 1]) - static_cast<int>(actual.pixels[offset + 1]));
        const int db = std::abs(static_cast<int>(baseline.pixels[offset + 2]) - static_cast<int>(actual.pixels[offset + 2]));

        double pixelError = 0.2126 * dr + 0.7152 * dg + 0.0722 * db;
        if (pixelError < noiseFloor) {
            pixelError = 0.0;
        }
        totalError += pixelError;
        maxError = std::max(maxError, pixelError);

        const int intensity = static_cast<int>(std::clamp(pixelError * 8.0, 0.0, 255.0));
        diffPixels[offset + 0] = static_cast<std::uint8_t>(intensity);
        diffPixels[offset + 1] = static_cast<std::uint8_t>(intensity);
        diffPixels[offset + 2] = static_cast<std::uint8_t>(intensity);
        diffPixels[offset + 3] = 255;
    }

    ImageDiffStats stats{};
    stats.meanError = totalError / static_cast<double>(pixelCount);
    stats.maxError = maxError;
    stats.width = width;
    stats.height = height;
    return stats;
}

vkengine::TextureData loadRenderImage(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Image file does not exist: " + path.string());
    }
    const auto ext = path.extension().string();
    if (ext == ".ppm" || ext == ".pnm") {
        vkengine::TextureData image = vkengine::loadTexture(path, false);
        if (image.width == 0 || image.height == 0 || image.pixels.empty()) {
            throw std::runtime_error("Image load failed or empty: " + path.string());
        }
        return image;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        const char* reason = stbi_failure_reason();
        if (pixels) {
            stbi_image_free(pixels);
        }
        throw std::runtime_error("stb_image failed to load: " + path.string() +
                                 (reason ? (" (" + std::string(reason) + ")") : ""));
    }

    vkengine::TextureData image{};
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.channels = 4;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height * 4;
    image.pixels.assign(pixels, pixels + pixelCount);
    stbi_image_free(pixels);
    return image;
}

bool ensureCaptureForExample(const std::string& name,
                             const std::filesystem::path& capturePath,
                             const std::filesystem::path& repoRoot)
{
    if (std::filesystem::exists(capturePath)) {
        return true;
    }

    auto buildDir = std::filesystem::current_path();
    if (buildDir.filename() == "tests") {
        buildDir = buildDir.parent_path();
    }
    if (buildDir.filename() != "build") {
        const auto candidate = repoRoot / "build";
        if (std::filesystem::exists(candidate)) {
            buildDir = candidate;
        }
    }
    const auto debugDir = buildDir / "Debug";
    const auto releaseDir = buildDir / "Release";
    const auto exeDir = std::filesystem::exists(debugDir) ? debugDir : releaseDir;
    const auto exePath = exeDir / (name + ".exe");
    if (!std::filesystem::exists(exePath)) {
        return false;
    }

    std::filesystem::create_directories(capturePath.parent_path());

#if defined(_WIN32)
    const std::wstring inner = L"cd /d \"" + repoRoot.wstring() + L"\" && \"" + exePath.wstring() + L"\" --headless --capture \"" + capturePath.wstring() + L"\"";
    const std::wstring command = L"cmd.exe /c \"" + inner + L"\"";
    const int code = _wsystem(command.c_str());
#else
    const std::string command = "cd \"" + repoRoot.string() + "\" && \"" + exePath.string() + "\" --headless --capture \"" + capturePath.string() + "\"";
    const int code = std::system(command.c_str());
#endif

    return code == 0 && std::filesystem::exists(capturePath);
}

TEST(RendererTests, CompareHeadlessRenders)
{
    const char* enable = std::getenv("VKENGINE_HEADLESS_TEST");
    if (!enable || std::string(enable) != "1") {
        GTEST_SKIP() << "Set VKENGINE_HEADLESS_TEST=1 to enable render comparisons.";
    }

    const auto root = vkengine::resolveRepoRoot();
    const auto baselineDir = root / "tests" / "test_resources" / "renders";
    const auto captureDir = root / "tests" / "test_results" / "renders";
    const auto diffDir = captureDir;
    if (std::filesystem::exists(captureDir)) {
        std::filesystem::remove_all(captureDir);
    }
    std::filesystem::create_directories(captureDir);

    const std::vector<std::string> examples = {
        //"CubeExample",
        "DeformableExample",
        "DrawOverlayExample",
        "EmittingBodiesExample",
        "GlassExample",
        "DustCubeExample",
        "MaterialsExample",
        "LatticeExample",
        //"LightsExample",
        //"ReflectionsExample",
        "MolecularDynamicsExample",
        //"NBodyExample",
        //"ParticleExample",
        "PhysicsExample",
        "ShaderStylesExample",
        "SkyImageBackgroundExample",
        "VolumetricFogExample",
        ///"WindowFeaturesExample"
    };

    const double meanThreshold = 2.0;
    const double maxThreshold = 30.0;

    const bool trace = std::getenv("VKENGINE_RENDER_TRACE") != nullptr;

    for (const auto& name : examples) {
        const auto filename = name + "_first_frame.jpg";
        const auto baselinePath = baselineDir / filename;
        const auto capturePath = captureDir / filename;
        const auto diffPath = diffDir / (name + "_diff.jpg");
            if (trace) {
                std::cout << "[RenderCompare] " << name << "\n" << std::flush;
            }

            const bool baselineExists = std::filesystem::exists(baselinePath);
            if (!baselineExists && name != "DustCubeExample") {
                ASSERT_TRUE(baselineExists) << "Missing baseline: " << baselinePath.string();
            }
            const bool captureExists = std::filesystem::exists(capturePath);
            if (trace) {
                std::cout << "  capture path: " << capturePath.string() << " (" << (captureExists ? "exists" : "missing") << ")\n" << std::flush;
            }
            if (!captureExists) {
                if (trace) {
                    std::cout << "  generating capture: " << capturePath.string() << "\n" << std::flush;
                }
                const bool captured = ensureCaptureForExample(name, capturePath, root);
                if (!captured) {
                    if (trace) {
                        std::cout << "  capture failed\n" << std::flush;
                    }
                    FAIL() << "Missing capture: " << capturePath.string();
                }
            }

            if (!baselineExists) {
                if (trace) {
                    std::cout << "  skipping compare (no baseline)\n" << std::flush;
                }
                continue;
            }

            vkengine::TextureData baseline;
            vkengine::TextureData capture;

            if (trace) {
                std::cout << "  loading images\n" << std::flush;
            }
            baseline = loadRenderImage(baselinePath);
            capture = loadRenderImage(capturePath);

            std::vector<std::uint8_t> diffPixels;
            const ImageDiffStats stats = computeImageDiff(baseline, capture, diffPixels);

            if (trace) {
                std::cout << "  diff mean=" << stats.meanError
                    << " max=" << stats.maxError;
            }

            EXPECT_LE(stats.meanError, meanThreshold) << "Mean error too high for " << name;
            EXPECT_LE(stats.maxError, maxThreshold) << "Max error too high for " << name;
    }
}

} // namespace
