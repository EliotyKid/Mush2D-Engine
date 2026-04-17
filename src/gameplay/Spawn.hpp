#pragma once

#include "CheckpointObject.hpp"
#include "PlayerObject.hpp"
#include "SlimeEnemy.hpp"
#include "TeleportObject.hpp"
#include "WallObject.hpp"

#include "../world/Scene2D.hpp"

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