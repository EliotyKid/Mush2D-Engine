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

void Camera2D::addShake(float amplitude, float duration) {
    if (amplitude > 0.0f && duration > 0.0f) shakes_.push_back({amplitude, duration, duration});
}

void Camera2D::updateShake(float deltaTime) {
    float amplitude = 0.0f;
    for (Shake& s : shakes_) {
        s.left -= deltaTime;
        if (s.left > 0.0f) amplitude += s.amplitude * s.left / s.duration;
    }
    std::erase_if(shakes_, [](const Shake& s) { return s.left <= 0.0f; });
    shakeSeed_ = shakeSeed_ * 1664525u + 1013904223u; // a cheap LCG: a new direction every frame
    const float angle = float(shakeSeed_ >> 8) / float(1u << 24) * 6.2831853f;
    shakeOffset_ = std::min(amplitude, maxShake) * glm::vec2(std::cos(angle), std::sin(angle));
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

void Camera2D::fitPixelScale(VkExtent2D e) {
    size = static_cast<float>(e.height) / (static_cast<float>(computePixelScale(static_cast<int>(e.width))) * pixelsPerUnit);
}

void Camera2D::clampSize(VkExtent2D e) {
    if (!pixelSmooth) { size = std::clamp(size, 0.2f, 20.0f); return; }
    float h = static_cast<float>(e.height);
    size = std::clamp(size, h / (8.0f * pixelsPerUnit), h / pixelsPerUnit);
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