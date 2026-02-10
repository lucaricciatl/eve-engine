#pragma once

#include "engine/assets/TextureLoader.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace vkengine {

// ---------------------------------------------------------------------------
// Color helper (RGBA, 8-bit per channel)
// ---------------------------------------------------------------------------
struct Color4 {
    std::uint8_t r{0}, g{0}, b{0}, a{255};
};

// ---------------------------------------------------------------------------
// Supported procedural-texture patterns
// ---------------------------------------------------------------------------
enum class PatternType : int {
    Voronoi = 0,
    RegularGrid,
    Triangular,
    Fiber,
    Bricks,
    Checkerboard,
    Noise,        // Perlin noise
    Herringbone,
    Hexagonal,
    Dots,
    Count         // sentinel
};

// ---------------------------------------------------------------------------
// Parameters that drive the texture generator
// ---------------------------------------------------------------------------
struct TextureParams {
    PatternType pattern{PatternType::Voronoi};
    std::uint32_t width{512};
    std::uint32_t height{512};
    std::uint32_t seed{42};

    // ---- Voronoi ----
    std::uint32_t voronoiCellCount{32};
    float voronoiJitter{1.0f};
    bool  voronoiShowEdges{false};
    float voronoiEdgeWidth{2.0f};
    Color4 voronoiEdgeColor{0, 0, 0, 255};
    Color4 voronoiColor1{30, 100, 200, 255};
    Color4 voronoiColor2{200, 220, 255, 255};

    // ---- Regular Grid ----
    std::uint32_t gridCols{8};
    std::uint32_t gridRows{8};
    float gridLineWidth{2.0f};
    Color4 gridLineColor{0, 0, 0, 255};
    Color4 gridFillColor{220, 220, 220, 255};

    // ---- Triangular ----
    std::uint32_t triCols{8};
    std::uint32_t triRows{8};
    float triLineWidth{2.0f};
    Color4 triColor1{255, 200, 100, 255};
    Color4 triColor2{200, 100, 50, 255};
    Color4 triLineColor{60, 30, 10, 255};

    // ---- Fiber ----
    std::uint32_t fiberCount{40};
    float fiberWidth{3.0f};
    float fiberTwist{5.0f};
    float fiberAmplitude{20.0f};
    Color4 fiberColor1{180, 140, 100, 255};
    Color4 fiberColor2{140, 100, 60, 255};
    Color4 fiberBgColor{240, 230, 210, 255};

    // ---- Bricks ----
    std::uint32_t brickCols{8};
    std::uint32_t brickRows{16};
    float brickMortar{4.0f};
    Color4 brickColor1{180, 60, 50, 255};
    Color4 brickColor2{200, 80, 60, 255};
    Color4 brickMortarColor{200, 200, 190, 255};

    // ---- Checkerboard ----
    std::uint32_t checkerCols{8};
    std::uint32_t checkerRows{8};
    Color4 checkerColor1{240, 240, 240, 255};
    Color4 checkerColor2{30, 30, 30, 255};

    // ---- Noise (Perlin) ----
    float noiseScale{4.0f};
    std::uint32_t noiseOctaves{4};
    float noiseLacunarity{2.0f};
    float noisePersistence{0.5f};
    Color4 noiseColor1{10, 10, 10, 255};
    Color4 noiseColor2{240, 240, 240, 255};

    // ---- Herringbone ----
    std::uint32_t herringboneCols{8};
    std::uint32_t herringboneRows{16};
    float herringboneMortar{3.0f};
    Color4 herringboneColor1{180, 140, 100, 255};
    Color4 herringboneColor2{160, 120, 80, 255};
    Color4 herringboneMortarColor{200, 200, 190, 255};

    // ---- Hexagonal ----
    std::uint32_t hexCols{8};
    std::uint32_t hexRows{8};
    float hexLineWidth{2.0f};
    Color4 hexLineColor{40, 40, 40, 255};
    Color4 hexFillColor1{100, 180, 100, 255};
    Color4 hexFillColor2{60, 140, 60, 255};

    // ---- Dots ----
    std::uint32_t dotCols{8};
    std::uint32_t dotRows{8};
    float dotRadius{0.3f};
    Color4 dotColor{30, 30, 30, 255};
    Color4 dotBgColor{240, 240, 240, 255};
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/// Generate an RGBA texture in CPU memory.
[[nodiscard]] TextureData generateTexture(const TextureParams& params);

/// Generate a texture and write it straight to a JPEG file (headless).
[[nodiscard]] bool generateTextureToJpeg(const TextureParams& params,
                                         const std::filesystem::path& outputPath,
                                         int jpegQuality = 90);

} // namespace vkengine
