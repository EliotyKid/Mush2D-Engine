#include "Camera2D.hpp"

#include "engine/render/PixelScale.hpp"
#include "engine/world/Scene2D.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

void Camera2D::updateFollow(const Scene2D& scene, float deltaTime) {
    if (!followEnabled) {
        return;
    }

    const GameObject* target = scene.findObject(targetId);
    if (!target) {
        return;
    }

    glm::vec2 desiredPosition = target->world().position + followOffset;

    float t = 1.0f - std::exp(-followSmoothness * deltaTime);
    position = glm::mix(position, desiredPosition, t);

    if (glm::length(desiredPosition - position) < snapDistance) {
        position = desiredPosition;
    }
}

void Camera2D::clampToBounds(VkExtent2D viewportExtent) {
    if (!boundsEnabled || viewportExtent.height == 0) {
        return;
    }

    glm::vec2 half = halfExtent(viewportExtent);
    float halfWidth = half.x;
    float halfHeight = half.y;

    float minX = bounds.left + halfWidth;
    float maxX = bounds.right - halfWidth;
    float minY = bounds.bottom + halfHeight;
    float maxY = bounds.top - halfHeight;

    if (minX > maxX) {
        position.x = (bounds.left + bounds.right) * 0.5f;
    } else {
        position.x = std::clamp(position.x, minX, maxX);
    }

    if (minY > maxY) {
        position.y = (bounds.bottom + bounds.top) * 0.5f;
    } else {
        position.y = std::clamp(position.y, minY, maxY);
    }
}

glm::vec2 Camera2D::halfExtent(VkExtent2D viewportExtent) const {
    float w = static_cast<float>(viewportExtent.width);
    float h = static_cast<float>(viewportExtent.height);
    float halfHeight = pixelArt
        ? h / (2.0f * static_cast<float>(computePixelScale(static_cast<int>(viewportExtent.width))) * pixelsPerUnit)
        : size * 0.5f;
    return {halfHeight * w / h, halfHeight};
}

glm::mat4 Camera2D::buildProjectionMatrix(VkExtent2D viewportExtent) const {
    glm::vec2 half = halfExtent(viewportExtent);

    glm::mat4 proj = glm::ortho(
        -half.x, half.x,
        -half.y, half.y,
        -1.0f, 1.0f
    );

    proj[1][1] *= -1.0f;
    return proj;
}