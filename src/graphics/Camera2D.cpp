#include "Camera2D.hpp"

#include "../world/Scene2D.hpp"

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

    glm::vec2 desiredPosition = target->transform.position + followOffset;

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

    float aspect =
        static_cast<float>(viewportExtent.width) /
        static_cast<float>(viewportExtent.height);

    float halfHeight = size * 0.5f;
    float halfWidth = halfHeight * aspect;

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

glm::mat4 Camera2D::buildViewMatrix() const {
    glm::mat4 view(1.0f);
    view = glm::translate(view, glm::vec3(-position, 0.0f));
    return view;
}

glm::mat4 Camera2D::buildProjectionMatrix(VkExtent2D viewportExtent) const {
    float aspect =
        static_cast<float>(viewportExtent.width) /
        static_cast<float>(viewportExtent.height);

    float halfHeight = size * 0.5f;
    float halfWidth = halfHeight * aspect;

    glm::mat4 proj = glm::ortho(
        -halfWidth, halfWidth,
        -halfHeight, halfHeight,
        -1.0f, 1.0f
    );

    proj[1][1] *= -1.0f;
    return proj;
}