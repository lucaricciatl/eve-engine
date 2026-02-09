/**
 * @file Colliders.cpp
 * @brief Implementation of physics query system and collision detection algorithms
 */

#include "engine/Colliders.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace vkengine {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {
    constexpr float EPSILON = 1e-6f;
    
    inline float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(value, maxVal));
    }
    
    inline glm::vec3 closestPointOnLineSegment(const glm::vec3& a, const glm::vec3& b, const glm::vec3& point) {
        glm::vec3 ab = b - a;
        float t = glm::dot(point - a, ab) / glm::dot(ab, ab);
        t = clamp(t, 0.0f, 1.0f);
        return a + t * ab;
    }
}

// ============================================================================
// PhysicsQuerySystem Implementation
// ============================================================================

RaycastHit PhysicsQuerySystem::raycast(const Scene& /*scene*/, const Ray& ray, const QueryFilter& /*filter*/) const {
    RaycastHit result;
    result.hit = false;
    result.distance = ray.maxDistance;
    // TODO: Iterate through scene objects and test ray against colliders
    return result;
}

std::vector<RaycastHit> PhysicsQuerySystem::raycastAll(const Scene& /*scene*/, const Ray& ray, const QueryFilter& /*filter*/) const {
    std::vector<RaycastHit> results;
    // TODO: Iterate through scene objects and collect all ray hits
    (void)ray;
    return results;
}

ShapeCastHit PhysicsQuerySystem::sphereCast(const Scene& /*scene*/, const glm::vec3& /*origin*/, float /*radius*/,
                                             const glm::vec3& /*direction*/, float /*maxDistance*/, const QueryFilter& /*filter*/) const {
    ShapeCastHit result;
    result.hit = false;
    return result;
}

ShapeCastHit PhysicsQuerySystem::boxCast(const Scene& /*scene*/, const glm::vec3& /*origin*/, const glm::vec3& /*halfExtents*/,
                                          const glm::quat& /*rotation*/, const glm::vec3& /*direction*/, float /*maxDistance*/,
                                          const QueryFilter& /*filter*/) const {
    ShapeCastHit result;
    result.hit = false;
    return result;
}

ShapeCastHit PhysicsQuerySystem::capsuleCast(const Scene& /*scene*/, const glm::vec3& /*point1*/, const glm::vec3& /*point2*/,
                                              float /*radius*/, const glm::vec3& /*direction*/, float /*maxDistance*/,
                                              const QueryFilter& /*filter*/) const {
    ShapeCastHit result;
    result.hit = false;
    return result;
}

OverlapResult PhysicsQuerySystem::overlapSphere(const Scene& /*scene*/, const glm::vec3& /*center*/, float /*radius*/,
                                                 const QueryFilter& /*filter*/) const {
    OverlapResult result;
    return result;
}

OverlapResult PhysicsQuerySystem::overlapBox(const Scene& /*scene*/, const glm::vec3& /*center*/, const glm::vec3& /*halfExtents*/,
                                              const glm::quat& /*rotation*/, const QueryFilter& /*filter*/) const {
    OverlapResult result;
    return result;
}

OverlapResult PhysicsQuerySystem::overlapCapsule(const Scene& /*scene*/, const glm::vec3& /*point1*/, const glm::vec3& /*point2*/,
                                                  float /*radius*/, const QueryFilter& /*filter*/) const {
    OverlapResult result;
    return result;
}

bool PhysicsQuerySystem::pointInside(const Scene& /*scene*/, const glm::vec3& /*point*/, const QueryFilter& /*filter*/) const {
    return false;
}

GameObject* PhysicsQuerySystem::closestObject(const Scene& /*scene*/, const glm::vec3& /*point*/, float /*maxDistance*/,
                                               const QueryFilter& /*filter*/) const {
    return nullptr;
}

// Private ray intersection tests
bool PhysicsQuerySystem::rayVsBox(const Ray& ray, const BoxShape& box, const glm::mat4& transform, float& tMin, glm::vec3& normal) const {
    glm::mat4 invTransform = glm::inverse(transform);
    glm::vec3 localOrigin = glm::vec3(invTransform * glm::vec4(ray.origin, 1.0f));
    glm::vec3 localDir = glm::normalize(glm::vec3(invTransform * glm::vec4(ray.direction, 0.0f)));
    
    glm::vec3 minBound = box.center - box.halfExtents;
    glm::vec3 maxBound = box.center + box.halfExtents;
    
    float tNear = -std::numeric_limits<float>::max();
    float tFar = std::numeric_limits<float>::max();
    glm::vec3 nearNormal(0.0f);
    
    for (int i = 0; i < 3; ++i) {
        if (std::abs(localDir[i]) < EPSILON) {
            if (localOrigin[i] < minBound[i] || localOrigin[i] > maxBound[i]) {
                return false;
            }
        } else {
            float t1 = (minBound[i] - localOrigin[i]) / localDir[i];
            float t2 = (maxBound[i] - localOrigin[i]) / localDir[i];
            glm::vec3 n(0.0f);
            n[i] = localDir[i] > 0 ? -1.0f : 1.0f;
            
            if (t1 > t2) {
                std::swap(t1, t2);
                n[i] = -n[i];
            }
            
            if (t1 > tNear) {
                tNear = t1;
                nearNormal = n;
            }
            tFar = std::min(tFar, t2);
            
            if (tNear > tFar || tFar < 0) {
                return false;
            }
        }
    }
    
    tMin = tNear > 0 ? tNear : tFar;
    if (tMin < 0 || tMin > ray.maxDistance) {
        return false;
    }
    
    // Transform normal back to world space
    glm::mat3 normalMat = glm::transpose(glm::mat3(invTransform));
    normal = glm::normalize(normalMat * nearNormal);
    return true;
}

bool PhysicsQuerySystem::rayVsSphere(const Ray& ray, const SphereShape& sphere, const glm::mat4& transform, float& t, glm::vec3& normal) const {
    glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(sphere.center, 1.0f));
    glm::vec3 oc = ray.origin - worldCenter;
    
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return false;
    }
    
    float sqrtD = std::sqrt(discriminant);
    float t0 = (-b - sqrtD) / (2.0f * a);
    float t1 = (-b + sqrtD) / (2.0f * a);
    
    t = t0 >= 0 ? t0 : t1;
    if (t < 0 || t > ray.maxDistance) {
        return false;
    }
    
    glm::vec3 hitPoint = ray.origin + ray.direction * t;
    normal = glm::normalize(hitPoint - worldCenter);
    return true;
}

bool PhysicsQuerySystem::rayVsCapsule(const Ray& ray, const CapsuleShape& capsule, const glm::mat4& transform, float& t, glm::vec3& normal) const {
    // Transform capsule to world space
    glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(capsule.center, 1.0f));
    glm::vec3 worldAxis = glm::normalize(glm::vec3(transform * glm::vec4(capsule.axis, 0.0f)));
    
    float halfHeight = (capsule.height - 2.0f * capsule.radius) * 0.5f;
    glm::vec3 point1 = worldCenter + worldAxis * halfHeight;
    glm::vec3 point2 = worldCenter - worldAxis * halfHeight;
    
    // Find closest point on capsule line segment to ray
    glm::vec3 closestOnSegment = closestPointOnLineSegment(point1, point2, ray.origin);
    
    // Test ray against sphere at closest point
    SphereShape testSphere;
    testSphere.radius = capsule.radius;
    testSphere.center = glm::vec3(0.0f);
    
    glm::mat4 sphereTransform = glm::translate(glm::mat4(1.0f), closestOnSegment);
    return rayVsSphere(ray, testSphere, sphereTransform, t, normal);
}

bool PhysicsQuerySystem::rayVsCylinder(const Ray& ray, const CylinderShape& cylinder, const glm::mat4& transform, float& t, glm::vec3& normal) const {
    (void)ray; (void)cylinder; (void)transform; (void)t; (void)normal;
    // TODO: Implement ray vs cylinder intersection
    return false;
}

bool PhysicsQuerySystem::rayVsTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                        float& t, glm::vec3& normal, glm::vec2& bary) const {
    // Möller–Trumbore intersection algorithm
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);
    
    if (std::abs(a) < EPSILON) {
        return false;
    }
    
    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);
    
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    
    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);
    
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    
    t = f * glm::dot(edge2, q);
    if (t < EPSILON || t > ray.maxDistance) {
        return false;
    }
    
    bary = glm::vec2(u, v);
    normal = glm::normalize(glm::cross(edge1, edge2));
    return true;
}

bool PhysicsQuerySystem::sphereVsBox(const glm::vec3& center, float radius, const BoxShape& box, const glm::mat4& transform) const {
    glm::mat4 invTransform = glm::inverse(transform);
    glm::vec3 localCenter = glm::vec3(invTransform * glm::vec4(center, 1.0f));
    
    // Find closest point on box to sphere center
    glm::vec3 closest;
    closest.x = clamp(localCenter.x, box.center.x - box.halfExtents.x, box.center.x + box.halfExtents.x);
    closest.y = clamp(localCenter.y, box.center.y - box.halfExtents.y, box.center.y + box.halfExtents.y);
    closest.z = clamp(localCenter.z, box.center.z - box.halfExtents.z, box.center.z + box.halfExtents.z);
    
    float distSq = glm::dot(localCenter - closest, localCenter - closest);
    return distSq <= radius * radius;
}

bool PhysicsQuerySystem::sphereVsSphere(const glm::vec3& center, float radius, const SphereShape& sphere, const glm::mat4& transform) const {
    glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(sphere.center, 1.0f));
    float totalRadius = radius + sphere.radius;
    return glm::dot(center - worldCenter, center - worldCenter) <= totalRadius * totalRadius;
}

bool PhysicsQuerySystem::boxVsBox(const BoxShape& a, const glm::mat4& transformA, const BoxShape& b, const glm::mat4& transformB) const {
    (void)a; (void)transformA; (void)b; (void)transformB;
    // TODO: Implement SAT-based box vs box test
    return false;
}

// ============================================================================
// ConstraintSolver Implementation
// ============================================================================

void ConstraintSolver::addConstraint(std::shared_ptr<PhysicsConstraint> constraint) {
    if (constraint) {
        activeConstraints.push_back(std::move(constraint));
    }
}

void ConstraintSolver::removeConstraint(const std::string& name) {
    activeConstraints.erase(
        std::remove_if(activeConstraints.begin(), activeConstraints.end(),
                       [&name](const auto& c) { return c->name == name; }),
        activeConstraints.end());
}

void ConstraintSolver::removeConstraint(PhysicsConstraint* constraint) {
    activeConstraints.erase(
        std::remove_if(activeConstraints.begin(), activeConstraints.end(),
                       [constraint](const auto& c) { return c.get() == constraint; }),
        activeConstraints.end());
}

void ConstraintSolver::clear() {
    activeConstraints.clear();
}

void ConstraintSolver::solve(float deltaSeconds, int iterations) {
    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& constraint : activeConstraints) {
            if (constraint && !constraint->broken) {
                solvePositionConstraint(*constraint, deltaSeconds);
            }
        }
    }
    
    for (auto& constraint : activeConstraints) {
        if (constraint && !constraint->broken) {
            solveVelocityConstraint(*constraint, deltaSeconds);
            checkBreakage(*constraint);
        }
    }
}

void ConstraintSolver::solvePositionConstraint(PhysicsConstraint& /*constraint*/, float /*dt*/) {
    // TODO: Implement position constraint solving based on constraint type
}

void ConstraintSolver::solveVelocityConstraint(PhysicsConstraint& /*constraint*/, float /*dt*/) {
    // TODO: Implement velocity constraint solving based on constraint type
}

void ConstraintSolver::checkBreakage(PhysicsConstraint& constraint) {
    // TODO: Check if constraint forces exceed break thresholds
    // For now, just stub
    if (constraint.breakForce > 0.0f || constraint.breakTorque > 0.0f) {
        // Would check accumulated impulses here
    }
}

// ============================================================================
// collision namespace functions
// ============================================================================

namespace collision {

GjkResult gjkDistance(const std::vector<glm::vec3>& /*shapeA*/, const glm::mat4& /*transformA*/,
                      const std::vector<glm::vec3>& /*shapeB*/, const glm::mat4& /*transformB*/) {
    GjkResult result;
    result.intersecting = false;
    result.distance = 0.0f;
    // TODO: Implement GJK algorithm
    return result;
}

EpaResult epa(const std::vector<glm::vec3>& /*shapeA*/, const glm::mat4& /*transformA*/,
              const std::vector<glm::vec3>& /*shapeB*/, const glm::mat4& /*transformB*/,
              const std::vector<glm::vec3>& /*simplex*/) {
    EpaResult result;
    result.depth = 0.0f;
    // TODO: Implement EPA algorithm
    return result;
}

SatResult satBoxBox(const BoxShape& /*a*/, const glm::mat4& /*transformA*/,
                    const BoxShape& /*b*/, const glm::mat4& /*transformB*/) {
    SatResult result;
    result.intersecting = false;
    result.depth = 0.0f;
    // TODO: Implement SAT for box-box
    return result;
}

bool sphereVsSphere(const SphereShape& a, const glm::mat4& transformA,
                    const SphereShape& b, const glm::mat4& transformB,
                    CollisionManifold& manifold) {
    glm::vec3 centerA = glm::vec3(transformA * glm::vec4(a.center, 1.0f));
    glm::vec3 centerB = glm::vec3(transformB * glm::vec4(b.center, 1.0f));
    
    glm::vec3 diff = centerB - centerA;
    float distSq = glm::dot(diff, diff);
    float totalRadius = a.radius + b.radius;
    
    if (distSq > totalRadius * totalRadius) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    manifold.normal = dist > EPSILON ? diff / dist : glm::vec3(1, 0, 0);
    
    ContactPoint contact;
    contact.penetration = totalRadius - dist;
    contact.normal = manifold.normal;
    contact.pointOnA = centerA + manifold.normal * a.radius;
    contact.pointOnB = centerB - manifold.normal * b.radius;
    manifold.contacts.push_back(contact);
    
    return true;
}

bool sphereVsBox(const SphereShape& sphere, const glm::mat4& sphereTransform,
                 const BoxShape& box, const glm::mat4& boxTransform,
                 CollisionManifold& manifold) {
    glm::vec3 sphereCenter = glm::vec3(sphereTransform * glm::vec4(sphere.center, 1.0f));
    glm::mat4 invBoxTransform = glm::inverse(boxTransform);
    glm::vec3 localCenter = glm::vec3(invBoxTransform * glm::vec4(sphereCenter, 1.0f));
    
    // Find closest point on box
    glm::vec3 closest;
    closest.x = clamp(localCenter.x, box.center.x - box.halfExtents.x, box.center.x + box.halfExtents.x);
    closest.y = clamp(localCenter.y, box.center.y - box.halfExtents.y, box.center.y + box.halfExtents.y);
    closest.z = clamp(localCenter.z, box.center.z - box.halfExtents.z, box.center.z + box.halfExtents.z);
    
    glm::vec3 diff = localCenter - closest;
    float distSq = glm::dot(diff, diff);
    
    if (distSq > sphere.radius * sphere.radius) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    glm::vec3 localNormal = dist > EPSILON ? diff / dist : glm::vec3(1, 0, 0);
    
    // Transform back to world space
    glm::mat3 rotation = glm::mat3(boxTransform);
    manifold.normal = glm::normalize(rotation * localNormal);
    
    ContactPoint contact;
    contact.penetration = sphere.radius - dist;
    contact.normal = manifold.normal;
    contact.pointOnA = sphereCenter - manifold.normal * sphere.radius;
    contact.pointOnB = glm::vec3(boxTransform * glm::vec4(closest, 1.0f));
    manifold.contacts.push_back(contact);
    
    return true;
}

bool sphereVsCapsule(const SphereShape& sphere, const glm::mat4& sphereTransform,
                     const CapsuleShape& capsule, const glm::mat4& capsuleTransform,
                     CollisionManifold& manifold) {
    glm::vec3 sphereCenter = glm::vec3(sphereTransform * glm::vec4(sphere.center, 1.0f));
    glm::vec3 capsuleCenter = glm::vec3(capsuleTransform * glm::vec4(capsule.center, 1.0f));
    glm::vec3 capsuleAxis = glm::normalize(glm::vec3(capsuleTransform * glm::vec4(capsule.axis, 0.0f)));
    
    float halfHeight = (capsule.height - 2.0f * capsule.radius) * 0.5f;
    glm::vec3 p1 = capsuleCenter + capsuleAxis * halfHeight;
    glm::vec3 p2 = capsuleCenter - capsuleAxis * halfHeight;
    
    glm::vec3 closest = closestPointOnLineSegment(p1, p2, sphereCenter);
    glm::vec3 diff = sphereCenter - closest;
    float distSq = glm::dot(diff, diff);
    float totalRadius = sphere.radius + capsule.radius;
    
    if (distSq > totalRadius * totalRadius) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    manifold.normal = dist > EPSILON ? diff / dist : glm::vec3(1, 0, 0);
    
    ContactPoint contact;
    contact.penetration = totalRadius - dist;
    contact.normal = manifold.normal;
    contact.pointOnA = sphereCenter - manifold.normal * sphere.radius;
    contact.pointOnB = closest + manifold.normal * capsule.radius;
    manifold.contacts.push_back(contact);
    
    return true;
}

bool capsuleVsCapsule(const CapsuleShape& a, const glm::mat4& transformA,
                      const CapsuleShape& b, const glm::mat4& transformB,
                      CollisionManifold& manifold) {
    (void)a; (void)transformA; (void)b; (void)transformB; (void)manifold;
    // TODO: Implement capsule vs capsule collision
    return false;
}

bool capsuleVsBox(const CapsuleShape& capsule, const glm::mat4& capsuleTransform,
                  const BoxShape& box, const glm::mat4& boxTransform,
                  CollisionManifold& manifold) {
    (void)capsule; (void)capsuleTransform; (void)box; (void)boxTransform; (void)manifold;
    // TODO: Implement capsule vs box collision
    return false;
}

bool boxVsBox(const BoxShape& a, const glm::mat4& transformA,
              const BoxShape& b, const glm::mat4& transformB,
              CollisionManifold& manifold) {
    (void)a; (void)transformA; (void)b; (void)transformB; (void)manifold;
    // TODO: Implement box vs box collision using SAT
    return false;
}

bool testCollision(const ColliderEx& a, const glm::mat4& transformA,
                   const ColliderEx& b, const glm::mat4& transformB,
                   CollisionManifold& manifold) {
    manifold.contacts.clear();
    manifold.friction = std::sqrt(a.friction * b.friction);
    manifold.restitution = std::max(a.restitution, b.restitution);
    
    // Dispatch based on collider types
    // This is a simplified version - a full implementation would handle all combinations
    
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Sphere) {
        auto& sphereA = std::get<SphereShape>(a.shape);
        auto& sphereB = std::get<SphereShape>(b.shape);
        return sphereVsSphere(sphereA, transformA, sphereB, transformB, manifold);
    }
    
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Box) {
        auto& sphere = std::get<SphereShape>(a.shape);
        auto& box = std::get<BoxShape>(b.shape);
        return sphereVsBox(sphere, transformA, box, transformB, manifold);
    }
    
    if (a.type == ColliderType::Box && b.type == ColliderType::Sphere) {
        auto& box = std::get<BoxShape>(a.shape);
        auto& sphere = std::get<SphereShape>(b.shape);
        bool result = sphereVsBox(sphere, transformB, box, transformA, manifold);
        if (result) {
            manifold.normal = -manifold.normal;
            for (auto& contact : manifold.contacts) {
                std::swap(contact.pointOnA, contact.pointOnB);
                contact.normal = -contact.normal;
            }
        }
        return result;
    }
    
    if (a.type == ColliderType::Box && b.type == ColliderType::Box) {
        auto& boxA = std::get<BoxShape>(a.shape);
        auto& boxB = std::get<BoxShape>(b.shape);
        return boxVsBox(boxA, transformA, boxB, transformB, manifold);
    }
    
    // TODO: Handle other collision type combinations
    return false;
}

} // namespace collision

} // namespace vkengine
