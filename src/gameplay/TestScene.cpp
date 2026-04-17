#include "TestScene.hpp"

#include "Spawn.hpp"

ISceneDefinition::BuildResult TestScene::build(Scene2D& scene) {
    scene.clear();

    auto& floorTop = Spawn::wall(
        scene,
        {0.0f, 2.8f},
        {10.0f, 0.6f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 0,
            .size = {10.0f, 0.6f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    floorTop.name = "TestTopWall";

    auto& floorBottom = Spawn::wall(
        scene,
        {0.0f, -2.8f},
        {10.0f, 0.6f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 1,
            .size = {10.0f, 0.6f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    floorBottom.name = "TestBottomWall";

    auto& leftWall = Spawn::wall(
        scene,
        {-5.0f, 0.0f},
        {0.6f, 6.0f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 2,
            .size = {0.6f, 6.0f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    leftWall.name = "TestLeftWall";

    auto& rightWall = Spawn::wall(
        scene,
        {5.0f, 0.0f},
        {0.6f, 6.0f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 3,
            .size = {0.6f, 6.0f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    rightWall.name = "TestRightWall";

    auto& slimeA = Spawn::slime(
        scene,
        {-2.5f, 0.0f},
        SlimeEnemy::Config{
            .moveSpeed = 1.0f,
            .patrolDistance = 1.5f,
            .maxHealth = 10,
            .textureIndex = 0,
            .colliderSize = {0.8f, 0.8f},
            .layer = 1,
            .orderInLayer = 0,
            .initialScale = {0.8f, 0.8f}
        }
    );
    slimeA.name = "TestSlimeA";

    auto& slimeB = Spawn::slime(
        scene,
        {2.5f, 0.0f},
        SlimeEnemy::Config{
            .moveSpeed = 1.8f,
            .patrolDistance = 2.2f,
            .maxHealth = 20,
            .textureIndex = 0,
            .colliderSize = {0.9f, 0.9f},
            .layer = 1,
            .orderInLayer = 1,
            .initialScale = {0.9f, 0.9f}
        }
    );
    slimeB.name = "TestSlimeB";

    auto& checkpoint = Spawn::checkpoint(
        scene,
        {0.0f, 1.8f},
        CheckpointObject::Config{
            .textureIndex = 1,
            .layer = 1,
            .orderInLayer = 2,
            .triggerSize = {1.2f, 1.2f},
            .oneShot = false
        }
    );
    checkpoint.name = "TestCheckpoint";

    auto& teleport = Spawn::teleport(
        scene,
        {0.0f, -1.8f},
        {0.0f, 0.0f},
        TeleportObject::Config{
            .textureIndex = 1,
            .layer = 1,
            .orderInLayer = 3,
            .triggerSize = {1.2f, 1.2f},
            .oneShot = false
        }
    );
    teleport.name = "TestTeleport";

    auto& player = Spawn::player(
        scene,
        {0.0f, 0.0f},
        PlayerObject::Config{
            .moveSpeed = 2.8f,
            .dashSpeed = 7.0f,
            .dashDuration = 0.18f,
            .hitDuration = 0.35f,
            .textureIndex = 1,
            .colliderSize = {0.6f, 0.6f},
            .layer = 2,
            .orderInLayer = 0,
            .initialScale = {0.7f, 0.7f}
        }
    );

    return BuildResult{
        .playerObjectId = player.id
    };
}