#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

enum class TriggerType {
    Checkpoint,
    Teleport
};

struct SpriteData {
    std::uint32_t atlasTextureIndex = 0;
    std::string frameName;
    int layer = 0;
    int orderInLayer = 0;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct ColliderData {
    glm::vec2 size{1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f};
    bool isTrigger = false;
    bool enabled = true;
};

struct HealthData {
    int current = 1;
    int max = 1;
};

struct TriggerData {
    TriggerType type = TriggerType::Checkpoint;
    glm::vec2 targetPosition{0.0f, 0.0f};
    bool oneShot = false;
    bool consumed = false;
};