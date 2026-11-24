#pragma once

#include "Component.hpp"
#include "ObjectData.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

class Scene2D;
class StateMachineBase;

struct Transform2D {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
};

// A point in a parent's local space to world space (same math Scene2D uses for child transforms).
// shortcut: approximate under negative or non-uniform parent scale combined with rotation; see Scene2D::computeWorld.
inline glm::vec2 localToWorld(const Transform2D& parentWorld, const glm::vec2& local) {
    float c = std::cos(parentWorld.rotation), s = std::sin(parentWorld.rotation);
    glm::vec2 scaled = parentWorld.scale * local;
    return parentWorld.position + glm::vec2{c * scaled.x - s * scaled.y, s * scaled.x + c * scaled.y};
}

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
    std::string roomIid;     // LDtk room that created it (empty = none); unloading that room destroys it
    bool persistent = false; // survives its room's unload (player, camera, carried items)
    std::string entityIid;   // LDtk entity iid (empty = not made from an LDtk entity)
    std::string prefab;      // prefab it was created from (empty = made in code)
    bool flipX = false;      // exported shortcut: mirrors the sprite when it is placed at its pivot

    Transform2D transform{};

    std::optional<SpriteData> sprite;
    std::optional<ColliderData> collider;
    std::optional<HealthData> health;
    std::optional<TriggerData> trigger;

    std::unordered_set<std::string> tags;

    // Attributes whose value came from an LDtk entity field (the inspector does not save them to the prefab).
    std::set<std::string> ldtkOverridden;

    // Free values from the prefab's "extra" block and LDtk fields that match no attribute.
    const nlohmann::json& extra() const { return extra_; }
    nlohmann::json& extra() { return extra_; }

public:
    virtual ~GameObject() = default;

    // Overridden by MUSH2D_OBJECT; use typeOf(object). The GameObject type exports name, transform,
    // visible, updateOrder, persistent, sprite and collider fields (registered in Reflect.cpp).
    virtual const TypeInfo& reflectType() const;

    virtual void onCreate(Scene2D& scene) {}
    // Objects made from a prefab: runs after onCreate, the prefab values and the LDtk fields were applied
    // (order in Reflect.hpp), so the object can react to its final values. Not called for objects made in code.
    virtual void onPrefabApplied(Scene2D& scene) {}
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

    // Attaches a component (onAttach runs now). The object must belong to a scene (call from onCreate or later).
    // On an object being destroyed: from onDetach the new component is detached too; from onDestroy it never
    // gets onDetach (components were already detached and cleared). Avoid it there.
    template<typename T, typename... Args>
    T& addComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T precisa herdar de Component.");
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *component;
        attach(std::move(component));
        return ref;
    }

    // Same, for a component created by name (TypeInfo::createComponent).
    Component& addComponent(std::unique_ptr<Component> component) {
        Component& ref = *component;
        attach(std::move(component));
        return ref;
    }

    // First component of type T, or nullptr. Still returns one marked by removeComponent until the next flush.
    template<typename T>
    T* getComponent() const {
        for (const auto& c : components_) {
            if (auto* casted = dynamic_cast<T*>(c.get())) return casted;
        }
        return nullptr;
    }

    // Deferred: stops stepping now, onDetach runs and the component is freed at the next flush.
    template<typename T>
    void removeComponent() {
        for (const auto& c : components_) {
            if (!c->removing_ && dynamic_cast<T*>(c.get())) {
                markRemoved(*c);
                return;
            }
        }
    }

    const std::vector<std::unique_ptr<Component>>& components() const { return components_; }

    // State machines the F1 inspector lists, read only ("move: fall (0.32 s)"). The machines must outlive
    // the object (usually its members). Every change of a registered machine emits StateChanged (Effects.hpp);
    // a machine started before it was registered emits its current state now.
    void registerStateMachine(std::string label, StateMachineBase* machine); // Scene2D.cpp
    const std::vector<std::pair<std::string, StateMachineBase*>>& stateMachines() const { return stateMachines_; }

    bool pendingDestroy() const { return pendingDestroy_; }

    GameObjectId parent() const { return parent_; }
    const std::vector<GameObjectId>& children() const { return children_; }

    // Roots: the live local transform. Children: cached, refreshed by Scene2D::updateTransforms() at every
    // flush, so a child moved by game code shows its new world transform after the next flush. For children
    // the value is the one cached at the last flush; call scene.updateTransforms() for a fresh one mid-step.
    Transform2D world() const { return parent_ ? world_ : transform; }

private:
    friend class Scene2D;
    void attach(std::unique_ptr<Component> component); // Scene2D.cpp: needs the full Scene2D
    void markRemoved(Component& component);

    Scene2D* scene_ = nullptr; // set when the scene takes the object
    std::vector<std::unique_ptr<Component>> components_;
    bool pendingDestroy_ = false;
    std::uint64_t creationSeq_ = 0; // tie-break for updateOrder
    GameObjectId parent_ = InvalidGameObjectId;
    std::vector<GameObjectId> children_;
    Transform2D world_{};
    nlohmann::json extra_ = nlohmann::json::object();
    std::vector<std::pair<std::string, StateMachineBase*>> stateMachines_;
};