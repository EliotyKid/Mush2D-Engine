#pragma once

#include "iki_demo/objects/CheckpointObject.hpp"
#include "iki_demo/objects/PlayerObject.hpp"
#include "iki_demo/objects/SlimeEnemy.hpp"
#include "iki_demo/objects/TeleportObject.hpp"
#include "iki_demo/objects/WallObject.hpp"

#include "engine/world/Scene2D.hpp"

namespace Spawn {

PlayerObject& player(
    Scene2D& scene,
    const glm::vec2& position,
    const PlayerObject::Config& config = {}
);

// The class defaults; the caller tunes the attributes (moveSpeed, gravity, ...) on the result.
SlimeEnemy& slime(Scene2D& scene, const glm::vec2& position);

WallObject& wall(
    Scene2D& scene,
    const glm::vec2& position,
    const glm::vec2& scale,
    const WallObject::Config& config = {}
);

CheckpointObject& checkpoint(Scene2D& scene, const glm::vec2& position);

TeleportObject& teleport(
    Scene2D& scene,
    const glm::vec2& position,
    const glm::vec2& targetPosition,
    const TeleportObject::Config& config = {}
);

}