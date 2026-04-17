#pragma once

#include "ObjectData.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

#include <glm/glm.hpp>

class Scene2D;

struct Transform2D {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
};

using GameObjectId = std::uint32_t;
constexpr GameObjectId InvalidGameObjectId = 0;

class GameObject {
public:
    GameObjectId id = InvalidGameObjectId;
    std::string name;
    bool active = true;
    bool visible = true;

    Transform2D transform{};

    std::optional<SpriteData> sprite;
    std::optional<ColliderData> collider;
    std::optional<HealthData> health;
    std::optional<TriggerData> trigger;

    std::unordered_set<std::string> tags;

public:
    virtual ~GameObject() = default;

    virtual void onCreate(Scene2D& scene) {}
    virtual void onStep(Scene2D& scene, float deltaTime) {}
    virtual void onLateStep(Scene2D& scene, float deltaTime) {}
    virtual void onTriggerEnter(Scene2D& scene, GameObject& other) {}
    virtual void onTriggerExit(Scene2D& scene, GameObject& other) {}
    virtual void onCollision(Scene2D& scene, GameObject& other) {}
    virtual void onDestroy(Scene2D& scene) {}

    void addTag(const std::string& tag) {
        tags.insert(tag);
    }

    void removeTag(const std::string& tag) {
        tags.erase(tag);
    }

    bool hasTag(const std::string& tag) const {
        return tags.find(tag) != tags.end();
    }
};