#include "engine/PhysicsSystem.hpp"

#include "engine/PhysicsDetail.hpp"
#include "engine/GameEngine.hpp"
#include "engine/GpuCollisionSystem.hpp"

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
    // Apply transform scale to collider half-extents
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
    
    // Add minimum inertia bias to prevent instability with small/thin objects
    constexpr float minInertia = 0.001f;
    const glm::vec3 inertia{
        std::max(minInertia, factor * (size.y * size.y + size.z * size.z)),
        std::max(minInertia, factor * (size.x * size.x + size.z * size.z)),
        std::max(minInertia, factor * (size.x * size.x + size.y * size.y))
    };

    // Clamp inverse inertia to prevent excessive angular response
    constexpr float maxInverseInertia = 100.0f;
    glm::vec3 inverse{0.0f};
    if (inertia.x > 0.0f) {
        inverse.x = std::min(maxInverseInertia, 1.0f / inertia.x);
    }
    if (inertia.y > 0.0f) {
        inverse.y = std::min(maxInverseInertia, 1.0f / inertia.y);
    }
    if (inertia.z > 0.0f) {
        inverse.z = std::min(maxInverseInertia, 1.0f / inertia.z);
    }
    return inverse;
}

glm::vec3 applyInverseInertia(const glm::vec3& inverse, const glm::vec3& value)
{
    return {inverse.x * value.x, inverse.y * value.y, inverse.z * value.z};
}

glm::vec3 estimateContactPoint(const GameObject& a, const GameObject& b, const glm::vec3& normal, float /*penetration*/)
{
    // Estimate contact point on the surface between the two AABBs
    // The contact point should be on the face of the collision
    const glm::vec3& posA = a.transform().position;
    const glm::vec3& posB = b.transform().position;
    
    const AABB boundsA = a.worldBounds();
    const AABB boundsB = b.worldBounds();
    
    // Find the contact point by projecting centers onto the separating plane
    // The contact point lies on the face of object A that faces the collision normal
    glm::vec3 contactOnA = posA;
    glm::vec3 contactOnB = posB;
    
    const glm::vec3 halfA = (boundsA.max - boundsA.min) * 0.5f;
    const glm::vec3 halfB = (boundsB.max - boundsB.min) * 0.5f;
    
    // Move contact point to surface along normal direction
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(normal[axis]) > 0.5f) {
            // This is the collision axis
            contactOnA[axis] = posA[axis] + halfA[axis] * normal[axis];
            contactOnB[axis] = posB[axis] - halfB[axis] * normal[axis];
        } else {
            // For non-collision axes, use overlap region center
            float minOverlap = std::max(boundsA.min[axis], boundsB.min[axis]);
            float maxOverlap = std::min(boundsA.max[axis], boundsB.max[axis]);
            contactOnA[axis] = (minOverlap + maxOverlap) * 0.5f;
            contactOnB[axis] = contactOnA[axis];
        }
    }
    
    // Return midpoint between the two contact estimates
    return 0.5f * (contactOnA + contactOnB);
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

PhysicsSystem::PhysicsSystem()
    : gpuCollisionSystem(std::make_unique<GpuCollisionSystem>())
{
}

PhysicsSystem::~PhysicsSystem() = default;

PhysicsSystem::PhysicsSystem(PhysicsSystem&&) noexcept = default;
PhysicsSystem& PhysicsSystem::operator=(PhysicsSystem&&) noexcept = default;

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

    constexpr float maxDisplacementPerStep = 0.25f;
    const int subSteps = std::clamp(static_cast<int>(std::ceil((maxSpeed * deltaSeconds) / maxDisplacementPerStep)), 1, 16);
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
            
            // Apply stronger angular damping for stability
            const float angularDamping = std::clamp(properties.angularDamping, 0.0f, 1.0f);
            const float effectiveAngularDamping = std::max(angularDamping, 0.1f); // Minimum 10% damping
            properties.angularVelocity *= std::max(0.0f, 1.0f - effectiveAngularDamping * subDelta);

            // Clamp angular velocity to prevent instability
            constexpr float maxAngularSpeed = 15.0f; // Reduced from 20
            const float angularSpeed = glm::length(properties.angularVelocity);
            if (angularSpeed > maxAngularSpeed) {
                properties.angularVelocity *= maxAngularSpeed / angularSpeed;
            }
            
            // Kill very small angular velocities to help objects settle
            if (angularSpeed < 0.01f) {
                properties.angularVelocity = glm::vec3(0.0f);
            }

            // Proper Euler angle rate conversion (simplified for small angles)
            // For Euler XYZ: omega = R_z * R_y * R_x * euler_rate
            // Approximation: euler_rate ≈ omega for small rotations
            const glm::vec3 angularDelta = properties.angularVelocity * subDelta;
            transform.rotation += angularDelta;

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

        // Use GPU collision detection if enabled and initialized
        if (useGpuCollision && gpuCollisionSystem && gpuCollisionSystem->isEnabled()) {
            resolveCollisionsGpu(scene, subDelta);
        } else {
            resolveCollisions(scene, subDelta);
        }
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
    constexpr float positionCorrectionPercent = 0.95f;
    constexpr float baumgarteFactor = 0.4f;
    constexpr int velocityIterations = 16;

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

            // Immediate position correction for deep penetrations (speculative contacts)
            if (result.penetrationDepth > penetrationSlop * 2.0f) {
                const float correctionAmount = (result.penetrationDepth - penetrationSlop) * 0.5f;
                const glm::vec3 correction = correctionAmount * result.normal;
                if (invMassA > 0.0f) {
                    a.transform().position -= correction * (invMassA / invMassSum);
                }
                if (invMassB > 0.0f) {
                    b.transform().position += correction * (invMassB / invMassSum);
                }
                // Recompute penetration after correction
                result.penetrationDepth -= correctionAmount;
            }

            Contact contact{};
            contact.a = &a;
            contact.b = &b;
            contact.normal = result.normal;
            contact.penetration = result.penetrationDepth;
            contact.contactPoint = estimateContactPoint(a, b, result.normal, result.penetrationDepth);
            contact.ra = contact.contactPoint - a.transform().position;
            contact.rb = contact.contactPoint - b.transform().position;
            contact.invMassA = invMassA;
            contact.invMassB = invMassB;
            contact.invInertiaA = inverseInertiaTensor(a);
            contact.invInertiaB = inverseInertiaTensor(b);

            auto& propsA = a.physics();
            auto& propsB = b.physics();
            // Clamp restitution to max 0.9 to guarantee energy loss on every collision
            contact.restitution = std::clamp(std::min(propsA.restitution, propsB.restitution), 0.0f, 0.9f);
            contact.staticFriction = combineCoefficient(propsA.staticFriction, propsB.staticFriction);
            contact.dynamicFriction = combineCoefficient(propsA.dynamicFriction, propsB.dynamicFriction);

            contacts.emplace_back(contact);
        }
    }

    if (contacts.empty()) {
        return;
    }

    // Sequential impulse solver with angular response
    // Uses proper effective mass calculation including rotational inertia
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

            // Compute point velocities including angular contribution
            const glm::vec3 velPointA = propsA.velocity + glm::cross(propsA.angularVelocity, contact.ra);
            const glm::vec3 velPointB = propsB.velocity + glm::cross(propsB.angularVelocity, contact.rb);
            const glm::vec3 relativeVel = velPointB - velPointA;
            const float velAlongNormal = glm::dot(relativeVel, contact.normal);
            
            // Skip if objects are separating and not deeply penetrating
            if (velAlongNormal > 0.0f && contact.penetration <= penetrationSlop) {
                continue;
            }

            // Compute effective mass including angular terms
            // K = 1/m_a + 1/m_b + (r_a x n)^T * I_a^-1 * (r_a x n) + (r_b x n)^T * I_b^-1 * (r_b x n)
            const glm::vec3 raCrossN = glm::cross(contact.ra, contact.normal);
            const glm::vec3 rbCrossN = glm::cross(contact.rb, contact.normal);
            
            float denom = invMassSum;
            // Add angular contribution to effective mass
            const glm::vec3 angularTermA = applyInverseInertia(contact.invInertiaA, raCrossN);
            const glm::vec3 angularTermB = applyInverseInertia(contact.invInertiaB, rbCrossN);
            denom += glm::dot(raCrossN, angularTermA) + glm::dot(rbCrossN, angularTermB);
            
            if (denom <= std::numeric_limits<float>::epsilon()) {
                continue;
            }

            // Baumgarte stabilization for position correction
            float penetrationBias = 0.0f;
            if (contact.penetration > penetrationSlop) {
                penetrationBias = baumgarteFactor * (contact.penetration - penetrationSlop) / deltaSeconds;
            }

            // Calculate impulse magnitude
            float normalImpulseMag = -(1.0f + contact.restitution) * velAlongNormal + penetrationBias;
            normalImpulseMag /= denom;

            // Only apply separating impulse
            normalImpulseMag = std::max(0.0f, normalImpulseMag);
            
            // Clamp impulse to prevent instability
            constexpr float maxNormalImpulse = 50.0f;
            normalImpulseMag = std::min(normalImpulseMag, maxNormalImpulse);

            const glm::vec3 impulse = normalImpulseMag * contact.normal;

            // Apply linear impulse
            if (contact.invMassA > 0.0f) {
                propsA.velocity -= impulse * contact.invMassA;
            }
            if (contact.invMassB > 0.0f) {
                propsB.velocity += impulse * contact.invMassB;
            }
            
            // Apply angular impulse: torque = r x impulse, angular_vel_change = I^-1 * torque
            constexpr float angularImpulseScale = 0.5f; // Scale down angular response for stability
            constexpr float maxAngularImpulse = 5.0f;
            
            if (contact.invMassA > 0.0f && glm::length(contact.invInertiaA) > 0.0f) {
                glm::vec3 angularImpulseA = glm::cross(contact.ra, -impulse);
                angularImpulseA = applyInverseInertia(contact.invInertiaA, angularImpulseA) * angularImpulseScale;
                // Clamp angular impulse magnitude
                const float angImpLen = glm::length(angularImpulseA);
                if (angImpLen > maxAngularImpulse) {
                    angularImpulseA *= maxAngularImpulse / angImpLen;
                }
                propsA.angularVelocity += angularImpulseA;
            }
            if (contact.invMassB > 0.0f && glm::length(contact.invInertiaB) > 0.0f) {
                glm::vec3 angularImpulseB = glm::cross(contact.rb, impulse);
                angularImpulseB = applyInverseInertia(contact.invInertiaB, angularImpulseB) * angularImpulseScale;
                // Clamp angular impulse magnitude
                const float angImpLen = glm::length(angularImpulseB);
                if (angImpLen > maxAngularImpulse) {
                    angularImpulseB *= maxAngularImpulse / angImpLen;
                }
                propsB.angularVelocity += angularImpulseB;
            }

            // Friction impulse (with angular contribution)
            const glm::vec3 newVelPointA = propsA.velocity + glm::cross(propsA.angularVelocity, contact.ra);
            const glm::vec3 newVelPointB = propsB.velocity + glm::cross(propsB.angularVelocity, contact.rb);
            const glm::vec3 newRelativeVel = newVelPointB - newVelPointA;
            glm::vec3 tangent = newRelativeVel - glm::dot(newRelativeVel, contact.normal) * contact.normal;
            const float tangentLenSq = glm::dot(tangent, tangent);

            if (tangentLenSq > 1e-8f && (contact.staticFriction > 0.0f || contact.dynamicFriction > 0.0f)) {
                tangent /= std::sqrt(tangentLenSq);
                
                // Compute friction effective mass
                const glm::vec3 raCrossT = glm::cross(contact.ra, tangent);
                const glm::vec3 rbCrossT = glm::cross(contact.rb, tangent);
                float frictionDenom = invMassSum;
                frictionDenom += glm::dot(raCrossT, applyInverseInertia(contact.invInertiaA, raCrossT));
                frictionDenom += glm::dot(rbCrossT, applyInverseInertia(contact.invInertiaB, rbCrossT));
                
                const float tangentVel = glm::dot(newRelativeVel, tangent);
                float frictionImpulseMag = -tangentVel / frictionDenom;
                
                // Coulomb friction model
                const float maxFriction = normalImpulseMag * contact.staticFriction;
                if (std::abs(frictionImpulseMag) > maxFriction) {
                    frictionImpulseMag = -glm::sign(tangentVel) * normalImpulseMag * contact.dynamicFriction;
                }
                
                // Clamp friction
                constexpr float maxFrictionImpulse = 15.0f;
                frictionImpulseMag = std::clamp(frictionImpulseMag, -maxFrictionImpulse, maxFrictionImpulse);

                const glm::vec3 frictionImpulse = frictionImpulseMag * tangent;

                if (contact.invMassA > 0.0f) {
                    propsA.velocity -= frictionImpulse * contact.invMassA;
                    // Angular friction
                    glm::vec3 angFricA = glm::cross(contact.ra, -frictionImpulse);
                    angFricA = applyInverseInertia(contact.invInertiaA, angFricA) * angularImpulseScale;
                    const float angFricLenA = glm::length(angFricA);
                    if (angFricLenA > maxAngularImpulse) {
                        angFricA *= maxAngularImpulse / angFricLenA;
                    }
                    propsA.angularVelocity += angFricA;
                }
                if (contact.invMassB > 0.0f) {
                    propsB.velocity += frictionImpulse * contact.invMassB;
                    // Angular friction
                    glm::vec3 angFricB = glm::cross(contact.rb, frictionImpulse);
                    angFricB = applyInverseInertia(contact.invInertiaB, angFricB) * angularImpulseScale;
                    const float angFricLenB = glm::length(angFricB);
                    if (angFricLenB > maxAngularImpulse) {
                        angFricB *= maxAngularImpulse / angFricLenB;
                    }
                    propsB.angularVelocity += angFricB;
                }
            }
        }
    }

    // Position correction
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
    
    // Apply contact velocity damping to ensure energy loss for objects in contact
    // This helps objects settle and prevents perpetual bouncing
    constexpr float contactVelocityDamping = 0.995f; // 0.5% velocity reduction per contact
    for (auto& contact : contacts) {
        if (contact.invMassA > 0.0f) {
            auto& propsA = contact.a->physics();
            propsA.velocity *= contactVelocityDamping;
            propsA.angularVelocity *= contactVelocityDamping;
        }
        if (contact.invMassB > 0.0f) {
            auto& propsB = contact.b->physics();
            propsB.velocity *= contactVelocityDamping;
            propsB.angularVelocity *= contactVelocityDamping;
        }
    }
}

void PhysicsSystem::resolveCollisionsGpu(Scene& scene, float deltaSeconds)
{
    auto& objects = scene.objects();
    if (objects.size() < 2 || !gpuCollisionSystem) {
        return;
    }

    if (deltaSeconds <= 0.0f) {
        return;
    }

    // Run GPU collision detection
    auto gpuCollisions = gpuCollisionSystem->detectCollisions(scene);

    if (gpuCollisions.empty()) {
        return;
    }

    constexpr float penetrationSlop = 0.001f;
    constexpr float positionCorrectionPercent = 0.95f;
    constexpr float baumgarteFactor = 0.4f;
    constexpr int velocityIterations = 16;

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

    // Build contact list from GPU collision results
    std::vector<Contact> contacts;
    contacts.reserve(gpuCollisions.size());

    for (const auto& gpuResult : gpuCollisions) {
        if (gpuResult.objectA >= objects.size() || gpuResult.objectB >= objects.size()) {
            continue;
        }

        GameObject& a = objects[gpuResult.objectA];
        GameObject& b = objects[gpuResult.objectB];

        if (!a.hasCollider() || !b.hasCollider()) {
            continue;
        }

        const float invMassA = computeInverseMass(a, a.collider());
        const float invMassB = computeInverseMass(b, b.collider());
        const float invMassSum = invMassA + invMassB;

        if (invMassSum <= 0.0f || gpuResult.penetrationDepth <= 0.0f) {
            continue;
        }

        Contact contact{};
        contact.a = &a;
        contact.b = &b;
        contact.normal = gpuResult.normal;
        contact.penetration = gpuResult.penetrationDepth;
        contact.contactPoint = gpuResult.contactPoint;
        contact.ra = contact.contactPoint - a.transform().position;
        contact.rb = contact.contactPoint - b.transform().position;
        contact.invMassA = invMassA;
        contact.invMassB = invMassB;
        contact.invInertiaA = inverseInertiaTensor(a);
        contact.invInertiaB = inverseInertiaTensor(b);

        auto& propsA = a.physics();
        auto& propsB = b.physics();
        // Clamp restitution to max 0.9 to guarantee energy loss on every collision
        contact.restitution = std::clamp(std::min(propsA.restitution, propsB.restitution), 0.0f, 0.9f);
        contact.staticFriction = combineCoefficient(propsA.staticFriction, propsB.staticFriction);
        contact.dynamicFriction = combineCoefficient(propsA.dynamicFriction, propsB.dynamicFriction);

        contacts.emplace_back(contact);
    }

    if (contacts.empty()) {
        return;
    }

    // Sequential impulse solver with angular response (same as CPU path)
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

            // Compute point velocities including angular contribution
            const glm::vec3 velPointA = propsA.velocity + glm::cross(propsA.angularVelocity, contact.ra);
            const glm::vec3 velPointB = propsB.velocity + glm::cross(propsB.angularVelocity, contact.rb);
            const glm::vec3 relativeVel = velPointB - velPointA;
            const float velAlongNormal = glm::dot(relativeVel, contact.normal);
            
            if (velAlongNormal > 0.0f && contact.penetration <= penetrationSlop) {
                continue;
            }

            // Compute effective mass including angular terms
            const glm::vec3 raCrossN = glm::cross(contact.ra, contact.normal);
            const glm::vec3 rbCrossN = glm::cross(contact.rb, contact.normal);
            
            float denom = invMassSum;
            const glm::vec3 angularTermA = applyInverseInertia(contact.invInertiaA, raCrossN);
            const glm::vec3 angularTermB = applyInverseInertia(contact.invInertiaB, rbCrossN);
            denom += glm::dot(raCrossN, angularTermA) + glm::dot(rbCrossN, angularTermB);
            
            if (denom <= std::numeric_limits<float>::epsilon()) {
                continue;
            }

            float penetrationBias = 0.0f;
            if (contact.penetration > penetrationSlop) {
                penetrationBias = baumgarteFactor * (contact.penetration - penetrationSlop) / deltaSeconds;
            }

            float normalImpulseMag = -(1.0f + contact.restitution) * velAlongNormal + penetrationBias;
            normalImpulseMag /= denom;
            normalImpulseMag = std::max(0.0f, normalImpulseMag);
            
            constexpr float maxNormalImpulse = 50.0f;
            normalImpulseMag = std::min(normalImpulseMag, maxNormalImpulse);

            const glm::vec3 impulse = normalImpulseMag * contact.normal;

            // Apply linear impulse
            if (contact.invMassA > 0.0f) {
                propsA.velocity -= impulse * contact.invMassA;
            }
            if (contact.invMassB > 0.0f) {
                propsB.velocity += impulse * contact.invMassB;
            }
            
            // Apply angular impulse
            constexpr float angularImpulseScale = 0.5f;
            constexpr float maxAngularImpulse = 5.0f;
            
            if (contact.invMassA > 0.0f && glm::length(contact.invInertiaA) > 0.0f) {
                glm::vec3 angularImpulseA = glm::cross(contact.ra, -impulse);
                angularImpulseA = applyInverseInertia(contact.invInertiaA, angularImpulseA) * angularImpulseScale;
                const float angImpLen = glm::length(angularImpulseA);
                if (angImpLen > maxAngularImpulse) {
                    angularImpulseA *= maxAngularImpulse / angImpLen;
                }
                propsA.angularVelocity += angularImpulseA;
            }
            if (contact.invMassB > 0.0f && glm::length(contact.invInertiaB) > 0.0f) {
                glm::vec3 angularImpulseB = glm::cross(contact.rb, impulse);
                angularImpulseB = applyInverseInertia(contact.invInertiaB, angularImpulseB) * angularImpulseScale;
                const float angImpLen = glm::length(angularImpulseB);
                if (angImpLen > maxAngularImpulse) {
                    angularImpulseB *= maxAngularImpulse / angImpLen;
                }
                propsB.angularVelocity += angularImpulseB;
            }

            // Friction with angular contribution
            const glm::vec3 newVelPointA = propsA.velocity + glm::cross(propsA.angularVelocity, contact.ra);
            const glm::vec3 newVelPointB = propsB.velocity + glm::cross(propsB.angularVelocity, contact.rb);
            const glm::vec3 newRelativeVel = newVelPointB - newVelPointA;
            glm::vec3 tangent = newRelativeVel - glm::dot(newRelativeVel, contact.normal) * contact.normal;
            const float tangentLenSq = glm::dot(tangent, tangent);

            if (tangentLenSq > 1e-8f && (contact.staticFriction > 0.0f || contact.dynamicFriction > 0.0f)) {
                tangent /= std::sqrt(tangentLenSq);
                
                // Compute friction effective mass
                const glm::vec3 raCrossT = glm::cross(contact.ra, tangent);
                const glm::vec3 rbCrossT = glm::cross(contact.rb, tangent);
                float frictionDenom = invMassSum;
                frictionDenom += glm::dot(raCrossT, applyInverseInertia(contact.invInertiaA, raCrossT));
                frictionDenom += glm::dot(rbCrossT, applyInverseInertia(contact.invInertiaB, rbCrossT));
                
                const float tangentVel = glm::dot(newRelativeVel, tangent);
                float frictionImpulseMag = -tangentVel / frictionDenom;
                
                const float maxFriction = normalImpulseMag * contact.staticFriction;
                if (std::abs(frictionImpulseMag) > maxFriction) {
                    frictionImpulseMag = -glm::sign(tangentVel) * normalImpulseMag * contact.dynamicFriction;
                }
                
                constexpr float maxFrictionImpulse = 15.0f;
                frictionImpulseMag = std::clamp(frictionImpulseMag, -maxFrictionImpulse, maxFrictionImpulse);

                const glm::vec3 frictionImpulse = frictionImpulseMag * tangent;

                if (contact.invMassA > 0.0f) {
                    propsA.velocity -= frictionImpulse * contact.invMassA;
                    glm::vec3 angFricA = glm::cross(contact.ra, -frictionImpulse);
                    angFricA = applyInverseInertia(contact.invInertiaA, angFricA) * angularImpulseScale;
                    const float angFricLenA = glm::length(angFricA);
                    if (angFricLenA > maxAngularImpulse) {
                        angFricA *= maxAngularImpulse / angFricLenA;
                    }
                    propsA.angularVelocity += angFricA;
                }
                if (contact.invMassB > 0.0f) {
                    propsB.velocity += frictionImpulse * contact.invMassB;
                    glm::vec3 angFricB = glm::cross(contact.rb, frictionImpulse);
                    angFricB = applyInverseInertia(contact.invInertiaB, angFricB) * angularImpulseScale;
                    const float angFricLenB = glm::length(angFricB);
                    if (angFricLenB > maxAngularImpulse) {
                        angFricB *= maxAngularImpulse / angFricLenB;
                    }
                    propsB.angularVelocity += angFricB;
                }
            }
        }
    }

    // Position correction
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
    
    // Apply contact velocity damping to ensure energy loss
    constexpr float contactVelocityDamping = 0.995f;
    for (auto& contact : contacts) {
        if (contact.invMassA > 0.0f) {
            auto& propsA = contact.a->physics();
            propsA.velocity *= contactVelocityDamping;
            propsA.angularVelocity *= contactVelocityDamping;
        }
        if (contact.invMassB > 0.0f) {
            auto& propsB = contact.b->physics();
            propsB.velocity *= contactVelocityDamping;
            propsB.angularVelocity *= contactVelocityDamping;
        }
    }
}

} // namespace vkengine
