#pragma once

#include "engine/world/GameObject.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class Scene2D;

class Camera2D {
public:
    struct WorldBounds {
        float left = -5.0f;
        float right = 5.0f;
        float bottom = -3.0f;
        float top = 3.0f;
    };

public:
    glm::vec2 position{0.0f, 0.0f};
    float size = 2.0f; // visible world height; HD mode only

    // Pixel art: half height = viewportHeight / (2 * computePixelScale(w) * pixelsPerUnit); `size` is ignored.
    bool pixelArt = true;
    float pixelsPerUnit = 32.0f;

    bool followEnabled = true;
    GameObjectId targetId = InvalidGameObjectId;
    glm::vec2 followOffset{0.0f, 0.0f};

    float followSmoothness = 6.0f;
    float snapDistance = 0.001f;

    bool boundsEnabled = true;
    WorldBounds bounds{};

public:
    void updateFollow(const Scene2D& scene, float deltaTime);
    void clampToBounds(VkExtent2D viewportExtent);

    // Half of the visible world area (x, y), in world units.
    glm::vec2 halfExtent(VkExtent2D viewportExtent) const;
    // Orthographic, Y up, no camera translation (the sprite shader subtracts position * parallax).
    glm::mat4 buildProjectionMatrix(VkExtent2D viewportExtent) const;
};
