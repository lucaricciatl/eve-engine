#pragma once

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "engine/InputTypes.hpp"

namespace vkengine {

class DeformableBody;
class SoftBodyVolume;
class ParticleEmitter;

enum class MeshType {
    Cube,
    WireCubeLines,
    DeformableCloth,
    SoftBodyVolume,
    CustomMesh,
};

enum class LightType {
    Point = 0,
    Spot = 1,
    Area = 2,
};

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 matrix() const;
};

struct PhysicsProperties {
    bool simulate{false};
    float mass{1.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 accumulatedForces{0.0f};
    glm::vec3 lastAcceleration{0.0f};
    bool collidable{false};
    glm::vec3 angularVelocity{0.0f};
    glm::vec3 accumulatedTorque{0.0f};
    float linearDamping{0.02f};
    float angularDamping{0.04f};
    float restitution{0.25f};
    float staticFriction{0.6f};
    float dynamicFriction{0.5f};

    void addForce(const glm::vec3& force) { accumulatedForces += force; }
    void clearForces() { accumulatedForces = glm::vec3(0.0f); }
    void addTorque(const glm::vec3& torque) { accumulatedTorque += torque; }
    void clearTorques() { accumulatedTorque = glm::vec3(0.0f); }
};

struct Collider {
    glm::vec3 halfExtents{0.5f};
    bool isStatic{false};
};

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

struct RenderComponent {
    MeshType mesh{MeshType::Cube};
    std::string meshResource;
    std::string albedoTexture;
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    std::string materialName{"default"};
    float metallic{0.0f};
    float roughness{0.6f};
    float specular{0.5f};
    glm::vec3 emissive{0.0f};
    float emissiveIntensity{0.0f};
    float opacity{1.0f};
    bool visible{true};
};

struct NameComponent {
    std::string value;
};

struct ColliderComponent {
    std::optional<Collider> shape;
};

struct DeformableComponent {
    std::shared_ptr<DeformableBody> cloth;
};

struct SoftBodyComponent {
    std::shared_ptr<SoftBodyVolume> volume;
};

struct ParticleEmitterComponent {
    std::unique_ptr<ParticleEmitter> emitter;
    bool enabled{true};
};

struct LightComponent {
    std::string name;
    glm::vec3 position{0.0f, 3.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    LightType type{LightType::Point};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float range{18.0f};
    float innerConeAngle{0.261799f};
    float outerConeAngle{0.523599f};
    glm::vec2 areaSize{1.2f, 1.2f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    bool enabled{true};
};

struct InputStateComponent {
    std::array<bool, kMaxInputKeys> keyStates{};
    glm::vec2 pendingMouseDelta{0.0f};
    glm::vec2 scrollDelta{0.0f};
    bool mouseLookActive{false};
    std::uint64_t latestSample{0};
    CameraInput latestCameraInput{};
};

} // namespace vkengine
