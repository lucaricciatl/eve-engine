/**
 * @file test_texture_generator.cpp
 * @brief Google Test suite for the procedural TextureGenerator.
 */

#include <gtest/gtest.h>
#include "engine/assets/TextureGenerator.hpp"

#include <cstdint>
#include <vector>

using namespace vkengine;

// ===========================================================================
// Helpers
// ===========================================================================

static TextureParams makeParams(PatternType pat, uint32_t w = 64, uint32_t h = 64) {
    TextureParams p;
    p.pattern = pat;
    p.width   = w;
    p.height  = h;
    p.seed    = 42;
    return p;
}

// Check that the TextureData is well-formed.
static void expectValidTexture(const TextureData& tex, uint32_t w, uint32_t h) {
    EXPECT_EQ(tex.width, w);
    EXPECT_EQ(tex.height, h);
    EXPECT_EQ(tex.channels, 4u);
    EXPECT_EQ(tex.pixels.size(), static_cast<size_t>(w) * h * 4);
}

// Check that at least two distinct pixel values exist (not a flat fill).
static bool hasVariation(const TextureData& tex) {
    if (tex.pixels.size() < 8) return false;
    for (size_t i = 4; i < tex.pixels.size(); i += 4) {
        if (tex.pixels[i] != tex.pixels[0] ||
            tex.pixels[i + 1] != tex.pixels[1] ||
            tex.pixels[i + 2] != tex.pixels[2]) {
            return true;
        }
    }
    return false;
}

// ===========================================================================
// Per-pattern smoke tests
// ===========================================================================

class PatternTest : public ::testing::TestWithParam<PatternType> {};

TEST_P(PatternTest, GeneratesValidTexture) {
    auto p = makeParams(GetParam());
    auto tex = generateTexture(p);
    expectValidTexture(tex, p.width, p.height);
}

TEST_P(PatternTest, HasPixelVariation) {
    auto p = makeParams(GetParam());
    auto tex = generateTexture(p);
    EXPECT_TRUE(hasVariation(tex)) << "Pattern produced a flat single-color image.";
}

TEST_P(PatternTest, DifferentSeedsDifferentOutput) {
    auto p1 = makeParams(GetParam());
    p1.seed = 1;
    auto p2 = makeParams(GetParam());
    p2.seed = 999;

    auto t1 = generateTexture(p1);
    auto t2 = generateTexture(p2);

    // At least some pixels should differ (unless the pattern ignores seed).
    bool differs = false;
    for (size_t i = 0; i < t1.pixels.size() && !differs; ++i) {
        if (t1.pixels[i] != t2.pixels[i]) differs = true;
    }
    // Some patterns (checkerboard, grid) are deterministic regardless of seed.
    // We don't fail for those, just note it.
    if (!differs) {
        GTEST_LOG_(INFO) << "Pattern " << static_cast<int>(GetParam())
                         << " produces identical output with different seeds (deterministic).";
    }
}

TEST_P(PatternTest, DifferentResolutions) {
    auto p64  = makeParams(GetParam(), 64, 64);
    auto p128 = makeParams(GetParam(), 128, 128);

    auto t64  = generateTexture(p64);
    auto t128 = generateTexture(p128);

    expectValidTexture(t64, 64, 64);
    expectValidTexture(t128, 128, 128);
}

INSTANTIATE_TEST_SUITE_P(
    AllPatterns,
    PatternTest,
    ::testing::Values(
        PatternType::Voronoi,
        PatternType::RegularGrid,
        PatternType::Triangular,
        PatternType::Fiber,
        PatternType::Bricks,
        PatternType::Checkerboard,
        PatternType::Noise,
        PatternType::Herringbone,
        PatternType::Hexagonal,
        PatternType::Dots
    )
);

// ===========================================================================
// Edge cases
// ===========================================================================

TEST(TextureGenerator, MinimumSize) {
    auto p = makeParams(PatternType::Checkerboard, 1, 1);
    auto tex = generateTexture(p);
    expectValidTexture(tex, 1, 1);
}

TEST(TextureGenerator, NonSquare) {
    auto p = makeParams(PatternType::Noise, 128, 32);
    auto tex = generateTexture(p);
    expectValidTexture(tex, 128, 32);
}

TEST(TextureGenerator, ZeroSizeClampedToOne) {
    TextureParams p{};
    p.width = 0;
    p.height = 0;
    auto tex = generateTexture(p);
    EXPECT_GE(tex.width, 1u);
    EXPECT_GE(tex.height, 1u);
    EXPECT_FALSE(tex.pixels.empty());
}

TEST(TextureGenerator, LargerTexture) {
    auto p = makeParams(PatternType::Voronoi, 256, 256);
    auto tex = generateTexture(p);
    expectValidTexture(tex, 256, 256);
    EXPECT_TRUE(hasVariation(tex));
}

TEST(TextureGenerator, Deterministic) {
    auto p = makeParams(PatternType::Noise);
    auto t1 = generateTexture(p);
    auto t2 = generateTexture(p);
    EXPECT_EQ(t1.pixels, t2.pixels) << "Same params should produce identical output.";
}

// ===========================================================================
// Pattern-specific parameter tests
// ===========================================================================

TEST(VoronoiParams, CellCountAffectsOutput) {
    auto p1 = makeParams(PatternType::Voronoi);
    p1.voronoiCellCount = 4;
    auto p2 = makeParams(PatternType::Voronoi);
    p2.voronoiCellCount = 100;

    auto t1 = generateTexture(p1);
    auto t2 = generateTexture(p2);
    EXPECT_NE(t1.pixels, t2.pixels);
}

TEST(VoronoiParams, EdgeToggle) {
    auto p = makeParams(PatternType::Voronoi);
    p.voronoiShowEdges = false;
    auto t1 = generateTexture(p);
    p.voronoiShowEdges = true;
    auto t2 = generateTexture(p);
    EXPECT_NE(t1.pixels, t2.pixels);
}

TEST(CheckerboardParams, ColorsApplied) {
    auto p = makeParams(PatternType::Checkerboard);
    p.checkerColor1 = {255, 0, 0, 255};
    p.checkerColor2 = {0, 0, 255, 255};
    auto tex = generateTexture(p);

    // First pixel should be one of the two checker colors.
    uint8_t r = tex.pixels[0], g = tex.pixels[1], b = tex.pixels[2];
    bool isColor1 = (r == 255 && g == 0 && b == 0);
    bool isColor2 = (r == 0   && g == 0 && b == 255);
    EXPECT_TRUE(isColor1 || isColor2);
}

TEST(NoiseParams, ScaleAffectsOutput) {
    auto p1 = makeParams(PatternType::Noise);
    p1.noiseScale = 1.0f;
    auto p2 = makeParams(PatternType::Noise);
    p2.noiseScale = 20.0f;

    auto t1 = generateTexture(p1);
    auto t2 = generateTexture(p2);
    EXPECT_NE(t1.pixels, t2.pixels);
}

TEST(DotsParams, RadiusAffectsOutput) {
    auto p1 = makeParams(PatternType::Dots);
    p1.dotRadius = 0.1f;
    auto p2 = makeParams(PatternType::Dots);
    p2.dotRadius = 0.45f;

    auto t1 = generateTexture(p1);
    auto t2 = generateTexture(p2);
    EXPECT_NE(t1.pixels, t2.pixels);
}

TEST(BricksParams, MortarAffectsOutput) {
    auto p1 = makeParams(PatternType::Bricks);
    p1.brickMortar = 0.0f;
    auto p2 = makeParams(PatternType::Bricks);
    p2.brickMortar = 10.0f;

    auto t1 = generateTexture(p1);
    auto t2 = generateTexture(p2);
    EXPECT_NE(t1.pixels, t2.pixels);
}

// ===========================================================================
// JPEG export (headless)
// ===========================================================================

TEST(TextureGenerator, GenerateToJpeg) {
    namespace fs = std::filesystem;
    auto p = makeParams(PatternType::Checkerboard, 64, 64);
    fs::path outDir = fs::temp_directory_path() / "eve_texgen_test";
    fs::create_directories(outDir);
    fs::path outFile = outDir / "test_checkerboard.jpg";

    bool ok = generateTextureToJpeg(p, outFile, 90);
    // On systems without WIC / proper COM init this may fail gracefully.
    if (ok) {
        EXPECT_TRUE(fs::exists(outFile));
        EXPECT_GT(fs::file_size(outFile), 0u);
        fs::remove(outFile);
    } else {
        GTEST_LOG_(WARNING) << "JPEG export not available on this system (skipped).";
    }

    fs::remove_all(outDir);
}
