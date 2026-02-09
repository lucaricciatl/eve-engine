#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// Collider Shape Types
// ============================================================================

enum class ColliderType {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    ConvexHull,
    TriangleMesh,
    Compound
};

// ============================================================================
// Basic Shapes
// ============================================================================

struct BoxShape {
    glm::vec3 halfExtents{0.5f};
    glm::vec3 center{0.0f};  // Local offset
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct SphereShape {
    float radius{0.5f};
    glm::vec3 center{0.0f};
};

struct CapsuleShape {
    float radius{0.25f};
    float height{1.0f};  // Total height including caps
    glm::vec3 center{0.0f};
    glm::vec3 axis{0.0f, 1.0f, 0.0f};  // Capsule orientation
};

struct CylinderShape {
    float radius{0.5f};
    float height{1.0f};
    glm::vec3 center{0.0f};
    glm::vec3 axis{0.0f, 1.0f, 0.0f};
};

struct ConvexHullShape {
    std::vector<glm::vec3> vertices;
    std::vector<std::uint32_t> indices;  // Optional, for visualization
    glm::vec3 center{0.0f};
};

struct TriangleMeshShape {
    std::vector<glm::vec3> vertices;
    std::vector<std::uint32_t> indices;
    bool convex{false};  // If true, can be used for dynamic objects
};

// ============================================================================
// Compound Collider (multiple shapes)
// ============================================================================

struct CompoundChild {
    ColliderType type;
    std::variant<BoxShape, SphereShape, CapsuleShape, CylinderShape, ConvexHullShape> shape;
    glm::vec3 localPosition{0.0f};
    glm::quat localRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct CompoundShape {
    std::vector<CompoundChild> children;
};

// ============================================================================
// Unified Collider Shape
// ============================================================================

using ColliderShape = std::variant<
    BoxShape,
    SphereShape,
    CapsuleShape,
    CylinderShape,
    ConvexHullShape,
    TriangleMeshShape,
    CompoundShape
>;

// ============================================================================
// Extended Collider Component
// ============================================================================

struct ColliderEx {
    ColliderType type{ColliderType::Box};
    ColliderShape shape{BoxShape{}};
    bool isStatic{false};
    bool isTrigger{false};  // Trigger colliders don't generate physics response
    std::uint32_t collisionLayer{1};
    std::uint32_t collisionMask{~0u};  // Which layers this collider interacts with

    // Physics material
    float friction{0.5f};
    float restitution{0.3f};
    float density{1.0f};  // For mass calculation

    // Contact callback identifier
    std::string tag;
};

// ============================================================================
// Raycast & Shape Queries
// ============================================================================

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    float maxDistance{1000.0f};
};

struct RaycastHit {
    bool hit{false};
    float distance{0.0f};
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    GameObject* object{nullptr};
    std::string colliderTag;
    std::uint32_t triangleIndex{0};  // For mesh colliders
};

struct ShapeCastHit {
    bool hit{false};
    float distance{0.0f};
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    GameObject* object{nullptr};
    std::string colliderTag;
};

struct OverlapResult {
    std::vector<GameObject*> objects;
    std::vector<std::string> tags;
};

// ============================================================================
// Query Filters
// ============================================================================

struct QueryFilter {
    std::uint32_t layerMask{~0u};
    bool includeTriggers{false};
    bool includeStatic{true};
    bool includeDynamic{true};
    std::vector<GameObject*> ignoreObjects;
};

// ============================================================================
// Physics Query System
// ============================================================================

class PhysicsQuerySystem {
public:
    PhysicsQuerySystem() = default;

    // Raycasting
    [[nodiscard]] RaycastHit raycast(const Scene& scene, const Ray& ray, const QueryFilter& filter = {}) const;
    [[nodiscard]] std::vector<RaycastHit> raycastAll(const Scene& scene, const Ray& ray, const QueryFilter& filter = {}) const;

    // Shape casts
    [[nodiscard]] ShapeCastHit sphereCast(const Scene& scene, const glm::vec3& origin, float radius,
                                          const glm::vec3& direction, float maxDistance, const QueryFilter& filter = {}) const;
    [[nodiscard]] ShapeCastHit boxCast(const Scene& scene, const glm::vec3& origin, const glm::vec3& halfExtents,
                                        const glm::quat& rotation, const glm::vec3& direction, float maxDistance,
                                        const QueryFilter& filter = {}) const;
    [[nodiscard]] ShapeCastHit capsuleCast(const Scene& scene, const glm::vec3& point1, const glm::vec3& point2,
                                            float radius, const glm::vec3& direction, float maxDistance,
                                            const QueryFilter& filter = {}) const;

    // Overlap queries
    [[nodiscard]] OverlapResult overlapSphere(const Scene& scene, const glm::vec3& center, float radius,
                                               const QueryFilter& filter = {}) const;
    [[nodiscard]] OverlapResult overlapBox(const Scene& scene, const glm::vec3& center, const glm::vec3& halfExtents,
                                            const glm::quat& rotation, const QueryFilter& filter = {}) const;
    [[nodiscard]] OverlapResult overlapCapsule(const Scene& scene, const glm::vec3& point1, const glm::vec3& point2,
                                                float radius, const QueryFilter& filter = {}) const;

    // Point queries
    [[nodiscard]] bool pointInside(const Scene& scene, const glm::vec3& point, const QueryFilter& filter = {}) const;
    [[nodiscard]] GameObject* closestObject(const Scene& scene, const glm::vec3& point, float maxDistance,
                                             const QueryFilter& filter = {}) const;

private:
    // Shape intersection tests
    bool rayVsBox(const Ray& ray, const BoxShape& box, const glm::mat4& transform, float& tMin, glm::vec3& normal) const;
    bool rayVsSphere(const Ray& ray, const SphereShape& sphere, const glm::mat4& transform, float& t, glm::vec3& normal) const;
    bool rayVsCapsule(const Ray& ray, const CapsuleShape& capsule, const glm::mat4& transform, float& t, glm::vec3& normal) const;
    bool rayVsCylinder(const Ray& ray, const CylinderShape& cylinder, const glm::mat4& transform, float& t, glm::vec3& normal) const;
    bool rayVsTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                       float& t, glm::vec3& normal, glm::vec2& bary) const;

    bool sphereVsBox(const glm::vec3& center, float radius, const BoxShape& box, const glm::mat4& transform) const;
    bool sphereVsSphere(const glm::vec3& center, float radius, const SphereShape& sphere, const glm::mat4& transform) const;
    bool boxVsBox(const BoxShape& a, const glm::mat4& transformA, const BoxShape& b, const glm::mat4& transformB) const;
};

// ============================================================================
// Collision Manifold
// ============================================================================

struct ContactPoint {
    glm::vec3 pointOnA{0.0f};
    glm::vec3 pointOnB{0.0f};
    glm::vec3 normal{0.0f};  // From A to B
    float penetration{0.0f};
    float normalImpulse{0.0f};
    float tangentImpulse1{0.0f};
    float tangentImpulse2{0.0f};
};

struct CollisionManifold {
    GameObject* objectA{nullptr};
    GameObject* objectB{nullptr};
    std::vector<ContactPoint> contacts;
    glm::vec3 normal{0.0f};
    float friction{0.5f};
    float restitution{0.3f};
    bool persistent{false};  // From previous frame
};

// ============================================================================
// Collision Detection Algorithms
// ============================================================================

namespace collision {

// GJK (Gilbert-Johnson-Keerthi) algorithm for convex shapes
struct GjkResult {
    bool intersecting{false};
    glm::vec3 closestPointA{0.0f};
    glm::vec3 closestPointB{0.0f};
    float distance{0.0f};
};

GjkResult gjkDistance(const std::vector<glm::vec3>& shapeA, const glm::mat4& transformA,
                      const std::vector<glm::vec3>& shapeB, const glm::mat4& transformB);

// EPA (Expanding Polytope Algorithm) for penetration depth
struct EpaResult {
    glm::vec3 normal{0.0f};
    float depth{0.0f};
    glm::vec3 pointOnA{0.0f};
    glm::vec3 pointOnB{0.0f};
};

EpaResult epa(const std::vector<glm::vec3>& shapeA, const glm::mat4& transformA,
              const std::vector<glm::vec3>& shapeB, const glm::mat4& transformB,
              const std::vector<glm::vec3>& simplex);

// SAT (Separating Axis Theorem) for box-box
struct SatResult {
    bool intersecting{false};
    glm::vec3 normal{0.0f};
    float depth{0.0f};
};

SatResult satBoxBox(const BoxShape& a, const glm::mat4& transformA,
                    const BoxShape& b, const glm::mat4& transformB);

// Specialized intersection tests
bool sphereVsSphere(const SphereShape& a, const glm::mat4& transformA,
                    const SphereShape& b, const glm::mat4& transformB,
                    CollisionManifold& manifold);

bool sphereVsBox(const SphereShape& sphere, const glm::mat4& sphereTransform,
                 const BoxShape& box, const glm::mat4& boxTransform,
                 CollisionManifold& manifold);

bool sphereVsCapsule(const SphereShape& sphere, const glm::mat4& sphereTransform,
                     const CapsuleShape& capsule, const glm::mat4& capsuleTransform,
                     CollisionManifold& manifold);

bool capsuleVsCapsule(const CapsuleShape& a, const glm::mat4& transformA,
                      const CapsuleShape& b, const glm::mat4& transformB,
                      CollisionManifold& manifold);

bool capsuleVsBox(const CapsuleShape& capsule, const glm::mat4& capsuleTransform,
                  const BoxShape& box, const glm::mat4& boxTransform,
                  CollisionManifold& manifold);

bool boxVsBox(const BoxShape& a, const glm::mat4& transformA,
              const BoxShape& b, const glm::mat4& transformB,
              CollisionManifold& manifold);

// General collision test (dispatches to specialized functions)
bool testCollision(const ColliderEx& a, const glm::mat4& transformA,
                   const ColliderEx& b, const glm::mat4& transformB,
                   CollisionManifold& manifold);

} // namespace collision

// ============================================================================
// Constraints / Joints
// ============================================================================

enum class ConstraintType {
    Fixed,
    Distance,
    Hinge,
    Slider,
    BallSocket,
    ConeTwist,
    Generic6DOF
};

struct ConstraintLimits {
    bool enabled{false};
    float lower{0.0f};
    float upper{0.0f};
    float stiffness{0.0f};  // 0 = hard limit
    float damping{0.0f};
};

struct FixedConstraint {
    // Locks all degrees of freedom
};

struct DistanceConstraint {
    float minDistance{0.0f};
    float maxDistance{1.0f};
    float stiffness{1.0f};
    float damping{0.0f};
};

struct HingeConstraint {
    glm::vec3 axis{0.0f, 1.0f, 0.0f};
    ConstraintLimits angularLimits;
    bool enableMotor{false};
    float motorTargetVelocity{0.0f};
    float motorMaxForce{0.0f};
};

struct SliderConstraint {
    glm::vec3 axis{1.0f, 0.0f, 0.0f};
    ConstraintLimits linearLimits;
    bool enableMotor{false};
    float motorTargetVelocity{0.0f};
    float motorMaxForce{0.0f};
};

struct BallSocketConstraint {
    // 3 rotational DOF
    ConstraintLimits coneLimits;  // Swing limits
    ConstraintLimits twistLimits; // Twist limits
};

struct ConeTwistConstraint {
    float swingSpan1{glm::radians(45.0f)};
    float swingSpan2{glm::radians(45.0f)};
    float twistSpan{glm::radians(45.0f)};
    float softness{1.0f};
    float biasFactor{0.3f};
    float relaxationFactor{1.0f};
};

struct Generic6DOFConstraint {
    ConstraintLimits linearX;
    ConstraintLimits linearY;
    ConstraintLimits linearZ;
    ConstraintLimits angularX;
    ConstraintLimits angularY;
    ConstraintLimits angularZ;
};

using ConstraintData = std::variant<
    FixedConstraint,
    DistanceConstraint,
    HingeConstraint,
    SliderConstraint,
    BallSocketConstraint,
    ConeTwistConstraint,
    Generic6DOFConstraint
>;

struct PhysicsConstraint {
    std::string name;
    ConstraintType type{ConstraintType::Fixed};
    ConstraintData data;
    GameObject* objectA{nullptr};
    GameObject* objectB{nullptr};  // nullptr = world anchor
    glm::vec3 anchorA{0.0f};  // Local anchor on object A
    glm::vec3 anchorB{0.0f};  // Local anchor on object B (or world position if B is null)
    glm::quat frameA{1.0f, 0.0f, 0.0f, 0.0f};  // Local frame orientation on A
    glm::quat frameB{1.0f, 0.0f, 0.0f, 0.0f};  // Local frame orientation on B
    bool collideConnected{false};
    float breakForce{0.0f};  // 0 = unbreakable
    float breakTorque{0.0f}; // 0 = unbreakable
    bool broken{false};
};

// ============================================================================
// Constraint Solver
// ============================================================================

class ConstraintSolver {
public:
    ConstraintSolver() = default;

    void addConstraint(std::shared_ptr<PhysicsConstraint> constraint);
    void removeConstraint(const std::string& name);
    void removeConstraint(PhysicsConstraint* constraint);
    void clear();

    void solve(float deltaSeconds, int iterations = 10);

    [[nodiscard]] std::vector<std::shared_ptr<PhysicsConstraint>>& constraints() { return activeConstraints; }
    [[nodiscard]] const std::vector<std::shared_ptr<PhysicsConstraint>>& constraints() const { return activeConstraints; }

private:
    void solvePositionConstraint(PhysicsConstraint& constraint, float dt);
    void solveVelocityConstraint(PhysicsConstraint& constraint, float dt);
    void checkBreakage(PhysicsConstraint& constraint);

    std::vector<std::shared_ptr<PhysicsConstraint>> activeConstraints;
};

// ============================================================================
// Collision Callback System
// ============================================================================

enum class CollisionEventType {
    Enter,
    Stay,
    Exit
};

struct CollisionEvent {
    CollisionEventType type;
    GameObject* other;
    CollisionManifold manifold;
};

using CollisionCallback = std::function<void(const CollisionEvent&)>;

struct CollisionCallbackComponent {
    CollisionCallback onEnter;
    CollisionCallback onStay;
    CollisionCallback onExit;
    CollisionCallback onTriggerEnter;
    CollisionCallback onTriggerStay;
    CollisionCallback onTriggerExit;
};

} // namespace vkengine
