#include "Scene2D.hpp"

#include <algorithm>

void Scene2D::insert(std::unique_ptr<GameObject> object) {
    std::uint32_t index;
    if (!freeSlots_.empty()) {
        index = freeSlots_.back();
        freeSlots_.pop_back();
    } else {
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
    }
    Slot& slot = slots_[index];
    object->id = GameObjectId{index, slot.generation};
    object->creationSeq_ = nextSeq_++;
    slot.object = std::move(object);
    ++aliveCount_;
    liveDirty_ = true;
}

void Scene2D::destroy(GameObjectId id) {
    GameObject* object = find(id);
    if (!object || object->pendingDestroy_) {
        return;
    }
    object->pendingDestroy_ = true;
    destroyQueue_.push_back(id);
}

GameObject* Scene2D::find(GameObjectId id) {
    return const_cast<GameObject*>(static_cast<const Scene2D&>(*this).find(id));
}

const GameObject* Scene2D::find(GameObjectId id) const {
    if (!id || id.index >= slots_.size()) {
        return nullptr;
    }
    const Slot& slot = slots_[id.index];
    return slot.generation == id.generation ? slot.object.get() : nullptr;
}

void Scene2D::processDestroys() {
    ++iterating_; // a flush() from inside onDestroy must not re-enter
    while (!destroyQueue_.empty()) {
        std::vector<GameObjectId> batch;
        batch.swap(destroyQueue_);
        // Whole batch gets onDestroy before any is freed, so they can still see each other.
        for (GameObjectId id : batch) {
            slots_[id.index].object->onDestroy(*this); // onDestroy may create/destroy; queue grows, slots_ may realloc
        }
        std::erase_if(live_, [](const GameObject* o) { return o->pendingDestroy_; });
        for (GameObjectId id : batch) {
            Slot& slot = slots_[id.index];
            slot.object.reset();
            if (++slot.generation == 0) slot.generation = 1;
            freeSlots_.push_back(id.index);
            --aliveCount_;
        }
        liveDirty_ = true;
    }
    --iterating_;
}

void Scene2D::flush() {
    if (iterating_ > 0) {
        return;
    }
    processDestroys();

    auto before = [](const GameObject* a, const GameObject* b) {
        return a->updateOrder != b->updateOrder ? a->updateOrder < b->updateOrder : a->creationSeq_ < b->creationSeq_;
    };
    if (liveDirty_) {
        live_.clear();
        for (const Slot& slot : slots_) {
            if (slot.object) live_.push_back(slot.object.get());
        }
        liveDirty_ = false;
    }
    if (!std::is_sorted(live_.begin(), live_.end(), before)) {
        std::sort(live_.begin(), live_.end(), before);
    }
}

void Scene2D::clear() {
    for (std::uint32_t i = 0; i < slots_.size(); ++i) {
        destroy(GameObjectId{i, slots_[i].generation});
    }
    processDestroys();

    // shortcut: objects created by onDestroy during clear() are dropped without onDestroy (old behaviour);
    // loop the destroy pass instead if a game ever relies on them.
    live_.clear();
    freeSlots_.clear();
    for (std::uint32_t i = static_cast<std::uint32_t>(slots_.size()); i-- > 0;) {
        Slot& slot = slots_[i];
        if (slot.object) {
            slot.object.reset();
            if (++slot.generation == 0) slot.generation = 1;
        }
        freeSlots_.push_back(i); // descending, so reuse starts at slot 0
    }
    aliveCount_ = 0;
    liveDirty_ = false;
}

std::vector<GameObject*> Scene2D::findObjectsByTag(const std::string& tag) {
    std::vector<GameObject*> result;
    for (auto& slot : slots_) {
        if (slot.object && slot.object->hasTag(tag)) result.push_back(slot.object.get());
    }
    return result;
}

std::vector<const GameObject*> Scene2D::findObjectsByTag(const std::string& tag) const {
    std::vector<const GameObject*> result;
    for (const auto& slot : slots_) {
        if (slot.object && slot.object->hasTag(tag)) result.push_back(slot.object.get());
    }
    return result;
}

void Scene2D::step(float deltaTime) {
    flush();
    ++iterating_;
    for (std::size_t i = 0; i < live_.size(); ++i) {
        GameObject* object = live_[i];
        if (object->active && !object->pendingDestroy_) object->onStep(*this, deltaTime);
    }
    --iterating_;
    flush();
}

void Scene2D::lateStep(float deltaTime) {
    // No flush first (spec flush points): an object made between step and lateStep must not get onLateStep before onStep.
    ++iterating_;
    for (std::size_t i = 0; i < live_.size(); ++i) {
        GameObject* object = live_[i];
        if (object->active && !object->pendingDestroy_) object->onLateStep(*this, deltaTime);
    }
    --iterating_;
    flush();
}
