#include "TestScene.hpp"

#include "../objects/PlayerObject.hpp"
#include "../spawners/Spawn.hpp"

#include <cmath>

ISceneDefinition::BuildResult TestScene::build(
    Scene2D& scene,
    const SceneTransitionData& transitionData
) {
    scene.clear();

    auto& floorTop = Spawn::wall(
        scene,
        {0.0f, 2.8f},
        {10.0f, 0.6f},
        WallObject::Config{
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
            .colliderSize = {0.6f, 0.6f},
            .layer = 2,
            .orderInLayer = 0,
            .initialScale = {0.7f, 0.7f}
        }
    );

    if (transitionData.overridePlayerSpawn) {
        player.transform.position = transitionData.playerSpawnPosition;
        player.setCheckpointPosition(transitionData.playerSpawnPosition);
    }

    return BuildResult{
        .playerObjectId = player.id
    };
}

void TestScene::onEnter(Scene2D& scene, const SceneTransitionData& transitionData) {
    initialized = true;
    elapsedTime = 0.0f;
    pulseCycle = 0;
}

void TestScene::onExit(Scene2D& scene) {
}

void TestScene::onReload(Scene2D& scene) {
    elapsedTime = 0.0f;
    pulseCycle = 0;
}

void TestScene::update(Scene2D& scene, float deltaTime) {
    if (!initialized) {
        return;
    }

    elapsedTime += deltaTime;

    if (elapsedTime >= 1.0f) {
        elapsedTime = 0.0f;
        pulseCycle++;
    }

    auto* player = scene.findFirstObjectOfType<PlayerObject>();
    if (!player || !player->sprite.has_value()) {
        return;
    }

    float pulse = 0.85f + 0.15f * std::sin(static_cast<float>(pulseCycle) * 0.7f);
    player->sprite->color.a = pulse;
}