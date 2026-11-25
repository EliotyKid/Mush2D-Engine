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
    glm::vec4 tint{1.0f};         // multiplies color when drawn: a tint that survives game code rewriting color
    glm::vec4 flash{0.0f};        // rgb + amount 0..1: the drawn sprite mixes toward rgb, unlit (the Flash component owns it)
    float depth = 0.0f;           // 0 = gameplay layer, > 0 further back (fog), < 0 foreground
    glm::vec2 parallax{1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f}; // sprite centre relative to transform.position, times the world scale (pivot placement)
    glm::vec2 drawSize{0.0f, 0.0f}; // world units, multiplied by the world scale (x < 0 mirrors); {0, 0} = the scale alone
    bool additive = false;        // drawn in the additive pass (glows, halos)
    std::string atlasName;        // exported `spriteAtlas` (AssetRef); prefab spawn resolves it to atlasTextureIndex
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