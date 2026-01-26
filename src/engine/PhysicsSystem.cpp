#include "engine/PhysicsSystem.hpp"

#include "engine/PhysicsDetail.hpp"
#include "engine/GameEngine.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace vkengine {
namespace physics_detail {

glm::vec3 worldHalfExtents(const GameObject& object)
{
    const Collider* collider = object.collider();
    if (!collider) {
        return glm::vec3(0.0f);
    }
    return collider->halfExtents * object.transform().scale;
}

glm::vec3 inverseInertiaTensor(const GameObject& object)
{
    const Collider* collider = object.collider();
    const auto& props = object.physics();
    if (!collider || props.mass <= 0.0f || !std::isfinite(props.mass)) {
        return glm::vec3(0.0f);
    }

    const glm::vec3 size = worldHalfExtents(object) * 2.0f;
    const float factor = props.mass / 12.0f;
    const glm::vec3 inertia{
        factor * (size.y * size.y + size.z * size.z),
        factor * (size.x * size.x + size.z * size.z),
        factor * (size.x * size.x + size.y * size.y)
    };

    glm::vec3 inverse{0.0f};
    if (inertia.x > 0.0f) {
        inverse.x = 1.0f / inertia.x;
    }
    if (inertia.y > 0.0f) {
        inverse.y = 1.0f / inertia.y;
    }
    if (inertia.z > 0.0f) {
        inverse.z = 1.0f / inertia.z;
    }
    return inverse;
}

glm::vec3 applyInverseInertia(const glm::vec3& inverse, const glm::vec3& value)
{
    return {inverse.x * value.x, inverse.y * value.y, inverse.z * value.z};
}

glm::vec3 estimateContactPoint(const GameObject& a, const GameObject& b, const glm::vec3& normal)
{
    const glm::vec3 halfA = worldHalfExtents(a);
    const glm::vec3 halfB = worldHalfExtents(b);
    glm::vec3 pointA = a.transform().position;
    glm::vec3 pointB = b.transform().position;
    const glm::vec3 absNormal = glm::abs(normal);

    if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
        const float sign = glm::sign(normal.x);
        pointA.x += halfA.x * sign;
        pointB.x -= halfB.x * sign;
    } else if (absNormal.y > absNormal.z) {
        const float sign = glm::sign(normal.y);
        pointA.y += halfA.y * sign;
        pointB.y -= halfB.y * sign;
    } else {
        const float sign = glm::sign(normal.z);
        pointA.z += halfA.z * sign;
        pointB.z -= halfB.z * sign;
    }

    return 0.5f * (pointA + pointB);
}

bool computePenetration(const AABB& a, const AABB& b, CollisionResult& result)
{
    const glm::vec3 aCenter = (a.min + a.max) * 0.5f;
    const glm::vec3 bCenter = (b.min + b.max) * 0.5f;
    const glm::vec3 delta = bCenter - aCenter;
    const glm::vec3 aHalf = (a.max - a.min) * 0.5f;
    const glm::vec3 bHalf = (b.max - b.min) * 0.5f;

    const glm::vec3 overlap{
        (aHalf.x + bHalf.x) - std::abs(delta.x),
        (aHalf.y + bHalf.y) - std::abs(delta.y),
        (aHalf.z + bHalf.z) - std::abs(delta.z)
    };

    if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
        return false;
    }

    float minOverlap = overlap.x;
    glm::vec3 normal{(delta.x < 0.0f) ? -1.0f : 1.0f, 0.0f, 0.0f};

    if (overlap.y < minOverlap) {
        minOverlap = overlap.y;
        normal = {0.0f, (delta.y < 0.0f) ? -1.0f : 1.0f, 0.0f};
    }
    if (overlap.z < minOverlap) {
        minOverlap = overlap.z;
        normal = {0.0f, 0.0f, (delta.z < 0.0f) ? -1.0f : 1.0f};
    }

    result.normal = normal;
    result.penetrationDepth = minOverlap;
    return true;
}

float combineCoefficient(float a, float b)
{
    const float clampedA = std::max(a, 0.0f);
    const float clampedB = std::max(b, 0.0f);
    const float product = clampedA * clampedB;
    if (product <= 0.0f) {
        return 0.0f;
    }
    return std::sqrt(product);
}
} // namespace physics_detail

using namespace physics_detail;

void PhysicsSystem::update(Scene& scene, float deltaSeconds)
{
    if (deltaSeconds <= 0.0f) {
        return;
    }

    auto& objects = scene.objects();
    if (objects.empty()) {
        return;
    }

    float maxSpeed = 0.0f;
    for (auto& object : objects) {
        auto& properties = object.physics();
        if (!properties.simulate || properties.mass <= 0.0f || !std::isfinite(properties.mass)) {
            properties.lastAcceleration = glm::vec3(0.0f);
            properties.clearForces();
            properties.clearTorques();
            continue;
        }

        const glm::vec3 gravityForce = gravity * properties.mass;
        const glm::vec3 totalForce = gravityForce + properties.accumulatedForces;
        const glm::vec3 acceleration = totalForce / properties.mass;
        maxSpeed = std::max(maxSpeed, glm::length(properties.velocity + acceleration * deltaSeconds));
    }

    constexpr float maxDisplacementPerStep = 0.45f;
    const int subSteps = std::clamp(static_cast<int>(std::ceil((maxSpeed * deltaSeconds) / maxDisplacementPerStep)), 1, 8);
    const float subDelta = deltaSeconds / static_cast<float>(subSteps);

    for (int step = 0; step < subSteps; ++step) {
        for (auto& object : objects) {
            auto& properties = object.physics();
            if (!properties.simulate || properties.mass <= 0.0f || !std::isfinite(properties.mass)) {
                properties.lastAcceleration = glm::vec3(0.0f);
                properties.clearForces();
                properties.clearTorques();
                continue;
            }

            const glm::vec3 gravityForce = gravity * properties.mass;
            const glm::vec3 totalForce = gravityForce + properties.accumulatedForces;
            const glm::vec3 acceleration = totalForce / properties.mass;

            properties.velocity += acceleration * subDelta;
            const float linearDamping = std::clamp(properties.linearDamping, 0.0f, 1.0f);
            properties.velocity *= std::max(0.0f, 1.0f - linearDamping * subDelta);

            auto& transform = object.transform();
            transform.position += properties.velocity * subDelta;

            const glm::vec3 invInertia = inverseInertiaTensor(object);
            const glm::vec3 angularAcceleration{
                invInertia.x * properties.accumulatedTorque.x,
                invInertia.y * properties.accumulatedTorque.y,
                invInertia.z * properties.accumulatedTorque.z
            };

            properties.angularVelocity += angularAcceleration * subDelta;
            const float angularDamping = std::clamp(properties.angularDamping, 0.0f, 1.0f);
            properties.angularVelocity *= std::max(0.0f, 1.0f - angularDamping * subDelta);
            transform.rotation += properties.angularVelocity * subDelta;

            for (int axis = 0; axis < 3; ++axis) {
                transform.rotation[axis] = std::fmod(transform.rotation[axis], glm::two_pi<float>());
                if (transform.rotation[axis] < 0.0f) {
                    transform.rotation[axis] += glm::two_pi<float>();
                }
            }

            properties.lastAcceleration = acceleration;
            properties.clearForces();
            properties.clearTorques();
        }

        resolveCollisions(scene, subDelta);
    }
}

void PhysicsSystem::resolveCollisions(Scene& scene, float deltaSeconds)
{
    auto& objects = scene.objects();
    if (objects.size() < 2) {
        return;
    }

    if (deltaSeconds <= 0.0f) {
        return;
    }

    constexpr float penetrationSlop = 0.0005f;
    constexpr float positionCorrectionPercent = 0.85f;
    constexpr float baumgarteFactor = 0.25f;
    constexpr int velocityIterations = 12;

    const auto computeInverseMass = [](const GameObject& object, const Collider* collider) {
        if (!collider || collider->isStatic) {
            return 0.0f;
        }
        const auto& props = object.physics();
        if (props.mass <= 0.0f || !std::isfinite(props.mass)) {
            return 0.0f;
        }
        return 1.0f / props.mass;
    };

    std::vector<Contact> contacts;
    contacts.reserve(objects.size());

    for (size_t i = 0; i < objects.size(); ++i) {
        for (size_t j = i + 1; j < objects.size(); ++j) {
            GameObject& a = objects[i];
            GameObject& b = objects[j];
            if (!a.hasCollider() || !b.hasCollider()) {
                continue;
            }

            CollisionResult result;
            if (!computePenetration(a.worldBounds(), b.worldBounds(), result)) {
                continue;
            }

            const float invMassA = computeInverseMass(a, a.collider());
            const float invMassB = computeInverseMass(b, b.collider());
            const float invMassSum = invMassA + invMassB;

            if (invMassSum <= 0.0f || result.penetrationDepth <= 0.0f) {
                continue;
            }

            Contact contact{};
            contact.a = &a;
            contact.b = &b;
            contact.normal = result.normal;
            contact.penetration = result.penetrationDepth;
            contact.contactPoint = estimateContactPoint(a, b, result.normal);
            contact.ra = contact.contactPoint - a.transform().position;
            contact.rb = contact.contactPoint - b.transform().position;
            contact.invMassA = invMassA;
            contact.invMassB = invMassB;
            contact.invInertiaA = inverseInertiaTensor(a);
            contact.invInertiaB = inverseInertiaTensor(b);

            auto& propsA = a.physics();
            auto& propsB = b.physics();
            contact.restitution = std::clamp(std::max(propsA.restitution, propsB.restitution), 0.0f, 1.0f);
            contact.staticFriction = combineCoefficient(propsA.staticFriction, propsB.staticFriction);
            contact.dynamicFriction = combineCoefficient(propsA.dynamicFriction, propsB.dynamicFriction);

            contacts.emplace_back(contact);
        }
    }

    if (contacts.empty()) {
        return;
    }

    for (int iteration = 0; iteration < velocityIterations; ++iteration) {
        for (auto& contact : contacts) {
            const float invMassSum = contact.totalInverseMass();
            if (invMassSum <= 0.0f) {
                continue;
            }

            GameObject& objA = *contact.a;
            GameObject& objB = *contact.b;
            auto& propsA = objA.physics();
            auto& propsB = objB.physics();

            glm::vec3 velA = propsA.velocity + glm::cross(propsA.angularVelocity, contact.ra);
            glm::vec3 velB = propsB.velocity + glm::cross(propsB.angularVelocity, contact.rb);
            glm::vec3 relativeVel = velB - velA;

            const float velAlongNormal = glm::dot(relativeVel, contact.normal);
            if (velAlongNormal > 0.0f && contact.penetration <= penetrationSlop) {
                continue;
            }

            const glm::vec3 raCrossN = glm::cross(contact.ra, contact.normal);
            const glm::vec3 rbCrossN = glm::cross(contact.rb, contact.normal);
            const glm::vec3 invInertiaRaCrossN = applyInverseInertia(contact.invInertiaA, raCrossN);
            const glm::vec3 invInertiaRbCrossN = applyInverseInertia(contact.invInertiaB, rbCrossN);
            const glm::vec3 angularFactorVec = glm::cross(invInertiaRaCrossN, contact.ra) + glm::cross(invInertiaRbCrossN, contact.rb);
            const float angularTerm = glm::dot(angularFactorVec, contact.normal);

            float denom = invMassSum + angularTerm;
            if (denom <= std::numeric_limits<float>::epsilon()) {
                continue;
            }

            float penetrationBias = 0.0f;
            if (contact.penetration > penetrationSlop) {
                penetrationBias = -baumgarteFactor * (contact.penetration - penetrationSlop) / deltaSeconds;
            }

            float normalImpulseMag = -(1.0f + contact.restitution) * velAlongNormal + penetrationBias;
            normalImpulseMag /= denom;

            const glm::vec3 impulse = normalImpulseMag * contact.normal;

            if (contact.invMassA > 0.0f) {
                propsA.velocity -= impulse * contact.invMassA;
                propsA.angularVelocity -= applyInverseInertia(contact.invInertiaA, glm::cross(contact.ra, impulse));
            }
            if (contact.invMassB > 0.0f) {
                propsB.velocity += impulse * contact.invMassB;
                propsB.angularVelocity += applyInverseInertia(contact.invInertiaB, glm::cross(contact.rb, impulse));
            }

            velA = propsA.velocity + glm::cross(propsA.angularVelocity, contact.ra);
            velB = propsB.velocity + glm::cross(propsB.angularVelocity, contact.rb);
            relativeVel = velB - velA;
            glm::vec3 tangent = relativeVel - glm::dot(relativeVel, contact.normal) * contact.normal;
            const float tangentLenSq = glm::dot(tangent, tangent);

            if (tangentLenSq > 1e-6f && (contact.staticFriction > 0.0f || contact.dynamicFriction > 0.0f)) {
                tangent /= std::sqrt(tangentLenSq);

                const glm::vec3 raCrossT = glm::cross(contact.ra, tangent);
                const glm::vec3 rbCrossT = glm::cross(contact.rb, tangent);
                const glm::vec3 invInertiaRaCrossT = applyInverseInertia(contact.invInertiaA, raCrossT);
                const glm::vec3 invInertiaRbCrossT = applyInverseInertia(contact.invInertiaB, rbCrossT);
                const glm::vec3 angularFactorVecT = glm::cross(invInertiaRaCrossT, contact.ra) + glm::cross(invInertiaRbCrossT, contact.rb);
                float denomFriction = invMassSum + glm::dot(angularFactorVecT, tangent);

                if (denomFriction > std::numeric_limits<float>::epsilon()) {
                    float jt = -glm::dot(relativeVel, tangent) / denomFriction;
                    const float maxStatic = std::abs(normalImpulseMag) * contact.staticFriction;
                    float frictionImpulseMag = jt;
                    if (std::abs(jt) > maxStatic && contact.dynamicFriction > 0.0f) {
                        frictionImpulseMag = -glm::sign(jt) * std::abs(normalImpulseMag) * contact.dynamicFriction;
                    }

                    const glm::vec3 frictionImpulse = frictionImpulseMag * tangent;

                    if (contact.invMassA > 0.0f) {
                        propsA.velocity -= frictionImpulse * contact.invMassA;
                        propsA.angularVelocity -= applyInverseInertia(contact.invInertiaA, glm::cross(contact.ra, frictionImpulse));
                    }
                    if (contact.invMassB > 0.0f) {
                        propsB.velocity += frictionImpulse * contact.invMassB;
                        propsB.angularVelocity += applyInverseInertia(contact.invInertiaB, glm::cross(contact.rb, frictionImpulse));
                    }
                }
            }
        }
    }

    for (auto& contact : contacts) {
        const float invMassSum = contact.totalInverseMass();
        if (invMassSum <= 0.0f) {
            continue;
        }

        const float penetration = std::max(contact.penetration - penetrationSlop, 0.0f);
        if (penetration <= 0.0f) {
            continue;
        }

        const glm::vec3 correction = (penetration / invMassSum) * positionCorrectionPercent * contact.normal;

        if (contact.invMassA > 0.0f) {
            contact.a->transform().position -= correction * (contact.invMassA / invMassSum);
        }
        if (contact.invMassB > 0.0f) {
            contact.b->transform().position += correction * (contact.invMassB / invMassSum);
        }
    }
}

} // namespace vkengine
