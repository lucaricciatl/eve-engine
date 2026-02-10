/**
 * @file TextureGenerator.cpp
 * @brief CPU-side procedural texture generation (10 pattern types).
 */

#include "engine/assets/TextureGenerator.hpp"
#include "engine/assets/ImageWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace vkengine {

// ===== Helpers =============================================================

static constexpr float kPi = 3.14159265358979323846f;

static inline Color4 lerpColor(const Color4& a, const Color4& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        static_cast<uint8_t>(a.r + t * (b.r - a.r)),
        static_cast<uint8_t>(a.g + t * (b.g - a.g)),
        static_cast<uint8_t>(a.b + t * (b.b - a.b)),
        static_cast<uint8_t>(a.a + t * (b.a - a.a)),
    };
}

static inline void setPixel(std::vector<uint8_t>& px, uint32_t w, uint32_t x, uint32_t y, const Color4& c) {
    const size_t idx = (static_cast<size_t>(y) * w + x) * 4;
    px[idx + 0] = c.r;
    px[idx + 1] = c.g;
    px[idx + 2] = c.b;
    px[idx + 3] = c.a;
}

static inline void fillAll(std::vector<uint8_t>& px, uint32_t w, uint32_t h, const Color4& c) {
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            setPixel(px, w, x, y, c);
}

// Simple hash-based Perlin-style value noise
static inline float hashFloat(int ix, int iy, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(ix) * 374761393u
               + static_cast<uint32_t>(iy) * 668265263u
               + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0x7fffffffu) / static_cast<float>(0x7fffffffu);
}

static float smoothNoise(float x, float y, uint32_t seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - ix;
    float fy = y - iy;
    // Smoothstep
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float v00 = hashFloat(ix, iy, seed);
    float v10 = hashFloat(ix + 1, iy, seed);
    float v01 = hashFloat(ix, iy + 1, seed);
    float v11 = hashFloat(ix + 1, iy + 1, seed);

    float top    = v00 + fx * (v10 - v00);
    float bottom = v01 + fx * (v11 - v01);
    return top + fy * (bottom - top);
}

static float perlinFbm(float x, float y, uint32_t seed, int octaves, float lacunarity, float persistence) {
    float value = 0.0f;
    float amp   = 1.0f;
    float freq  = 1.0f;
    float maxAmp = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        value  += smoothNoise(x * freq, y * freq, seed + static_cast<uint32_t>(i) * 31u) * amp;
        maxAmp += amp;
        freq   *= lacunarity;
        amp    *= persistence;
    }
    return value / maxAmp;
}

// ===== Pattern generators ==================================================

static void genVoronoi(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    std::mt19937 rng(p.seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const auto cellCount = std::max(1u, p.voronoiCellCount);
    std::vector<float> cx(cellCount), cy(cellCount);
    for (uint32_t i = 0; i < cellCount; ++i) {
        cx[i] = dist(rng);
        cy[i] = dist(rng);
    }

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float nx = static_cast<float>(x) / w;
            float ny = static_cast<float>(y) / h;

            float minDist  = 1e9f;
            float minDist2 = 1e9f;
            uint32_t closest = 0;
            for (uint32_t i = 0; i < cellCount; ++i) {
                float dx = nx - cx[i];
                float dy = ny - cy[i];
                float d  = dx * dx + dy * dy;
                if (d < minDist) {
                    minDist2 = minDist;
                    minDist  = d;
                    closest  = i;
                } else if (d < minDist2) {
                    minDist2 = d;
                }
            }

            float t = static_cast<float>(closest) / static_cast<float>(cellCount);
            Color4 col = lerpColor(p.voronoiColor1, p.voronoiColor2, t);

            if (p.voronoiShowEdges) {
                float edgeDist = std::sqrt(minDist2) - std::sqrt(minDist);
                float edgeNorm = edgeDist * static_cast<float>(std::max(w, h));
                if (edgeNorm < p.voronoiEdgeWidth) {
                    col = p.voronoiEdgeColor;
                }
            }

            setPixel(px, w, x, y, col);
        }
    }
}

static void genRegularGrid(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.gridCols);
    const auto rows = std::max(1u, p.gridRows);
    float cellW = static_cast<float>(w) / cols;
    float cellH = static_cast<float>(h) / rows;
    float halfLine = p.gridLineWidth * 0.5f;

    fillAll(px, w, h, p.gridFillColor);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);
            float modX = std::fmod(fx, cellW);
            float modY = std::fmod(fy, cellH);
            if (modX < halfLine || modX > cellW - halfLine ||
                modY < halfLine || modY > cellH - halfLine) {
                setPixel(px, w, x, y, p.gridLineColor);
            }
        }
    }
}

static void genTriangular(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.triCols);
    const auto rows = std::max(1u, p.triRows);
    float cellW = static_cast<float>(w) / cols;
    float cellH = static_cast<float>(h) / rows;
    float halfLine = p.triLineWidth * 0.5f;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            int col = static_cast<int>(fx / cellW);
            int row = static_cast<int>(fy / cellH);

            float lx = fx - col * cellW;
            float ly = fy - row * cellH;

            // Diagonal goes from top-left to bottom-right (even) or top-right to bottom-left (odd)
            bool upper;
            if ((col + row) % 2 == 0) {
                upper = (ly / cellH) < (lx / cellW);
            } else {
                upper = (ly / cellH) < (1.0f - lx / cellW);
            }

            Color4 fill = upper ? p.triColor1 : p.triColor2;

            // Check proximity to grid edges and diagonal
            bool onEdge = false;
            if (lx < halfLine || lx > cellW - halfLine || ly < halfLine || ly > cellH - halfLine)
                onEdge = true;

            // Diagonal distance
            float diagDist;
            if ((col + row) % 2 == 0) {
                diagDist = std::abs(ly * cellW - lx * cellH) / std::sqrt(cellW * cellW + cellH * cellH);
            } else {
                diagDist = std::abs(ly * cellW - (cellW - lx) * cellH) / std::sqrt(cellW * cellW + cellH * cellH);
            }
            if (diagDist < halfLine)
                onEdge = true;

            setPixel(px, w, x, y, onEdge ? p.triLineColor : fill);
        }
    }
}

static void genFiber(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    fillAll(px, w, h, p.fiberBgColor);

    std::mt19937 rng(p.seed);
    std::uniform_real_distribution<float> distY(0.0f, static_cast<float>(h));
    std::uniform_real_distribution<float> distPhase(0.0f, 2.0f * kPi);
    std::uniform_int_distribution<int>    distCol(0, 1);

    const auto count = std::max(1u, p.fiberCount);
    for (uint32_t i = 0; i < count; ++i) {
        float baseY = distY(rng);
        float phase = distPhase(rng);
        Color4 col  = distCol(rng) ? p.fiberColor1 : p.fiberColor2;
        float halfW = p.fiberWidth * 0.5f;

        for (uint32_t x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);
            float cy = baseY + p.fiberAmplitude * std::sin(p.fiberTwist * fx * 2.0f * kPi + phase);

            int yMin = static_cast<int>(cy - halfW);
            int yMax = static_cast<int>(cy + halfW);
            yMin = std::clamp(yMin, 0, static_cast<int>(h) - 1);
            yMax = std::clamp(yMax, 0, static_cast<int>(h) - 1);
            for (int yy = yMin; yy <= yMax; ++yy) {
                setPixel(px, w, x, static_cast<uint32_t>(yy), col);
            }
        }
    }
}

static void genBricks(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.brickCols);
    const auto rows = std::max(1u, p.brickRows);
    float cellW = static_cast<float>(w) / cols;
    float cellH = static_cast<float>(h) / rows;
    float halfMortar = p.brickMortar * 0.5f;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float fy = static_cast<float>(y);
            int row = static_cast<int>(fy / cellH);
            float ly = fy - row * cellH;

            // Offset every other row by half a brick
            float offset = (row % 2 == 1) ? cellW * 0.5f : 0.0f;
            float fx = std::fmod(static_cast<float>(x) + offset, static_cast<float>(w));
            float lx = std::fmod(fx, cellW);

            bool mortar = (lx < halfMortar || lx > cellW - halfMortar ||
                           ly < halfMortar || ly > cellH - halfMortar);

            if (mortar) {
                setPixel(px, w, x, y, p.brickMortarColor);
            } else {
                Color4 col = ((row + static_cast<int>(fx / cellW)) % 2 == 0) ? p.brickColor1 : p.brickColor2;
                setPixel(px, w, x, y, col);
            }
        }
    }
}

static void genCheckerboard(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.checkerCols);
    const auto rows = std::max(1u, p.checkerRows);
    float cellW = static_cast<float>(w) / cols;
    float cellH = static_cast<float>(h) / rows;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            int col = static_cast<int>(static_cast<float>(x) / cellW);
            int row = static_cast<int>(static_cast<float>(y) / cellH);
            Color4 c = ((col + row) % 2 == 0) ? p.checkerColor1 : p.checkerColor2;
            setPixel(px, w, x, y, c);
        }
    }
}

static void genNoise(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const int octaves = static_cast<int>(std::max(1u, p.noiseOctaves));

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float nx = static_cast<float>(x) / w * p.noiseScale;
            float ny = static_cast<float>(y) / h * p.noiseScale;
            float n  = perlinFbm(nx, ny, p.seed, octaves, p.noiseLacunarity, p.noisePersistence);
            n = std::clamp(n, 0.0f, 1.0f);
            Color4 c = lerpColor(p.noiseColor1, p.noiseColor2, n);
            setPixel(px, w, x, y, c);
        }
    }
}

static void genHerringbone(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.herringboneCols);
    const auto rows = std::max(1u, p.herringboneRows);
    float cellW = static_cast<float>(w) / cols;
    float cellH = static_cast<float>(h) / rows;
    float halfMortar = p.herringboneMortar * 0.5f;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            int col = static_cast<int>(fx / cellW);
            int row = static_cast<int>(fy / cellH);
            float lx = fx - col * cellW;
            float ly = fy - row * cellH;

            bool mortar = (lx < halfMortar || lx > cellW - halfMortar ||
                           ly < halfMortar || ly > cellH - halfMortar);

            if (mortar) {
                setPixel(px, w, x, y, p.herringboneMortarColor);
            } else {
                // Herringbone: alternate direction based on row
                bool flip = (row % 2 == 0);
                float diag = flip ? (lx / cellW + ly / cellH) : (lx / cellW - ly / cellH + 1.0f);
                int band = static_cast<int>(diag * 2.0f);
                Color4 c = (band % 2 == 0) ? p.herringboneColor1 : p.herringboneColor2;
                setPixel(px, w, x, y, c);
            }
        }
    }
}

static void genHexagonal(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.hexCols);
    const auto rows = std::max(1u, p.hexRows);

    float hexW = static_cast<float>(w) / cols;
    float hexH = static_cast<float>(h) / rows;
    float halfLine = p.hexLineWidth * 0.5f;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            // Offset every other row
            int row = static_cast<int>(fy / hexH);
            float offset = (row % 2 == 1) ? hexW * 0.5f : 0.0f;
            float adjX = fx + offset;
            int col = static_cast<int>(adjX / hexW);

            float lx = adjX - col * hexW;
            float ly = fy - row * hexH;

            // Normalize to [0,1]
            float nx = lx / hexW;
            float ny = ly / hexH;

            // Simple hex edge detection: check proximity to cell boundaries
            bool edge = false;
            if (nx < halfLine / hexW || nx > 1.0f - halfLine / hexW)
                edge = true;
            if (ny < halfLine / hexH || ny > 1.0f - halfLine / hexH)
                edge = true;

            // Diagonal edges for hex shape
            float dx = std::abs(nx - 0.5f);
            float dy = std::abs(ny - 0.5f);
            float hexDist = dx + dy * 0.5f;
            if (std::abs(hexDist - 0.5f) < halfLine / hexW)
                edge = true;

            if (edge) {
                setPixel(px, w, x, y, p.hexLineColor);
            } else {
                Color4 c = ((col + row) % 2 == 0) ? p.hexFillColor1 : p.hexFillColor2;
                setPixel(px, w, x, y, c);
            }
        }
    }
}

static void genDots(const TextureParams& p, std::vector<uint8_t>& px) {
    const auto w = p.width, h = p.height;
    const auto cols = std::max(1u, p.dotCols);
    const auto rows = std::max(1u, p.dotRows);
    float cellW = static_cast<float>(w) / cols;
    float cellH = static_cast<float>(h) / rows;
    float r2 = p.dotRadius * p.dotRadius;

    fillAll(px, w, h, p.dotBgColor);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            float lx = std::fmod(fx, cellW) / cellW - 0.5f;
            float ly = std::fmod(fy, cellH) / cellH - 0.5f;
            float d  = lx * lx + ly * ly;

            if (d <= r2) {
                setPixel(px, w, x, y, p.dotColor);
            }
        }
    }
}

// ===== Public API ==========================================================

TextureData generateTexture(const TextureParams& params) {
    TextureData result;
    result.width    = std::max(1u, params.width);
    result.height   = std::max(1u, params.height);
    result.channels = 4;
    result.pixels.resize(static_cast<size_t>(result.width) * result.height * 4, 255);

    switch (params.pattern) {
    case PatternType::Voronoi:      genVoronoi(params, result.pixels);      break;
    case PatternType::RegularGrid:  genRegularGrid(params, result.pixels);  break;
    case PatternType::Triangular:   genTriangular(params, result.pixels);   break;
    case PatternType::Fiber:        genFiber(params, result.pixels);        break;
    case PatternType::Bricks:       genBricks(params, result.pixels);       break;
    case PatternType::Checkerboard: genCheckerboard(params, result.pixels); break;
    case PatternType::Noise:        genNoise(params, result.pixels);        break;
    case PatternType::Herringbone:  genHerringbone(params, result.pixels);  break;
    case PatternType::Hexagonal:    genHexagonal(params, result.pixels);    break;
    case PatternType::Dots:         genDots(params, result.pixels);         break;
    default:                        genCheckerboard(params, result.pixels); break;
    }

    return result;
}

bool generateTextureToJpeg(const TextureParams& params,
                            const std::filesystem::path& outputPath,
                            int jpegQuality) {
    auto tex = generateTexture(params);
    return writeJpeg(outputPath,
                     static_cast<int>(tex.width),
                     static_cast<int>(tex.height),
                     tex.pixels,
                     jpegQuality);
}

} // namespace vkengine
