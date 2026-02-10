#pragma once

#include "core/ecs/Components.hpp"
#include "core/ecs/Registry.hpp"

#include <glm/glm.hpp>

#include "engine/DeformableBody.hpp"
#include "engine/InputTypes.hpp"
#include "engine/SoftBodyVolume.hpp"
#include "engine/IGameEngine.hpp"
#include "engine/Material.hpp"
#include "engine/ParticleSystem.hpp"
#include "engine/PhysicsSystem.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vkengine {

class Camera {
public:
    void setPosition(const glm::vec3& newPosition) noexcept { position = newPosition; }
    [[nodiscard]] const glm::vec3& getPosition() const noexcept { return position; }

    void lookAt(const glm::vec3& target) noexcept;
    void setYawPitch(float newYaw, float newPitch) noexcept;
    void rotate(float deltaYaw, float deltaPitch) noexcept;

    void setPerspective(float fovDegrees, float nearPlane, float farPlane) noexcept;
    void setMovementSpeed(float speed) noexcept { movementSpeed = speed; }
    [[nodiscard]] float getMovementSpeed() const noexcept { return movementSpeed; }
    void setRotationSpeed(float speedRadiansPerSecond) noexcept { rotationSpeed = speedRadiansPerSecond; }

    [[nodiscard]] glm::vec3 forwardVector() const noexcept { return forward(); }
    [[nodiscard]] glm::vec3 rightVector() const noexcept { return right(); }
    [[nodiscard]] glm::vec3 upVector() const noexcept { return glm::normalize(glm::cross(right(), forward())); }

    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] glm::mat4 projectionMatrix(float aspectRatio) const;

    void moveLocal(const glm::vec3& delta) noexcept;
    void applyInput(const CameraInput& input, float deltaSeconds) noexcept;

private:
    glm::vec3 forward() const;
    glm::vec3 right() const;

private:
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
    float yaw = glm::radians(-90.0f);
    float pitch = 0.0f;
    float fieldOfView = glm::radians(60.0f);
    float nearClip = 0.1f;
    float farClip = 100.0f;
    float movementSpeed = 3.0f;
    float rotationSpeed = glm::radians(180.0f);
};

class Scene;

class Light {
public:
    Light(Scene& owner, core::ecs::Entity entity);

    [[nodiscard]] const std::string& name() const;

    void setPosition(const glm::vec3& newPosition);
    [[nodiscard]] const glm::vec3& position() const;

    void setColor(const glm::vec3& newColor);
    [[nodiscard]] const glm::vec3& color() const;

    void setIntensity(float newIntensity);
    [[nodiscard]] float intensity() const;

    void setType(LightType newType);
    [[nodiscard]] LightType type() const;

    void setDirection(const glm::vec3& newDirection);
    [[nodiscard]] const glm::vec3& direction() const;

    void setRange(float newRange);
    [[nodiscard]] float range() const;

    void setConeAngles(float innerRadians, float outerRadians);
    [[nodiscard]] float innerConeAngle() const;
    [[nodiscard]] float outerConeAngle() const;

    void setAreaSize(const glm::vec2& newSize);
    [[nodiscard]] const glm::vec2& areaSize() const;

    void setUp(const glm::vec3& newUp);
    [[nodiscard]] const glm::vec3& up() const;

    void setEnabled(bool value);
    [[nodiscard]] bool isEnabled() const;

    [[nodiscard]] core::ecs::Entity entity() const noexcept { return entityHandle; }

private:
    Scene* ownerScene{nullptr};
    core::ecs::Entity entityHandle{};

    core::ecs::Registry& registry();
    const core::ecs::Registry& registry() const;
};

struct LightCreateInfo {
    std::string name;
    glm::vec3 position{0.0f, 3.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity{2.0f};
    LightType type{LightType::Point};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float range{18.0f};
    float innerConeAngle{0.261799f};
    float outerConeAngle{0.523599f};
    glm::vec2 areaSize{1.2f, 1.2f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    bool enabled{true};
};


class GameObject {
public:
    GameObject(Scene& owner, core::ecs::Entity entity);

    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] MeshType mesh() const;

    [[nodiscard]] Transform& transform();
    [[nodiscard]] const Transform& transform() const;

    [[nodiscard]] PhysicsProperties& physics();
    [[nodiscard]] const PhysicsProperties& physics() const;
    [[nodiscard]] RenderComponent& render();
    [[nodiscard]] const RenderComponent& render() const;

    [[nodiscard]] glm::mat4 modelMatrix() const { return transform().matrix(); }

    Collider& enableCollider(const glm::vec3& halfExtents, bool isStatic = false);
    void disableCollider();
    [[nodiscard]] bool hasCollider() const;
    [[nodiscard]] Collider* collider();
    [[nodiscard]] const Collider* collider() const;
    [[nodiscard]] AABB worldBounds() const;

    DeformableBody& enableDeformableCloth(int width, int height, float spacing);
    [[nodiscard]] bool hasDeformableBody() const;
    [[nodiscard]] DeformableBody* deformable();
    [[nodiscard]] const DeformableBody* deformable() const;

    SoftBodyVolume& enableSoftBodyVolume(int sizeX, int sizeY, int sizeZ, float spacing);
    [[nodiscard]] bool hasSoftBodyVolume() const;
    [[nodiscard]] SoftBodyVolume* softBody();
    [[nodiscard]] const SoftBodyVolume* softBody() const;

    ParticleEmitter& enableParticleEmitter(const std::string& name);
    void disableParticleEmitter();
    [[nodiscard]] bool hasParticleEmitter() const;
    [[nodiscard]] ParticleEmitter* particleEmitter();
    [[nodiscard]] const ParticleEmitter* particleEmitter() const;

    void setMeshResource(const std::string& meshPath);
    [[nodiscard]] const std::string& meshResource() const;
    void setAlbedoTexture(const std::string& texturePath);
    [[nodiscard]] const std::string& albedoTexture() const;
    void setBaseColor(const glm::vec3& color);
    void setBaseColor(const glm::vec4& color) { render().baseColor = color; }
    [[nodiscard]] glm::vec3 baseColor() const;
    [[nodiscard]] glm::vec4 baseColorWithAlpha() const { return render().baseColor; }
    void setMaterial(const Material& material);
    bool setMaterial(const std::string& name);

    [[nodiscard]] core::ecs::Entity entity() const noexcept { return entityHandle; }

private:
    Scene* ownerScene{nullptr};
    core::ecs::Entity entityHandle{};

    core::ecs::Registry& registry();
    const core::ecs::Registry& registry() const;
};

class Scene {
public:
    GameObject& createObject(const std::string& name, MeshType meshType);
    std::vector<GameObject*> createObjects(std::size_t count, MeshType meshType, const std::string& namePrefix = "");
    [[nodiscard]] std::deque<GameObject>& objects() noexcept { return gameObjects; }
    [[nodiscard]] const std::deque<GameObject>& objects() const noexcept { return gameObjects; }
    [[nodiscard]] const std::vector<GameObject*>& objectsCached() const noexcept { return objectCache; }

    [[nodiscard]] Camera& camera() noexcept { return sceneCamera; }
    [[nodiscard]] const Camera& camera() const noexcept { return sceneCamera; }

    Light& createLight(const std::string& name);
    Light& createLight(const LightCreateInfo& info);
    [[nodiscard]] std::vector<Light>& lights() noexcept { return sceneLights; }
    [[nodiscard]] const std::vector<Light>& lights() const noexcept { return sceneLights; }

    [[nodiscard]] MaterialLibrary& materials() noexcept { return materialLibrary; }
    [[nodiscard]] const MaterialLibrary& materials() const noexcept { return materialLibrary; }

    void clear();

    [[nodiscard]] core::ecs::Registry& registry() noexcept { return ecsRegistry; }
    [[nodiscard]] const core::ecs::Registry& registry() const noexcept { return ecsRegistry; }

private:
    Camera sceneCamera{};
    std::deque<GameObject> gameObjects;
    std::vector<GameObject*> objectCache;
    std::vector<Light> sceneLights;
    std::uint64_t nextId = 0;
    core::ecs::Registry ecsRegistry{};
    MaterialLibrary materialLibrary{};
};

class GameEngine : public IGameEngine {
public:
    GameEngine();

    Scene& scene() noexcept override { return activeScene; }
    const Scene& scene() const noexcept override { return activeScene; }

    PhysicsSystem& physics() noexcept override { return physicsSystem; }
    const PhysicsSystem& physics() const noexcept override { return physicsSystem; }

    ParticleSystem& particles() noexcept override { return particleSystem; }
    const ParticleSystem& particles() const noexcept override { return particleSystem; }

    GameObject& createObject(const std::string& name, MeshType meshType) override { return activeScene.createObject(name, meshType); }
    std::vector<GameObject*> createObjects(std::size_t count, MeshType meshType, const std::string& namePrefix = "") { return activeScene.createObjects(count, meshType, namePrefix); }

    void update(float deltaSeconds) override;

private:
    Scene activeScene{};
    PhysicsSystem physicsSystem{};
    ParticleSystem particleSystem{};
};

} // namespace vkengine
