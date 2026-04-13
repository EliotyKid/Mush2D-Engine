#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct Transform2D {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
};

using GameObjectId = std::uint32_t;
constexpr GameObjectId InvalidGameObjectId = 0;

struct GameObject {
    GameObjectId id = InvalidGameObjectId;
    std::string name;
    bool active = true;
    Transform2D transform{};
};