#pragma once

#include "engine/reflect/Reflect.hpp"
#include "engine/world/GameObjectId.hpp"

#include <glm/glm.hpp>

#include <string>

// One hit. direction is where it pushes the target (any length, zero = no push): from the attacker to the
// target, or a projectile's velocity. source = who the hit counts for (the shooter of a projectile).
struct DamageInfo {
    int amount = 1;
    glm::vec2 direction{0.0f};
    std::string element; // an element name from project.json; empty = no element
    float knockback = 0;
    GameObjectId source{};
    bool parryable = true;  // false: a parry is only a block
    bool projectile = false;
};
