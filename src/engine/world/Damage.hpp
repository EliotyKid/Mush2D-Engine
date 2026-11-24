#pragma once

#include "engine/reflect/Reflect.hpp"

#include <glm/glm.hpp>

MUSH2D_ENUM(Element, None, Ice, Fire)

// One hit. direction is where it pushes the target (any length, zero = no push). D3 adds fields.
struct DamageInfo {
    int amount = 1;
    glm::vec2 direction{0.0f};
    Element element = Element::None;
};
