#pragma once

#include "CollisionWorld.hpp"
#include "EventBus.hpp"
#include "GameObject.hpp"
#include "SpatialGrid.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

// Emitted for every object freed at a flush; delivered when that flush dispatches events.
struct ObjectDestroyed {
    GameObjectId id;
};

// Owns GameObjects in a slot map. Create/destroy are deferred: they take effect at flush points
// (start and end of step, end of lateStep, or an explicit flush()), so they are safe inside callbacks.
class Scene2D {
public:
    std::string spawnRoomIid; // stamped as roomIid on every object created while set (LevelLoader::loadRoom)
    // Static level collision (segments per room; LevelLoader adds and removes rooms). Emptied by clear().
    CollisionWorld collision;

public:
    // The object resolves with find() at once and gets onCreate now; it joins the update list at the next flush.
    template<typename TObject, typename... Args>
    TObject& createObject(Args&&... args) {
        static_assert(std::is_base_of_v<GameObject, TObject>,
                      "TObject precisa herdar de GameObject.");

        return static_cast<TObject&>(addObject(std::make_unique<TObject>(std::forward<Args>(args)...)));
    }

    // Same, for an object created by name (TypeInfo::createObject, prefabs).
    GameObject& addObject(std::unique_ptr<GameObject> object) {
        GameObject& ref = *object;
        ref.roomIid = spawnRoomIid;
        insert(std::move(object));
        ref.onCreate(*this);
        return ref;
    }

    // Marks for destruction; onDestroy runs and the slot is freed at the next flush. Repeated calls do nothing.
    void destroy(GameObjectId id);

    // Applies pending destroys and rebuilds the update list. No-op while the scene is iterating.
    void flush();

    // Destroys everything (onDestroy is called). Generations are kept, so old handles stay invalid.
    void clear();

    // Engine-level lookup: resolves any handle that is still valid, including an object pending destroy.
    GameObject* find(GameObjectId id);
    const GameObject* find(GameObjectId id) const;

    // Game-facing lookup, like the other find* helpers: null for stale handles and objects pending destroy.
    GameObject* findObject(GameObjectId id) { return findLive(*this, id); }
    const GameObject* findObject(GameObjectId id) const { return findLive(*this, id); }

    template<typename TObject>
    TObject* findObjectAs(GameObjectId id) {
        GameObject* object = find(id);
        return object && !object->pendingDestroy_ ? dynamic_cast<TObject*>(object) : nullptr;
    }

    template<typename TObject>
    const TObject* findObjectAs(GameObjectId id) const {
        const GameObject* object = find(id);
        return object && !object->pendingDestroy_ ? dynamic_cast<const TObject*>(object) : nullptr;
    }

    template<typename TObject>
    TObject* findFirstObjectOfType() {
        for (auto& slot : slots_) {
            if (auto* casted = liveOf<TObject>(slot.object.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename TObject>
    const TObject* findFirstObjectOfType() const {
        for (const auto& slot : slots_) {
            if (auto* casted = liveOf<const TObject>(slot.object.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename TObject>
    std::vector<TObject*> findAllObjectsOfType() {
        std::vector<TObject*> result;
        for (auto& slot : slots_) {
            if (auto* casted = liveOf<TObject>(slot.object.get())) {
                result.push_back(casted);
            }
        }
        return result;
    }

    std::vector<GameObject*> findObjectsByTag(const std::string& tag);
    std::vector<const GameObject*> findObjectsByTag(const std::string& tag) const;

    // Attaches child under parent (InvalidGameObjectId detaches). keepWorld keeps the world transform by
    // rewriting the local one. Returns false on a cycle, a stale handle or a pending-destroy object; nothing changes then.
    bool setParent(GameObjectId child, GameObjectId parent, bool keepWorld = true);

    Transform2D worldTransform(GameObjectId id) const;

    // Recomputes the cached world transform of every child, parents first. Runs at the end of each flush.
    void updateTransforms();

    // While a hit stop runs, step only counts it down by the real deltaTime: objects, components (physics,
    // particles, effects, flash) and the frame's lateStep do not advance. Requests keep the longest, they do not add up.
    void hitstop(float seconds) { if (seconds > hitstop_) hitstop_ = seconds; }
    float timeScale() const { return hitstop_ > 0.0f ? 0.0f : 1.0f; }
    // The last step was frozen (it ran nothing): input read for it must wait for the next step.
    bool lastStepFrozen() const { return frozenStep_; }
    // How many times step() has run (frozen steps too). Tells whether any step ran since you last looked.
    std::uint64_t stepCount() const { return stepCount_; }

    void step(float deltaTime);
    void lateStep(float deltaTime);

    // Visits live objects (flushed, not pending destroy) in update order. Safe to create/destroy inside.
    template<typename F>
    void forEach(F&& fn) {
        IterGuard guard(iterating_);
        for (std::size_t i = 0; i < live_.size(); ++i) {
            if (!live_[i]->pendingDestroy_) fn(*live_[i]);
        }
    }

    template<typename F>
    void forEach(F&& fn) const {
        IterGuard guard(iterating_);
        for (std::size_t i = 0; i < live_.size(); ++i) {
            if (!live_[i]->pendingDestroy_) fn(static_cast<const GameObject&>(*live_[i]));
        }
    }

    // Broadphase over objects with an enabled collider. Inserted at flush, removed on destroy, and every live
    // object's box is refreshed at the start of each step (catches moves, size and enable/trigger changes).
    const SpatialGrid& grid() const { return grid_; }

    // Deferred events; dispatched at the end of every flush.
    EventBus& events() { return events_; }

    // Objects holding a slot, including created-but-unflushed and pending-destroy ones.
    std::size_t aliveCount() const { return aliveCount_; }

    // Shared systems objects reach by type (the game registers WorldState, World...). Not owned; nullptr
    // unregisters. Kept by clear(). service<T>() is nullptr when T was never registered.
    template<class T>
    void setService(T* service) {
        if (service) services_[std::type_index(typeid(T))] = service;
        else services_.erase(std::type_index(typeid(T)));
    }

    template<class T>
    T* service() const {
        auto it = services_.find(std::type_index(typeid(T)));
        return it == services_.end() ? nullptr : static_cast<T*>(it->second);
    }

private:
    friend class GameObject; // attach/markRemoved reach scene state
    struct Slot {
        std::unique_ptr<GameObject> object;
        std::uint32_t generation = 1;
    };

    void insert(std::unique_ptr<GameObject> object);
    // Keeps iterating_ balanced even if a callback throws.
    struct IterGuard {
        int& n;
        explicit IterGuard(int& counter) : n(counter) { ++n; }
        ~IterGuard() { --n; }
        IterGuard(const IterGuard&) = delete;
        IterGuard& operator=(const IterGuard&) = delete;
    };

    template<typename TScene>
    // Explicit return type: GCC rejects deducing `auto` here when findObject uses it before this point.
    static auto findLive(TScene& scene, GameObjectId id) -> decltype(scene.find(id)) {
        auto* object = scene.find(id);
        return object && !object->pendingDestroy_ ? object : nullptr;
    }

    // Cast for the find* scans: skips empty slots and objects pending destroy.
    template<typename TObject, typename TBase>
    static TObject* liveOf(TBase* object) {
        return object && !object->pendingDestroy_ ? dynamic_cast<TObject*>(object) : nullptr;
    }

    void markDestroy(GameObject& object);
    void computeWorld(GameObject& object);
    void processDestroys();
    void processComponentRemovals();
    void dispatchEvents();
    void syncGrid();

    SpatialGrid grid_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> freeSlots_;
    std::vector<GameObject*> live_; // update order, rebuilt at flush
    std::vector<GameObjectId> destroyQueue_;
    std::uint64_t nextSeq_ = 0;
    std::size_t aliveCount_ = 0;
    bool liveDirty_ = false;
    bool componentsDirty_ = false; // some component awaits removal
    bool gridFresh_ = false;       // the last flush synced the grid, so step() can skip its own sync
    float hitstop_ = 0.0f;         // real seconds of hit stop left
    std::uint64_t stepCount_ = 0;  // step() calls so far
    bool frozenStep_ = false;      // the last step was frozen, so lateStep skips too
    EventBus events_;
    std::unordered_map<std::type_index, void*> services_;
    mutable int iterating_ = 0;
};
