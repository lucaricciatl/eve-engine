#include <gtest/gtest.h>


#include <glm/glm.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "engine/GameEngine.hpp"

namespace {


TEST(GameEngineTests, StartsWithEmptyScene) {
    vkengine::GameEngine engine;

    EXPECT_TRUE(engine.scene().objects().empty());
    EXPECT_TRUE(engine.scene().lights().empty());
}

TEST(GameEngineTests, CameraDefaultsAreReasonable) {
    vkengine::GameEngine engine;
    const auto& camera = engine.scene().camera();

    EXPECT_FLOAT_EQ(camera.getMovementSpeed(), 3.0f);
    EXPECT_GT(glm::length(camera.getPosition()), 0.0f);
}

TEST(SceneTests, CreateObjectAssignsDefaults) {
    vkengine::Scene scene;
    auto& object = scene.createObject("", vkengine::MeshType::Cube);

    ASSERT_EQ(scene.objects().size(), 1);
    EXPECT_EQ(object.mesh(), vkengine::MeshType::Cube);
    EXPECT_EQ(object.name(), "Object_0");
    EXPECT_EQ(object.transform().scale, glm::vec3(1.0f));
    EXPECT_FALSE(object.hasCollider());
    EXPECT_FALSE(object.physics().collidable);
}

TEST(GameObjectTests, EnableColliderConfiguresPhysics) {
    vkengine::Scene scene;
    auto& object = scene.createObject("StaticBox", vkengine::MeshType::Cube);

    auto& collider = object.enableCollider(glm::vec3(2.0f, 1.0f, 3.0f), /*isStatic=*/true);

    EXPECT_TRUE(object.hasCollider());
    EXPECT_TRUE(collider.isStatic);
    EXPECT_EQ(collider.halfExtents, glm::vec3(2.0f, 1.0f, 3.0f));
    EXPECT_TRUE(std::isinf(object.physics().mass));
    EXPECT_TRUE(object.physics().collidable);

    object.disableCollider();
    EXPECT_FALSE(object.hasCollider());
    EXPECT_FALSE(object.physics().collidable);
}

TEST(SceneTests, CreateLightAutoNamesAndAcceptsCustomInfo) {
    vkengine::Scene scene;
    auto& generated = scene.createLight("");
    EXPECT_EQ(generated.name(), "Light_0");
    EXPECT_TRUE(generated.isEnabled());

    vkengine::LightCreateInfo info{};
    info.name = "KeyLight";
    info.intensity = 4.5f;
    info.color = glm::vec3(0.8f, 0.7f, 0.6f);
    auto& custom = scene.createLight(info);

    EXPECT_EQ(custom.name(), "KeyLight");
    EXPECT_FLOAT_EQ(custom.intensity(), 4.5f);
    EXPECT_EQ(custom.color(), info.color);
    EXPECT_EQ(scene.lights().size(), 2);
}

TEST(SceneTests, ClearResetsObjectsLightsAndIds) {
    vkengine::Scene scene;
    scene.createObject("Box", vkengine::MeshType::Cube);
    scene.createLight("Fill");

    scene.clear();

    EXPECT_TRUE(scene.objects().empty());
    EXPECT_TRUE(scene.lights().empty());

    auto& renamed = scene.createObject("", vkengine::MeshType::Cube);
    EXPECT_EQ(renamed.name(), "Object_0");
}

TEST(CameraTests, ApplyInputMovesAndRotatesCamera) {
    vkengine::Camera camera;
    vkengine::CameraInput input{};
    input.translation = glm::vec3(1.0f, 0.0f, 1.0f);
    input.rotation = glm::vec2(0.1f, 0.05f);
    input.speedMultiplier = 2.0f;

    camera.applyInput(input, 0.5f);

    const auto& position = camera.getPosition();
    EXPECT_NEAR(position.x, 3.0f, 1e-4f);
    EXPECT_NEAR(position.z, 2.0f, 1e-4f);

    const glm::vec3 fwd = camera.forwardVector();
    EXPECT_GT(fwd.x, 0.0f);
    EXPECT_LT(fwd.z, 0.0f);
    EXPECT_GT(fwd.y, 0.0f);
}

TEST(GameObjectTests, MeshResourceSwitchesToCustomMesh) {
    vkengine::Scene scene;
    auto& object = scene.createObject("Actor", vkengine::MeshType::Cube);

    object.setMeshResource("assets/models/actor.gltf");
    EXPECT_EQ(object.mesh(), vkengine::MeshType::CustomMesh);
    EXPECT_EQ(object.meshResource(), "assets/models/actor.gltf");
}

TEST(SanityCheck, BasicMath) {
    EXPECT_EQ(2 + 2, 4);
}


} // namespace
