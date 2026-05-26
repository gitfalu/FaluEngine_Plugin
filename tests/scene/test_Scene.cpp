#include <gtest/gtest.h>
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"

using namespace FaluEngine;

TEST(Scene, CreateEntity) {
    Scene scene("TestScene");
    Entity e = scene.createEntity("MyEntity");
    EXPECT_TRUE(e.isValid());
}

TEST(Scene, EntityHasDefaultComponents) {
    Scene scene("TestScene");
    Entity e = scene.createEntity("Hero");

    EXPECT_TRUE(e.hasComponent<TagComponent>());
    EXPECT_TRUE(e.hasComponent<TransformComponent>());
    EXPECT_EQ(e.getComponent<TagComponent>().name, "Hero");
}

TEST(Scene, AddAndGetComponent) {
    Scene scene("TestScene");
    Entity e = scene.createEntity();

    auto& mesh = e.addComponent<MeshComponent>();
    mesh.meshPath = "assets/meshes/cube.obj";

    EXPECT_TRUE(e.hasComponent<MeshComponent>());
    EXPECT_EQ(e.getComponent<MeshComponent>().meshPath, "assets/meshes/cube.obj");
}

TEST(Scene, DestroyEntity) {
    Scene scene("TestScene");
    Entity e = scene.createEntity();
    scene.destroyEntity(e);
    EXPECT_FALSE(e.isValid());
}
