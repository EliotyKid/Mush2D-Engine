#include "Spawn.hpp"

namespace Spawn {

PlayerObject& player(
    Scene2D& scene,
    const glm::vec2& position,
    const PlayerObject::Config& config
) {
    auto& object = scene.createObject<PlayerObject>(config);
    object.transform.position = position;
    return object;
}

SlimeEnemy& slime(
    Scene2D& scene,
    const glm::vec2& position,
    const SlimeEnemy::Config& config
) {
    auto& object = scene.createObject<SlimeEnemy>(config);
    object.transform.position = position;
    return object;
}

WallObject& wall(
    Scene2D& scene,
    const glm::vec2& position,
    const glm::vec2& scale,
    const WallObject::Config& config
) {
    auto& object = scene.createObject<WallObject>(config);
    object.transform.position = position;
    object.transform.scale = scale;
    return object;
}

CheckpointObject& checkpoint(
    Scene2D& scene,
    const glm::vec2& position,
    const CheckpointObject::Config& config
) {
    auto& object = scene.createObject<CheckpointObject>(config);
    object.transform.position = position;
    object.transform.scale = {1.0f, 1.0f};
    return object;
}

TeleportObject& teleport(
    Scene2D& scene,
    const glm::vec2& position,
    const glm::vec2& targetPosition,
    const TeleportObject::Config& config
) {
    auto finalConfig = config;
    finalConfig.targetPosition = targetPosition;

    auto& object = scene.createObject<TeleportObject>(finalConfig);
    object.transform.position = position;
    object.transform.scale = {1.0f, 1.0f};
    return object;
}

}