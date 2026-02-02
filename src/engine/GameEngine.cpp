#include "engine/GameEngine.hpp"

#include "core/ParallelFor.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace vkengine {

namespace {
constexpr float clampPitch(float pitch) noexcept
{
    const float limit = glm::radians(89.0f);
    return std::clamp(pitch, -limit, limit);
}

std::string makeName(const std::string& requested, const char* prefix, std::uint64_t& counter)
{
    if (!requested.empty()) {
        return requested;
    }
    return std::string(prefix) + std::to_string(counter++);
}
} // namespace

GameEngine::GameEngine()
{
    particleSystem.attachRegistry(&activeScene.registry());
}

glm::mat4 Transform::matrix() const
{
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});
    glm::mat4 rotationMatrix = rotationZ * rotationY * rotationX;
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);
    return translation * rotationMatrix * scaleMatrix;
}

GameObject::GameObject(Scene& owner, core::ecs::Entity entity)
    : ownerScene(&owner)
    , entityHandle(entity)
{
}

core::ecs::Registry& GameObject::registry()
{
    return ownerScene->registry();
}

const core::ecs::Registry& GameObject::registry() const
{
    return ownerScene->registry();
}

const std::string& GameObject::name() const
{
    return registry().get<NameComponent>(entityHandle).value;
}

MeshType GameObject::mesh() const
{
    return registry().get<RenderComponent>(entityHandle).mesh;
}

Transform& GameObject::transform()
{
    return registry().get<Transform>(entityHandle);
}

const Transform& GameObject::transform() const
{
    return registry().get<Transform>(entityHandle);
}

PhysicsProperties& GameObject::physics()
{
    return registry().get<PhysicsProperties>(entityHandle);
}

const PhysicsProperties& GameObject::physics() const
{
    return registry().get<PhysicsProperties>(entityHandle);
}

RenderComponent& GameObject::render()
{
    return registry().get<RenderComponent>(entityHandle);
}

const RenderComponent& GameObject::render() const
{
    return registry().get<RenderComponent>(entityHandle);
}

Collider& GameObject::enableCollider(const glm::vec3& halfExtents, bool isStatic)
{
    auto& colliderComponent = registry().getOrEmplace<ColliderComponent>(entityHandle);
    colliderComponent.shape = Collider{halfExtents, isStatic};

    auto& physicsComponent = physics();
    physicsComponent.collidable = true;
    physicsComponent.simulate = !isStatic;
    if (isStatic) {
        physicsComponent.mass = std::numeric_limits<float>::infinity();
        physicsComponent.velocity = glm::vec3(0.0f);
        physicsComponent.angularVelocity = glm::vec3(0.0f);
    }
    physicsComponent.clearTorques();
    return colliderComponent.shape.value();
}

void GameObject::disableCollider()
{
    registry().remove<ColliderComponent>(entityHandle);
    physics().collidable = false;
}

bool GameObject::hasCollider() const
{
    auto* colliderComponent = registry().tryGet<ColliderComponent>(entityHandle);
    return colliderComponent && colliderComponent->shape.has_value();
}

Collider* GameObject::collider()
{
    auto* colliderComponent = registry().tryGet<ColliderComponent>(entityHandle);
    if (!colliderComponent) {
        return nullptr;
    }
    if (!colliderComponent->shape) {
        return nullptr;
    }
    return &colliderComponent->shape.value();
}

const Collider* GameObject::collider() const
{
    const auto* colliderComponent = registry().tryGet<ColliderComponent>(entityHandle);
    if (!colliderComponent) {
        return nullptr;
    }
    if (!colliderComponent->shape) {
        return nullptr;
    }
    return &colliderComponent->shape.value();
}

void GameObject::setMeshResource(const std::string& meshPath)
{
    auto& renderComponent = render();
    renderComponent.mesh = MeshType::CustomMesh;
    renderComponent.meshResource = meshPath;
}

const std::string& GameObject::meshResource() const
{
    return render().meshResource;
}

void GameObject::setAlbedoTexture(const std::string& texturePath)
{
    auto& renderComponent = render();
    renderComponent.albedoTexture = texturePath;
}

const std::string& GameObject::albedoTexture() const
{
    return render().albedoTexture;
}

void GameObject::setBaseColor(const glm::vec3& color)
{
    render().baseColor = glm::vec4{color, 1.0f};
}

glm::vec3 GameObject::baseColor() const
{
    const auto color = render().baseColor;
    return glm::vec3{color};
}

void GameObject::setMaterial(const Material& material)
{
    auto& renderComponent = render();
    renderComponent.materialName = material.name;
    renderComponent.baseColor = material.baseColor;
    renderComponent.metallic = material.metallic;
    renderComponent.roughness = material.roughness;
    renderComponent.specular = material.specular;
    renderComponent.emissive = material.emissive;
    renderComponent.emissiveIntensity = material.emissiveIntensity;
    renderComponent.opacity = material.opacity;
    renderComponent.albedoTexture = material.albedoTexture;
}

bool GameObject::setMaterial(const std::string& name)
{
    if (!ownerScene) {
        return false;
    }
    const Material* material = ownerScene->materials().find(name);
    if (!material) {
        return false;
    }
    setMaterial(*material);
    return true;
}

AABB GameObject::worldBounds() const
{
    const Collider* colliderData = collider();
    const auto& transformComponent = transform();
    if (!colliderData) {
        return {transformComponent.position, transformComponent.position};
    }
    const glm::vec3 halfSize = colliderData->halfExtents * transformComponent.scale;
    return {transformComponent.position - halfSize, transformComponent.position + halfSize};
}

DeformableBody& GameObject::enableDeformableCloth(int width, int height, float spacing)
{
    auto& deformableComponent = registry().getOrEmplace<DeformableComponent>(entityHandle);
    deformableComponent.cloth = std::make_shared<DeformableBody>(width, height, spacing);
    registry().remove<SoftBodyComponent>(entityHandle);
    registry().get<RenderComponent>(entityHandle).mesh = MeshType::DeformableCloth;
    return *deformableComponent.cloth;
}

bool GameObject::hasDeformableBody() const
{
    const auto* deformableComponent = registry().tryGet<DeformableComponent>(entityHandle);
    return deformableComponent && deformableComponent->cloth;
}

DeformableBody* GameObject::deformable()
{
    auto* deformableComponent = registry().tryGet<DeformableComponent>(entityHandle);
    return (deformableComponent) ? deformableComponent->cloth.get() : nullptr;
}

const DeformableBody* GameObject::deformable() const
{
    const auto* deformableComponent = registry().tryGet<DeformableComponent>(entityHandle);
    return (deformableComponent) ? deformableComponent->cloth.get() : nullptr;
}

SoftBodyVolume& GameObject::enableSoftBodyVolume(int sizeX, int sizeY, int sizeZ, float spacing)
{
    auto& softBodyComponent = registry().getOrEmplace<SoftBodyComponent>(entityHandle);
    softBodyComponent.volume = std::make_shared<SoftBodyVolume>(sizeX, sizeY, sizeZ, spacing);
    registry().remove<DeformableComponent>(entityHandle);
    registry().get<RenderComponent>(entityHandle).mesh = MeshType::SoftBodyVolume;
    return *softBodyComponent.volume;
}

bool GameObject::hasSoftBodyVolume() const
{
    const auto* softBodyComponent = registry().tryGet<SoftBodyComponent>(entityHandle);
    return softBodyComponent && softBodyComponent->volume;
}

SoftBodyVolume* GameObject::softBody()
{
    auto* softBodyComponent = registry().tryGet<SoftBodyComponent>(entityHandle);
    return (softBodyComponent) ? softBodyComponent->volume.get() : nullptr;
}

const SoftBodyVolume* GameObject::softBody() const
{
    const auto* softBodyComponent = registry().tryGet<SoftBodyComponent>(entityHandle);
    return (softBodyComponent) ? softBodyComponent->volume.get() : nullptr;
}

ParticleEmitter& GameObject::enableParticleEmitter(const std::string& name)
{
    auto& component = registry().getOrEmplace<ParticleEmitterComponent>(entityHandle);
    if (!component.emitter) {
        const std::string emitterName = name.empty() ? (this->name() + "_Emitter") : name;
        component.emitter = std::make_unique<ParticleEmitter>(emitterName);
    }
    component.enabled = true;
    return *component.emitter;
}

void GameObject::disableParticleEmitter()
{
    registry().remove<ParticleEmitterComponent>(entityHandle);
}

bool GameObject::hasParticleEmitter() const
{
    const auto* component = registry().tryGet<ParticleEmitterComponent>(entityHandle);
    return component && component->emitter != nullptr;
}

ParticleEmitter* GameObject::particleEmitter()
{
    auto* component = registry().tryGet<ParticleEmitterComponent>(entityHandle);
    return component ? component->emitter.get() : nullptr;
}

const ParticleEmitter* GameObject::particleEmitter() const
{
    const auto* component = registry().tryGet<ParticleEmitterComponent>(entityHandle);
    return component ? component->emitter.get() : nullptr;
}

Light::Light(Scene& owner, core::ecs::Entity entity)
    : ownerScene(&owner)
    , entityHandle(entity)
{
}

core::ecs::Registry& Light::registry()
{
    return ownerScene->registry();
}

const core::ecs::Registry& Light::registry() const
{
    return ownerScene->registry();
}

const std::string& Light::name() const
{
    return registry().get<LightComponent>(entityHandle).name;
}

void Light::setPosition(const glm::vec3& newPosition)
{
    registry().get<LightComponent>(entityHandle).position = newPosition;
}

const glm::vec3& Light::position() const
{
    return registry().get<LightComponent>(entityHandle).position;
}

void Light::setColor(const glm::vec3& newColor)
{
    registry().get<LightComponent>(entityHandle).color = newColor;
}

const glm::vec3& Light::color() const
{
    return registry().get<LightComponent>(entityHandle).color;
}

void Light::setIntensity(float newIntensity)
{
    registry().get<LightComponent>(entityHandle).intensity = newIntensity;
}

float Light::intensity() const
{
    return registry().get<LightComponent>(entityHandle).intensity;
}

void Light::setType(LightType newType)
{
    registry().get<LightComponent>(entityHandle).type = newType;
}

LightType Light::type() const
{
    return registry().get<LightComponent>(entityHandle).type;
}

void Light::setDirection(const glm::vec3& newDirection)
{
    registry().get<LightComponent>(entityHandle).direction = newDirection;
}

const glm::vec3& Light::direction() const
{
    return registry().get<LightComponent>(entityHandle).direction;
}

void Light::setRange(float newRange)
{
    registry().get<LightComponent>(entityHandle).range = newRange;
}

float Light::range() const
{
    return registry().get<LightComponent>(entityHandle).range;
}

void Light::setConeAngles(float innerRadians, float outerRadians)
{
    auto& component = registry().get<LightComponent>(entityHandle);
    component.innerConeAngle = innerRadians;
    component.outerConeAngle = outerRadians;
}

float Light::innerConeAngle() const
{
    return registry().get<LightComponent>(entityHandle).innerConeAngle;
}

float Light::outerConeAngle() const
{
    return registry().get<LightComponent>(entityHandle).outerConeAngle;
}

void Light::setAreaSize(const glm::vec2& newSize)
{
    registry().get<LightComponent>(entityHandle).areaSize = newSize;
}

const glm::vec2& Light::areaSize() const
{
    return registry().get<LightComponent>(entityHandle).areaSize;
}

void Light::setUp(const glm::vec3& newUp)
{
    registry().get<LightComponent>(entityHandle).up = newUp;
}

const glm::vec3& Light::up() const
{
    return registry().get<LightComponent>(entityHandle).up;
}

void Light::setEnabled(bool value)
{
    registry().get<LightComponent>(entityHandle).enabled = value;
}

bool Light::isEnabled() const
{
    return registry().get<LightComponent>(entityHandle).enabled;
}

void Camera::lookAt(const glm::vec3& target) noexcept
{
    glm::vec3 dir = glm::normalize(target - position);
    pitch = clampPitch(std::asin(dir.y));
    yaw = std::atan2(dir.z, dir.x);
}

void Camera::setYawPitch(float newYaw, float newPitch) noexcept
{
    yaw = newYaw;
    pitch = clampPitch(newPitch);
}

void Camera::rotate(float deltaYaw, float deltaPitch) noexcept
{
    setYawPitch(yaw + deltaYaw, pitch + deltaPitch);
}

void Camera::setPerspective(float fovDegrees, float nearPlane, float farPlane) noexcept
{
    fieldOfView = glm::radians(fovDegrees);
    nearClip = nearPlane;
    farClip = farPlane;
}

void Camera::moveLocal(const glm::vec3& delta) noexcept
{
    const glm::vec3 fwd = forward();
    const glm::vec3 side = right();
    position += fwd * delta.z;
    position += side * delta.x;
    position += worldUp * delta.y;
}

void Camera::applyInput(const CameraInput& input, float deltaSeconds) noexcept
{
    const float dt = std::max(deltaSeconds, 0.0f);
    const glm::vec3 moveDelta = input.translation * movementSpeed * input.speedMultiplier * dt;
    moveLocal(moveDelta);

    const glm::vec2 lookDelta = input.rotation * rotationSpeed * dt;
    rotate(lookDelta.x, lookDelta.y);
}

glm::vec3 Camera::forward() const
{
    glm::vec3 direction;
    direction.x = std::cos(pitch) * std::cos(yaw);
    direction.y = std::sin(pitch);
    direction.z = std::cos(pitch) * std::sin(yaw);
    return glm::normalize(direction);
}

glm::vec3 Camera::right() const
{
    return glm::normalize(glm::cross(forward(), worldUp));
}

glm::mat4 Camera::viewMatrix() const
{
    return glm::lookAt(position, position + forward(), worldUp);
}

glm::mat4 Camera::projectionMatrix(float aspectRatio) const
{
    glm::mat4 proj = glm::perspective(fieldOfView, aspectRatio, nearClip, farClip);
    proj[1][1] *= -1.0f;
    return proj;
}

GameObject& Scene::createObject(const std::string& name, MeshType meshType)
{
    auto entity = ecsRegistry.createEntity();
    ecsRegistry.emplace<NameComponent>(entity, NameComponent{makeName(name, "Object_", nextId)});
    ecsRegistry.emplace<Transform>(entity, Transform{});
    ecsRegistry.emplace<PhysicsProperties>(entity, PhysicsProperties{});
    ecsRegistry.emplace<RenderComponent>(entity, RenderComponent{meshType});
    ecsRegistry.emplace<ColliderComponent>(entity, ColliderComponent{});
    gameObjects.emplace_back(*this, entity);
    return gameObjects.back();
}

Light& Scene::createLight(const std::string& name)
{
    LightCreateInfo info{};
    info.name = name;
    return createLight(info);
}

Light& Scene::createLight(const LightCreateInfo& info)
{
    auto entity = ecsRegistry.createEntity();
    LightComponent component{};
    component.name = makeName(info.name, "Light_", nextId);
    component.position = info.position;
    component.color = info.color;
    component.intensity = info.intensity;
    component.type = info.type;
    component.direction = info.direction;
    component.range = info.range;
    component.innerConeAngle = info.innerConeAngle;
    component.outerConeAngle = info.outerConeAngle;
    component.areaSize = info.areaSize;
    component.up = info.up;
    component.enabled = info.enabled;
    ecsRegistry.emplace<LightComponent>(entity, std::move(component));
    sceneLights.emplace_back(*this, entity);
    return sceneLights.back();
}

void Scene::clear()
{
    nextId = 0;
    ecsRegistry.clear();
    gameObjects.clear();
    sceneLights.clear();
}

void GameEngine::update(float deltaSeconds)
{
    physicsSystem.update(activeScene, deltaSeconds);
    particleSystem.update(deltaSeconds);

    std::vector<DeformableBody*> cloths;
    std::vector<SoftBodyVolume*> softBodies;
    cloths.reserve(activeScene.objects().size());
    softBodies.reserve(activeScene.objects().size());

    for (auto& object : activeScene.objects()) {
        if (auto* cloth = object.deformable()) {
            cloths.push_back(cloth);
        }
        if (auto* soft = object.softBody()) {
            softBodies.push_back(soft);
        }
    }

    const auto gravity = physicsSystem.getGravity();
    core::parallelFor(cloths.size(), 8, [&](std::size_t index) {
        cloths[index]->simulate(deltaSeconds, gravity);
    });
    core::parallelFor(softBodies.size(), 8, [&](std::size_t index) {
        softBodies[index]->simulate(deltaSeconds, gravity);
    });
}

} // namespace vkengine
