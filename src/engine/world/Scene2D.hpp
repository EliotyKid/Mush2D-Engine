#pragma once

#include "GameObject.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Owns GameObjects in a slot map. Create/destroy are deferred: they take effect at flush points
// (start and end of step, end of lateStep, or an explicit flush()), so they are safe inside callbacks.
class Scene2D {
public:
    void* userData = nullptr; // opaque, owned by the game

public:
    // The object resolves with find() at once and gets onCreate now; it joins the update list at the next flush.
    template<typename TObject, typename... Args>
    TObject& createObject(Args&&... args) {
        static_assert(std::is_base_of_v<GameObject, TObject>,
                      "TObject precisa herdar de GameObject.");

        auto object = std::make_unique<TObject>(std::forward<Args>(args)...);
        TObject& ref = *object;
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

    GameObject* find(GameObjectId id);
    const GameObject* find(GameObjectId id) const;

    GameObject* findObject(GameObjectId id) { return find(id); }
    const GameObject* findObject(GameObjectId id) const { return find(id); }

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

    const Transform2D& worldTransform(GameObjectId id) const;

    // Recomputes the cached world transform of every child, parents first. Runs at the end of each flush.
    void updateTransforms();

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

    // Objects holding a slot, including created-but-unflushed and pending-destroy ones.
    std::size_t aliveCount() const { return aliveCount_; }

private:
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

    // Cast for the find* scans: skips empty slots and objects pending destroy.
    template<typename TObject, typename TBase>
    static TObject* liveOf(TBase* object) {
        return object && !object->pendingDestroy_ ? dynamic_cast<TObject*>(object) : nullptr;
    }

    void markDestroy(GameObject& object);
    void computeWorld(GameObject& object);
    void processDestroys();

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> freeSlots_;
    std::vector<GameObject*> live_; // update order, rebuilt at flush
    std::vector<GameObjectId> destroyQueue_;
    std::uint64_t nextSeq_ = 0;
    std::size_t aliveCount_ = 0;
    bool liveDirty_ = false;
    mutable int iterating_ = 0;
};
