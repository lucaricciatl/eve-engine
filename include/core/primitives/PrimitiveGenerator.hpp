#pragma once

#include "engine/assets/MeshLoader.hpp"

#include <cstdint>

namespace vkengine {

/**
 * @brief Static class for generating procedural geometric primitives.
 *
 * All primitives are centered at the origin with unit dimensions by default.
 * Use transform operations to scale, rotate, and position them as needed.
 */
class PrimitiveGenerator {
public:
    PrimitiveGenerator() = delete;

    /**
     * @brief Generate a unit cube (1x1x1) centered at origin.
     * @return MeshData containing vertices and indices for the cube.
     */
    [[nodiscard]] static MeshData createCube();

    /**
     * @brief Generate a UV sphere centered at origin.
     * @param radius Sphere radius (default 0.5 for unit diameter).
     * @param sectors Number of longitudinal divisions (default 32).
     * @param stacks Number of latitudinal divisions (default 16).
     * @return MeshData containing vertices and indices for the sphere.
     */
    [[nodiscard]] static MeshData createSphere(float radius = 0.5f,
                                                std::uint32_t sectors = 32,
                                                std::uint32_t stacks = 16);

    /**
     * @brief Generate a cylinder aligned along the Y-axis, centered at origin.
     * @param radius Cylinder radius (default 0.5).
     * @param height Cylinder height (default 1.0).
     * @param sectors Number of radial divisions (default 32).
     * @param capSegments Number of rings on caps (default 1).
     * @return MeshData containing vertices and indices for the cylinder.
     */
    [[nodiscard]] static MeshData createCylinder(float radius = 0.5f,
                                                  float height = 1.0f,
                                                  std::uint32_t sectors = 32,
                                                  std::uint32_t capSegments = 1);

    /**
     * @brief Generate a cone aligned along the Y-axis, base at bottom.
     * @param radius Base radius (default 0.5).
     * @param height Cone height (default 1.0).
     * @param sectors Number of radial divisions (default 32).
     * @return MeshData containing vertices and indices for the cone.
     */
    [[nodiscard]] static MeshData createCone(float radius = 0.5f,
                                              float height = 1.0f,
                                              std::uint32_t sectors = 32);

    /**
     * @brief Generate a torus centered at origin, lying in the XZ plane.
     * @param majorRadius Distance from center of torus to center of tube (default 0.5).
     * @param minorRadius Radius of the tube (default 0.2).
     * @param majorSegments Number of segments around the torus (default 32).
     * @param minorSegments Number of segments around the tube (default 16).
     * @return MeshData containing vertices and indices for the torus.
     */
    [[nodiscard]] static MeshData createTorus(float majorRadius = 0.5f,
                                               float minorRadius = 0.2f,
                                               std::uint32_t majorSegments = 32,
                                               std::uint32_t minorSegments = 16);

    /**
     * @brief Generate a flat plane in the XZ plane, centered at origin.
     * @param width Width along X-axis (default 1.0).
     * @param depth Depth along Z-axis (default 1.0).
     * @param subdivisionsX Number of subdivisions along X (default 1).
     * @param subdivisionsZ Number of subdivisions along Z (default 1).
     * @return MeshData containing vertices and indices for the plane.
     */
    [[nodiscard]] static MeshData createPlane(float width = 1.0f,
                                               float depth = 1.0f,
                                               std::uint32_t subdivisionsX = 1,
                                               std::uint32_t subdivisionsZ = 1);

    /**
     * @brief Generate a capsule (cylinder with hemispherical caps) along Y-axis.
     * @param radius Radius of the capsule (default 0.25).
     * @param height Total height including caps (default 1.0).
     * @param sectors Number of radial divisions (default 32).
     * @param stacks Number of latitudinal divisions per hemisphere (default 8).
     * @return MeshData containing vertices and indices for the capsule.
     */
    [[nodiscard]] static MeshData createCapsule(float radius = 0.25f,
                                                 float height = 1.0f,
                                                 std::uint32_t sectors = 32,
                                                 std::uint32_t stacks = 8);

    /**
     * @brief Generate an icosphere using subdivision of an icosahedron.
     * @param radius Sphere radius (default 0.5).
     * @param subdivisions Number of subdivision iterations (default 2, max 5).
     * @return MeshData containing vertices and indices for the icosphere.
     */
    [[nodiscard]] static MeshData createIcosphere(float radius = 0.5f,
                                                   std::uint32_t subdivisions = 2);
};

} // namespace vkengine
