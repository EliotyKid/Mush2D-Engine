#pragma once

#include "engine/world/GameObject.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <vector>

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

    // Smooth pixel art: free zoom (like HD) but clamped to 1x-8x pixel scale; Engine sets it from the render mode.
    bool pixelSmooth = false;

    // Sharp pixel art: half height = viewportHeight / (2 * computePixelScale(w) * pixelsPerUnit); `size` is ignored.
    bool pixelArt = true;
    float pixelsPerUnit = 32.0f;

    bool followEnabled = true;
    GameObjectId targetId = InvalidGameObjectId;
    glm::vec2 followOffset{0.0f, 0.0f};

    float followSmoothness = 6.0f;
    float snapDistance = 0.001f;

    bool boundsEnabled = true;
    WorldBounds bounds{};

    // Limit of the summed shake amplitude, world units (Camera2D is not reflected: a plain field, not F1).
    float maxShake = 0.5f;

public:
    // A random offset of `amplitude` world units that decays linearly to 0 over `duration` seconds. Shakes
    // add up to maxShake. The offset is added to `position` only when the renderer builds the view.
    void addShake(float amplitude, float duration);
    // Advances the shakes by real time (the Engine calls it every frame: a hit stop does not freeze them).
    void updateShake(float deltaTime);
    glm::vec2 shakeOffset() const { return shakeOffset_; }

    void updateFollow(const Scene2D& scene, float deltaTime);
    void clampToBounds(VkExtent2D viewportExtent);

    // Sets `size` so the view equals the sharp integer scale (switching modes does not jump).
    void fitPixelScale(VkExtent2D viewportExtent);
    // Zoom limits: 1x-8x pixel scale in smooth mode, 0.2-20 world units otherwise.
    void clampSize(VkExtent2D viewportExtent);

    // Half of the visible world area (x, y), in world units.
    glm::vec2 halfExtent(VkExtent2D viewportExtent) const;
    // Orthographic, Y up, no camera translation (the sprite shader subtracts position * parallax).
    glm::mat4 buildProjectionMatrix(VkExtent2D viewportExtent) const;

private:
    struct Shake {
        float amplitude, duration, left;
    };
    std::vector<Shake> shakes_;
    glm::vec2 shakeOffset_{0.0f};
    unsigned shakeSeed_ = 1;
};
