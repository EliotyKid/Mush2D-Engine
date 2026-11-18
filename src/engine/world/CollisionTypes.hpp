#pragma once

#include <glm/glm.hpp>

namespace Collision2D {

struct AABB {
    glm::vec2 min{0.0f, 0.0f};
    glm::vec2 max{0.0f, 0.0f};
};

enum class QueryKind { Any, Solid, Trigger };

}
