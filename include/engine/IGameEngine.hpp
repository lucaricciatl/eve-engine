#pragma once

#include <string>

namespace vkengine {

class Scene;
class PhysicsSystem;
class ParticleSystem;
class GameObject;
enum class MeshType : int;

class IGameEngine {
public:
    virtual ~IGameEngine() = default;

    virtual Scene& scene() noexcept = 0;
    virtual const Scene& scene() const noexcept = 0;

    virtual PhysicsSystem& physics() noexcept = 0;
    virtual const PhysicsSystem& physics() const noexcept = 0;

    virtual ParticleSystem& particles() noexcept = 0;
    virtual const ParticleSystem& particles() const noexcept = 0;

    virtual GameObject& createObject(const std::string& name, MeshType meshType) = 0;

    virtual void update(float deltaSeconds) = 0;
};

} // namespace vkengine
