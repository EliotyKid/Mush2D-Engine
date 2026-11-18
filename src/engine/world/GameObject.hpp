#pragma once

#include "ObjectData.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

class Scene2D;

struct Transform2D {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
};

// Slot-map handle: a stale id (slot freed or reused) has a mismatched generation.
struct GameObjectId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0; // 0 = invalid

    explicit operator bool() const { return generation != 0; }
    bool operator==(const GameObjectId&) const = default;
};
inline constexpr GameObjectId InvalidGameObjectId{};

template<>
struct std::hash<GameObjectId> {
    std::size_t operator()(const GameObjectId& id) const noexcept {
        return std::hash<std::uint64_t>{}((std::uint64_t(id.generation) << 32) | id.index);
    }
};

class GameObject {
public:
    GameObjectId id = InvalidGameObjectId;
    std::string name;
    bool active = true; // false = paused, not destroyed
    bool visible = true;
    int updateOrder = 0; // lower runs first; changes apply at the next flush

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

    bool pendingDestroy() const { return pendingDestroy_; }

    GameObjectId parent() const { return parent_; }
    const std::vector<GameObjectId>& children() const { return children_; }

    // Roots: the live local transform. Children: cached, refreshed by Scene2D::updateTransforms() at every
    // flush, so a child moved by game code shows its new world transform after the next flush.
    Transform2D world() const { return parent_ ? world_ : transform; }

private:
    friend class Scene2D;
    bool pendingDestroy_ = false;
    std::uint64_t creationSeq_ = 0; // tie-break for updateOrder
    GameObjectId parent_ = InvalidGameObjectId;
    std::vector<GameObjectId> children_;
    Transform2D world_{};
};