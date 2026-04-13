#pragma once

#include "GameObject.hpp"

#include <glm/glm.hpp>

struct SpriteComponent {
    std::uint32_t textureIndex = 0;
    int layer = 0;
    int orderInLayer = 0;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    bool visible = true;
};

struct PlayerComponent {
    glm::vec2 velocity{0.0f, 0.0f};
    float moveSpeed = 2.5f;
};

struct ColliderComponent {
    glm::vec2 size{1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f};
    bool isTrigger = false;
    bool enabled = true;
};