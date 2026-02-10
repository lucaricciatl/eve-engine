#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <cmath>
#include <limits>
#include <string>

#include "engine/GameEngine.hpp"
#include "engine/PhysicsDetail.hpp"
#include "engine/PhysicsSystem.hpp"

namespace {

using namespace vkengine;
using namespace vkengine::physics_detail;

GameObject& createDynamicCube(Scene& scene, const std::string& name, const glm::vec3& position,
                              const glm::vec3& halfExtents, float mass = 1.0f)
{
    auto& object = scene.createObject(name, MeshType::Cube);
    object.transform().position = position;
    object.transform().scale = glm::vec3(1.0f);
    object.enableCollider(halfExtents, /*isStatic=*/false);
    auto& props = object.physics();
    props.mass = mass;
    props.simulate = true;
    props.collidable = true;
    props.velocity = glm::vec3(0.0f);
    props.angularVelocity = glm::vec3(0.0f);
    props.accumulatedForces = glm::vec3(0.0f);
    props.accumulatedTorque = glm::vec3(0.0f);
    props.staticFriction = 0.5f;
    props.dynamicFriction = 0.4f;
    props.restitution = 0.4f;
    return object;
}

TEST(PhysicsHelpers, ContactReportsTotalInverseMass)
{
    Contact contact;
    contact.invMassA = 0.25f;
    contact.invMassB = 0.75f;
    EXPECT_FLOAT_EQ(contact.totalInverseMass(), 1.0f);
}

TEST(PhysicsHelpers, WorldHalfExtentsRespectScale)
{
    Scene scene;
    auto& object = scene.createObject("Scaled", MeshType::Cube);
    object.transform().scale = {2.0f, 1.0f, 0.5f};
    object.enableCollider(glm::vec3(1.0f, 2.0f, 3.0f), false);

    EXPECT_EQ(worldHalfExtents(object), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(PhysicsHelpers, InverseInertiaTensorZeroWhenStatic)
{
    Scene scene;
    auto& object = scene.createObject("Static", MeshType::Cube);
    object.enableCollider(glm::vec3(1.0f), /*isStatic=*/true);

    EXPECT_EQ(inverseInertiaTensor(object), glm::mat3(0.0f));
}

TEST(PhysicsHelpers, InverseInertiaTensorMatchesBox)
{
    Scene scene;
    auto& object = createDynamicCube(scene, "Dynamic", glm::vec3(0.0f), glm::vec3(0.5f), 2.0f);

    const glm::vec3 size = worldHalfExtents(object) * 2.0f;
    const float factor = object.physics().mass / 12.0f;
    const glm::vec3 expected{
        factor * (size.y * size.y + size.z * size.z),
        factor * (size.x * size.x + size.z * size.z),
        factor * (size.x * size.x + size.y * size.y)
    };

    const glm::mat3 body = inertiaTensorBody(object);
    EXPECT_FLOAT_EQ(body[0][0], expected.x);
    EXPECT_FLOAT_EQ(body[1][1], expected.y);
    EXPECT_FLOAT_EQ(body[2][2], expected.z);

    const glm::mat3 inverse = inverseInertiaTensor(object);
    EXPECT_FLOAT_EQ(inverse[0][0], 1.0f / expected.x);
    EXPECT_FLOAT_EQ(inverse[1][1], 1.0f / expected.y);
    EXPECT_FLOAT_EQ(inverse[2][2], 1.0f / expected.z);
}

TEST(PhysicsHelpers, ApplyInverseInertiaScalesComponents)
{
    const glm::mat3 inverse{
        2.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 4.0f
    };
    const glm::vec3 value{1.0f, 2.0f, -3.0f};
    EXPECT_EQ(applyInverseInertia(inverse, value), glm::vec3(2.0f, 1.0f, -12.0f));
}

TEST(PhysicsHelpers, EstimateContactPointChoosesDominantAxis)
{
    Scene scene;
    auto& a = createDynamicCube(scene, "A", {-0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
    auto& b = createDynamicCube(scene, "B", {0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});

    const glm::vec3 normal{1.0f, 0.0f, 0.0f};
    const glm::vec3 contact = estimateContactPoint(a, b, normal);

    EXPECT_FLOAT_EQ(contact.x, 0.0f);
    EXPECT_FLOAT_EQ(contact.y, 0.0f);
    EXPECT_FLOAT_EQ(contact.z, 0.0f);
}

TEST(PhysicsHelpers, ComputePenetrationDetectsOverlap)
{
    Scene scene;
    auto& a = createDynamicCube(scene, "A", {0.0f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.5f});
    auto& b = createDynamicCube(scene, "B", {0.25f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.5f});

    CollisionResult result;
    EXPECT_TRUE(computePenetration(a.worldBounds(), b.worldBounds(), result));
    EXPECT_NEAR(result.penetrationDepth, 0.75f, 1e-4f);
    EXPECT_EQ(result.normal.x, 1.0f);
}

TEST(PhysicsHelpers, CombineCoefficientUsesGeometricMean)
{
    EXPECT_FLOAT_EQ(combineCoefficient(0.6f, 0.2f), std::sqrt(0.12f));
    EXPECT_FLOAT_EQ(combineCoefficient(-1.0f, 0.5f), 0.0f);
}

TEST(PhysicsSystemTests, UpdateAppliesGravityAndDamping)
{
    Scene scene;
    auto& falling = createDynamicCube(scene, "Faller", {0.0f, 10.0f, 0.0f}, glm::vec3(0.5f));
    auto& props = falling.physics();
    props.linearDamping = 0.0f;

    PhysicsSystem system;
    system.setGravity({0.0f, -9.81f, 0.0f});
    system.update(scene, 0.5f);

    EXPECT_LT(falling.transform().position.y, 10.0f);
    EXPECT_NEAR(props.velocity.y, -9.81f * 0.5f, 0.5f);
    EXPECT_NEAR(glm::length(props.lastAcceleration), 9.81f, 0.1f);
}

TEST(PhysicsSystemTests, CollisionsSeparateObjects)
{
    Scene scene;
    auto& left = createDynamicCube(scene, "Left", {-0.4f, 0.0f, 0.0f}, glm::vec3(0.5f));
    auto& right = createDynamicCube(scene, "Right", {0.4f, 0.0f, 0.0f}, glm::vec3(0.5f));

    PhysicsSystem system;
    system.setGravity({0.0f, 0.0f, 0.0f});
    system.update(scene, 0.016f);

    const float separation = std::abs(right.transform().position.x - left.transform().position.x);
    EXPECT_GT(separation, 0.82f);
    EXPECT_GE(right.transform().position.x, left.transform().position.x);
}

TEST(PhysicsSystemTests, AngularMomentumConservedWithoutTorque)
{
    Scene scene;
    auto& object = createDynamicCube(scene, "Spin", {0.0f, 0.0f, 0.0f}, glm::vec3(0.3f, 0.6f, 0.9f), 2.0f);
    object.physics().angularVelocity = glm::vec3(0.2f, 0.6f, -0.4f);
    object.physics().angularDamping = 0.0f;
    object.physics().accumulatedTorque = glm::vec3(0.0f);

    const glm::mat3 inertiaStart = inertiaTensor(object);
    const glm::vec3 angularMomentumStart = inertiaStart * object.physics().angularVelocity;

    PhysicsSystem system;
    system.setGravity({0.0f, 0.0f, 0.0f});
    system.update(scene, 0.016f);

    const glm::mat3 inertiaEnd = inertiaTensor(object);
    const glm::vec3 angularMomentumEnd = inertiaEnd * object.physics().angularVelocity;

    const float startMag = glm::length(angularMomentumStart);
    const float endMag = glm::length(angularMomentumEnd);
    EXPECT_NEAR(startMag, endMag, 1e-2f);
}

TEST(PhysicsSystemTests, AngularDampingDissipatesEnergy)
{
    Scene scene;
    auto& object = createDynamicCube(scene, "DampedSpin", {0.0f, 0.0f, 0.0f}, glm::vec3(0.4f), 2.0f);
    object.physics().angularVelocity = glm::vec3(0.0f, 2.0f, 0.0f);
    object.physics().angularDamping = 0.0f;
    object.physics().dynamicFriction = 0.8f;
    object.physics().restitution = 0.1f;
    object.physics().accumulatedTorque = glm::vec3(0.0f);

    PhysicsSystem system;
    system.setGravity({0.0f, 0.0f, 0.0f});
    system.update(scene, 0.5f);

    const float speedAfter = glm::length(object.physics().angularVelocity);
    EXPECT_LT(speedAfter, 2.0f);
}

TEST(PhysicsSystemTests, HigherMaterialDissipationSlowsSpinMore)
{
    Scene scene;
    auto& low = createDynamicCube(scene, "LowDiss", {0.0f, 0.0f, 0.0f}, glm::vec3(0.4f), 2.0f);
    auto& high = createDynamicCube(scene, "HighDiss", {0.0f, 0.0f, 0.0f}, glm::vec3(0.4f), 2.0f);

    low.physics().angularVelocity = glm::vec3(0.0f, 2.0f, 0.0f);
    high.physics().angularVelocity = glm::vec3(0.0f, 2.0f, 0.0f);

    low.physics().angularDamping = 0.0f;
    high.physics().angularDamping = 0.0f;

    low.physics().dynamicFriction = 0.1f;
    low.physics().restitution = 0.9f;

    high.physics().dynamicFriction = 0.9f;
    high.physics().restitution = 0.1f;

    PhysicsSystem system;
    system.setGravity({0.0f, 0.0f, 0.0f});
    system.update(scene, 0.5f);

    const float lowSpeed = glm::length(low.physics().angularVelocity);
    const float highSpeed = glm::length(high.physics().angularVelocity);
    EXPECT_LT(highSpeed, lowSpeed);
}

} // namespace
