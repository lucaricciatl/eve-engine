#pragma once

#include <glm/glm.hpp>

namespace vkengine {

struct AABB;
class GameObject;

namespace physics_detail {

struct CollisionResult {
    glm::vec3 normal{0.0f};
    float penetrationDepth{0.0f};
};

struct Contact {
    GameObject* a{nullptr};
    GameObject* b{nullptr};
    glm::vec3 normal{0.0f};
    glm::vec3 contactPoint{0.0f};
    glm::vec3 ra{0.0f};
    glm::vec3 rb{0.0f};
    float penetration{0.0f};
    float restitution{0.0f};
    float staticFriction{0.0f};
    float dynamicFriction{0.0f};
    float invMassA{0.0f};
    float invMassB{0.0f};
    glm::mat3 invInertiaA{0.0f};
    glm::mat3 invInertiaB{0.0f};

    [[nodiscard]] float totalInverseMass() const { return invMassA + invMassB; }
};

glm::vec3 worldHalfExtents(const GameObject& object);
glm::mat3 inertiaTensorBody(const GameObject& object);
glm::mat3 inertiaTensor(const GameObject& object);
glm::mat3 inverseInertiaTensor(const GameObject& object);
glm::vec3 applyInverseInertia(const glm::mat3& inverse, const glm::vec3& value);
glm::vec3 estimateContactPoint(const GameObject& a, const GameObject& b, const glm::vec3& normal, float penetration = 0.0f);
bool computePenetration(const AABB& a, const AABB& b, CollisionResult& result);
float combineCoefficient(float a, float b);

} // namespace physics_detail

} // namespace vkengine
