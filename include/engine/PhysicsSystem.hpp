#pragma once

#include <glm/glm.hpp>

#include <cstddef>

namespace vkengine {

struct Collider;
struct AABB;
class Scene;
class GameObject;

class PhysicsSystem {
public:
    void setGravity(const glm::vec3& gravityVector) noexcept { gravity = gravityVector; }
    [[nodiscard]] const glm::vec3& getGravity() const noexcept { return gravity; }

    void update(Scene& scene, float deltaSeconds);

private:
    void resolveCollisions(Scene& scene, float deltaSeconds);

private:
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
};

} // namespace vkengine
