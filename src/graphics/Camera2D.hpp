#pragma once

#include "../world/GameObject.hpp"

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
    float size = 2.0f;

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

    glm::mat4 buildViewMatrix() const;
    glm::mat4 buildProjectionMatrix(VkExtent2D viewportExtent) const;
};