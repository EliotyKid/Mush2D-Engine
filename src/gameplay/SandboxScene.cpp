#include "SandboxScene.hpp"

#include "Spawn.hpp"

ISceneDefinition::BuildResult SandboxScene::build(Scene2D& scene) {
    scene.clear();

    auto& leftWall = Spawn::wall(
        scene,
        {-4.5f, 0.0f},
        {0.8f, 4.0f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 0,
            .size = {0.8f, 4.0f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    leftWall.name = "LeftWall";

    auto& rightWall = Spawn::wall(
        scene,
        {4.5f, 0.0f},
        {0.8f, 4.0f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 1,
            .size = {0.8f, 4.0f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    rightWall.name = "RightWall";

    auto& topWall = Spawn::wall(
        scene,
        {0.0f, 2.5f},
        {8.0f, 0.8f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 2,
            .size = {8.0f, 0.8f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    topWall.name = "TopWall";

    auto& bottomWall = Spawn::wall(
        scene,
        {0.0f, -2.5f},
        {8.0f, 0.8f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 0,
            .orderInLayer = 3,
            .size = {8.0f, 0.8f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    bottomWall.name = "BottomWall";

    auto& centerBox = Spawn::wall(
        scene,
        {1.5f, 0.0f},
        {1.0f, 1.0f},
        WallObject::Config{
            .textureIndex = 0,
            .layer = 1,
            .orderInLayer = 0,
            .size = {1.0f, 1.0f},
            .colliderOffset = {0.0f, 0.0f}
        }
    );
    centerBox.name = "CenterBox";

    auto& checkpoint = Spawn::checkpoint(
        scene,
        {-1.5f, 1.2f},
        CheckpointObject::Config{
            .textureIndex = 1,
            .layer = 1,
            .orderInLayer = 1,
            .triggerSize = {1.2f, 1.2f},
            .oneShot = false
        }
    );
    checkpoint.name = "CheckpointTrigger";

    auto& teleport = Spawn::teleport(
        scene,
        {2.8f, 1.2f},
        {-3.0f, -1.5f},
        TeleportObject::Config{
            .textureIndex = 1,
            .layer = 1,
            .orderInLayer = 2,
            .triggerSize = {1.2f, 1.2f},
            .oneShot = false
        }
    );
    teleport.name = "TeleportTrigger";

    auto& slime = Spawn::slime(
        scene,
        {3.0f, 0.0f},
        SlimeEnemy::Config{
            .moveSpeed = 1.2f,
            .patrolDistance = 2.5f,
            .maxHealth = 20,
            .textureIndex = 0,
            .colliderSize = {0.8f, 0.8f},
            .layer = 1,
            .orderInLayer = 3,
            .initialScale = {0.8f, 0.8f}
        }
    );
    slime.name = "Slime01";

    auto& player = Spawn::player(
        scene,
        {0.0f, 0.0f},
        PlayerObject::Config{
            .moveSpeed = 2.5f,
            .dashSpeed = 6.5f,
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