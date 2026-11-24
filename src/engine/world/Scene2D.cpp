#include "Scene2D.hpp"

#include "Collision.hpp"
#include "Effects.hpp"
#include "StateMachine.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

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
    object->scene_ = this;
    object->creationSeq_ = nextSeq_++;
    slot.object = std::move(object);
    ++aliveCount_;
    liveDirty_ = true;
}

// Descendants are queued before the object, so children get onDestroy before their parent.
void Scene2D::markDestroy(GameObject& object) {
    if (object.pendingDestroy_) {
        return;
    }
    object.pendingDestroy_ = true; // set first: guards against re-entry through a corrupt hierarchy
    for (GameObjectId childId : object.children_) {
        if (GameObject* child = find(childId)) markDestroy(*child);
    }
    destroyQueue_.push_back(object.id);
}

void Scene2D::destroy(GameObjectId id) {
    if (GameObject* object = find(id)) markDestroy(*object);
}

bool Scene2D::setParent(GameObjectId childId, GameObjectId parentId, bool keepWorld) {
    GameObject* child = find(childId);
    GameObject* parent = parentId ? find(parentId) : nullptr;
    if (!child || child->pendingDestroy_ || (parentId && (!parent || parent->pendingDestroy_))) {
        return false;
    }
    for (const GameObject* p = parent; p; p = find(p->parent_)) { // reject cycles (and parent == child)
        if (p == child) {
            std::fprintf(stderr, "Scene2D: setParent rejected, '%s' is an ancestor of '%s' (cycle)\n",
                         child->name.c_str(), parent->name.c_str());
            return false;
        }
    }
    if (child->parent_ == parentId) {
        return true;
    }

    // Child and new parent need current world transforms: refresh just their two chains, not the scene.
    auto rootOf = [&](GameObject* o) {
        while (GameObject* up = find(o->parent_)) o = up;
        return o;
    };
    GameObject* childRoot = rootOf(child);
    computeWorld(*childRoot);
    if (parent) {
        GameObject* parentRoot = rootOf(parent);
        if (parentRoot != childRoot) computeWorld(*parentRoot);
    }
    Transform2D world = child->world();

    if (GameObject* old = find(child->parent_)) {
        std::erase(old->children_, childId);
    }
    child->parent_ = parentId;
    if (parent) parent->children_.push_back(childId);

    if (keepWorld) {
        Transform2D p = parent ? parent->world() : Transform2D{};
        glm::vec2 offset = world.position - p.position;
        float c = std::cos(-p.rotation), s = std::sin(-p.rotation);
        glm::vec2 unrotated{c * offset.x - s * offset.y, s * offset.x + c * offset.y};
        auto safeDiv = [](float a, float b) { return b != 0.0f ? a / b : a; };
        child->transform.position = {safeDiv(unrotated.x, p.scale.x), safeDiv(unrotated.y, p.scale.y)};
        child->transform.rotation = world.rotation - p.rotation;
        child->transform.scale = {safeDiv(world.scale.x, p.scale.x), safeDiv(world.scale.y, p.scale.y)};
    }
    computeWorld(*child);
    return true;
}

// Children only; roots read their live transform directly. Parents are always computed before children.
void Scene2D::computeWorld(GameObject& object) {
    const GameObject* parent = find(object.parent_);
    if (parent) {
        Transform2D p = parent->world();
        // shortcut: rotation and scale composition is approximate under negative or non-uniform parent scale
        // combined with child rotation; upgrade to a 2x3 matrix if that ever matters.
        object.world_.position = localToWorld(p, object.transform.position);
        object.world_.rotation = p.rotation + object.transform.rotation;
        object.world_.scale = p.scale * object.transform.scale;
    }
    for (GameObjectId childId : object.children_) {
        if (GameObject* child = find(childId)) computeWorld(*child);
    }
}

void Scene2D::updateTransforms() {
    for (Slot& slot : slots_) {
        if (slot.object && !slot.object->parent_ && !slot.object->children_.empty()) {
            computeWorld(*slot.object);
        }
    }
}

Transform2D Scene2D::worldTransform(GameObjectId id) const {
    const GameObject* object = find(id);
    if (!object) return Transform2D{};
    return object->parent_ ? object->world_ : object->transform;
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
    IterGuard guard(iterating_); // a flush() from inside onDestroy must not re-enter
    while (!destroyQueue_.empty()) {
        std::vector<GameObjectId> batch;
        batch.swap(destroyQueue_);
        // A queued id can be stale (its slot was freed or reused): resolve each one and drop the dead ones.
        std::erase_if(batch, [&](GameObjectId id) { return find(id) == nullptr; });
        // Whole batch gets onDestroy before any is freed, so they can still see each other.
        for (GameObjectId id : batch) {
            // Order: components detach first, then the object's onDestroy, so onDestroy never sees live components.
            // Both may create/destroy; the queue grows and slots_ may realloc, so re-index every time.
            GameObject& object = *slots_[id.index].object;
            for (std::size_t i = 0; i < object.components_.size(); ++i) {
                object.components_[i]->onDetach(object, *this); // index loop: onDetach may add components
            }
            object.components_.clear();
            slots_[id.index].object->onDestroy(*this);
            grid_.remove(id);
            events_.emit(ObjectDestroyed{id});
        }
        for (GameObjectId id : batch) { // unlink from surviving parents while every object is still alive
            const GameObject* o = slots_[id.index].object.get();
            if (GameObject* parent = find(o->parent_)) std::erase(parent->children_, id);
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
}

void GameObject::attach(std::unique_ptr<Component> component) {
    assert(scene_ && "addComponent before the scene owns the object (use onCreate or later)");
    Component& ref = *component;
    components_.push_back(std::move(component));
    ref.onAttach(*this, *scene_);
}

void GameObject::registerStateMachine(std::string label, StateMachineBase* machine) {
    machine->registeredChange = [this, label](const char* from, const char* to) {
        if (scene_) scene_->events().emit(StateChanged{id, label, from, to});
    };
    if (*machine->current()) machine->registeredChange("", machine->current()); // started before registration
    stateMachines_.emplace_back(std::move(label), machine);
}

void GameObject::markRemoved(Component& component) {
    component.removing_ = true;
    if (scene_) scene_->componentsDirty_ = true;
}

void Scene2D::processComponentRemovals() {
    if (!componentsDirty_) return;
    componentsDirty_ = false;
    IterGuard guard(iterating_);
    for (std::size_t i = 0; i < slots_.size(); ++i) { // onDetach may create objects (slots_ may grow): index loop
        if (!slots_[i].object || slots_[i].object->pendingDestroy_) continue;
        GameObject& object = *slots_[i].object;
        std::vector<std::unique_ptr<Component>> removed;
        for (auto& c : object.components_) {
            if (c->removing_) removed.push_back(std::move(c));
        }
        std::erase_if(object.components_, [](const auto& c) { return !c; });
        for (auto& c : removed) c->onDetach(object, *this);
    }
}

void Scene2D::flush() {
    if (iterating_ > 0) {
        return;
    }
    processDestroys();
    processComponentRemovals();

    auto before = [](const GameObject* a, const GameObject* b) {
        return a->updateOrder != b->updateOrder ? a->updateOrder < b->updateOrder : a->creationSeq_ < b->creationSeq_;
    };
    const bool rebuilt = liveDirty_;
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
    updateTransforms();
    if (rebuilt) syncGrid(); // objects that joined live_ enter the grid with their world box
    gridFresh_ = rebuilt;
    dispatchEvents();
}

void Scene2D::syncGrid() {
    for (const GameObject* o : live_) {
        if (o->pendingDestroy_) continue; // removed from the grid when freed
        if (Collision2D::hasCollider(*o)) {
            grid_.update(o->id, Collision2D::buildAABB(*o), o->collider->isTrigger);
        } else {
            grid_.remove(o->id);
        }
    }
}

void Scene2D::dispatchEvents() {
    IterGuard guard(iterating_); // a handler calling flush() must not re-enter dispatch
    events_.dispatch();
}

void Scene2D::clear() {
    assert(iterating_ == 0 && "clear() inside a step/forEach callback");
    for (std::uint32_t i = 0; i < slots_.size(); ++i) {
        destroy(GameObjectId{i, slots_[i].generation});
    }
    processDestroys();
    dispatchEvents(); // deliver ObjectDestroyed now so it does not leak into the next level

    // shortcut: objects created by onDestroy during clear() are dropped without onDestroy (old behaviour);
    // loop the destroy pass instead if a game ever relies on them.
    live_.clear();
    grid_.clear();
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
    componentsDirty_ = false;
    destroyQueue_.clear(); // handlers may have queued destroys for objects just dropped; they must not hit reused slots
    collision = CollisionWorld{};
}

std::vector<GameObject*> Scene2D::findObjectsByTag(const std::string& tag) {
    std::vector<GameObject*> result;
    for (auto& slot : slots_) {
        if (slot.object && !slot.object->pendingDestroy_ && slot.object->hasTag(tag)) result.push_back(slot.object.get());
    }
    return result;
}

std::vector<const GameObject*> Scene2D::findObjectsByTag(const std::string& tag) const {
    std::vector<const GameObject*> result;
    for (const auto& slot : slots_) {
        if (slot.object && !slot.object->pendingDestroy_ && slot.object->hasTag(tag)) result.push_back(slot.object.get());
    }
    return result;
}

void Scene2D::step(float deltaTime) {
    flush();
    if (!gridFresh_) syncGrid(); // objects moved or edited since the last step; within a step, boxes may lag one frame
    gridFresh_ = false;
    {
        IterGuard guard(iterating_);
        for (std::size_t i = 0; i < live_.size(); ++i) {
            GameObject* object = live_[i];
            if (object->active && !object->pendingDestroy_) {
                object->onStep(*this, deltaTime);
                // Index loop: a component may add components. A destroyed owner stops its components too.
                for (std::size_t c = 0; c < object->components_.size() && !object->pendingDestroy_; ++c) {
                    Component& comp = *object->components_[c];
                    if (comp.enabled && !comp.removing_) comp.onStep(*object, *this, deltaTime);
                }
            }
        }
    }
    flush();
}

void Scene2D::lateStep(float deltaTime) {
    // No flush first: objects spawned during step were already flushed by the after-step flush, so they do get onLateStep this frame.
    {
        IterGuard guard(iterating_);
        for (std::size_t i = 0; i < live_.size(); ++i) {
            GameObject* object = live_[i];
            if (object->active && !object->pendingDestroy_) object->onLateStep(*this, deltaTime);
        }
    }
    flush();
}
