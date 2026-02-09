#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>

namespace vkengine {

struct Collider;
struct AABB;
class Scene;
class GameObject;
class GpuCollisionSystem;

class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem();

    // Non-copyable due to unique_ptr
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    PhysicsSystem(PhysicsSystem&&) noexcept;
    PhysicsSystem& operator=(PhysicsSystem&&) noexcept;

    void setGravity(const glm::vec3& gravityVector) noexcept { gravity = gravityVector; }
    [[nodiscard]] const glm::vec3& getGravity() const noexcept { return gravity; }

    // GPU collision detection control
    void setGpuCollisionEnabled(bool enabled) noexcept { useGpuCollision = enabled; }
    [[nodiscard]] bool isGpuCollisionEnabled() const noexcept { return useGpuCollision; }

    // Access GPU collision system for initialization/configuration
    [[nodiscard]] GpuCollisionSystem* gpuCollision() noexcept { return gpuCollisionSystem.get(); }
    [[nodiscard]] const GpuCollisionSystem* gpuCollision() const noexcept { return gpuCollisionSystem.get(); }

    void update(Scene& scene, float deltaSeconds);

private:
    void resolveCollisions(Scene& scene, float deltaSeconds);
    void resolveCollisionsGpu(Scene& scene, float deltaSeconds);

private:
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    bool useGpuCollision{false};
    std::unique_ptr<GpuCollisionSystem> gpuCollisionSystem;
};

} // namespace vkengine
