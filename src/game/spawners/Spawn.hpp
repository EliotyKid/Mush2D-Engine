#pragma once

#include "game/objects/CheckpointObject.hpp"
#include "game/objects/PlayerObject.hpp"
#include "game/objects/SlimeEnemy.hpp"
#include "game/objects/TeleportObject.hpp"
#include "game/objects/WallObject.hpp"

#include "engine/world/Scene2D.hpp"

namespace Spawn {

PlayerObject& player(
    Scene2D& scene,
    const glm::vec2& position,
    const PlayerObject::Config& config = {}
);

SlimeEnemy& slime(
    Scene2D& scene,
    const glm::vec2& position,
    const SlimeEnemy::Config& config = {}
);

WallObject& wall(
    Scene2D& scene,
    const glm::vec2& position,
    const glm::vec2& scale,
    const WallObject::Config& config = {}
);

CheckpointObject& checkpoint(
    Scene2D& scene,
    const glm::vec2& position,
    const CheckpointObject::Config& config = {}
);

TeleportObject& teleport(
    Scene2D& scene,
    const glm::vec2& position,
    const glm::vec2& targetPosition,
    const TeleportObject::Config& config = {}
);

}