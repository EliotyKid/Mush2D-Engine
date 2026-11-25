#include "engine/world/Projectile.hpp"

#include "engine/world/Combat.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/world/Scene2D.hpp"

#include <algorithm>
#include <iostream>
#include <set>

namespace {

float cross(glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; }

// Fraction of from->to where it first crosses a solid segment (one-way ones let it pass), or > 1 for none.
float wallHit(const Scene2D& scene, glm::vec2 from, glm::vec2 to, std::vector<const Segment*>& segments) {
    const glm::vec2 pad{1e-3f};
    scene.collision.query({glm::min(from, to) - pad, glm::max(from, to) + pad}, segments);
    const glm::vec2 r = to - from;
    float best = 2.0f;
    for (const Segment* s : segments) {
        if (s->oneWay) continue;
        const glm::vec2 e = s->b - s->a;
        const float denom = cross(r, e);
        if (std::abs(denom) < 1e-9f) continue; // parallel
        const float t = cross(s->a - from, e) / denom;
        const float u = cross(s->a - from, r) / denom;
        if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) best = std::min(best, t);
    }
    return best;
}

} // namespace

void Projectile::launch(GameObject& owner, glm::vec2 dir, const std::string& newTeam, GameObjectId newSource) {
    direction = glm::length(dir) > 1e-6f ? glm::normalize(dir) : glm::vec2(1.0f, 0.0f);
    team = newTeam;
    source = newSource;
    reflected = false;
    flying_ = true;
    hitPending_ = false;
    traveled_ = 0.0f;
    owner_ = &owner;
    owner.visible = true;
    owner.flipX = direction.x < 0.0f;
}

void Projectile::reflect(GameObjectId newSource, const std::string& newTeam) {
    if (reflected) return;
    reflected = true;
    direction = -direction;
    source = newSource;
    team = newTeam;
    hitPending_ = false; // the parry keeps it flying
    traveled_ = 0.0f;
    if (owner_) owner_->flipX = direction.x < 0.0f;
}

void Projectile::onStep(GameObject& owner, Scene2D& scene, float dt) {
    if (!flying_) return;
    if (hitPending_) return breakAt(owner, scene, owner.world().position);

    const float length = std::min(speed * dt, std::max(range - traveled_, 0.0f));
    const glm::vec2 from = owner.transform.position; // pooled projectiles are roots: local = world
    const glm::vec2 to = from + direction * length;
    if (const float t = wallHit(scene, from, to, segments_); t <= 1.0f) {
        owner.transform.position = from + (to - from) * t;
        return breakAt(owner, scene, owner.transform.position);
    }
    owner.transform.position = to;
    traveled_ += length;

    if (CombatWorld* world = scene.service<CombatWorld>()) {
        world->query(combatBox(owner, size, glm::vec2(0.0f)), found_);
        for (Hurtbox* h : found_) {
            if (h->team == team) continue;
            DamageInfo info;
            info.amount = damage;
            info.direction = direction;
            info.element = element;
            info.source = source;
            info.parryable = parryable;
            info.projectile = true;
            scene.events().emit(HitEvent{owner.id, h->owner()->id, info});
            hitPending_ = true;
            return;
        }
    }
    if (traveled_ >= range) land(owner);
}

void Projectile::breakAt(GameObject& owner, Scene2D& scene, glm::vec2 at) {
    emitMovementCue(scene, owner, "break", 1.0f, at);
    land(owner);
}

// The owner stays active and hidden, so its break particles keep playing while it waits in the pool.
void Projectile::land(GameObject& owner) {
    flying_ = false;
    hitPending_ = false;
    owner.visible = false;
}

GameObject* ProjectilePool::spawn(const std::string& prefab, glm::vec2 pos, glm::vec2 dir, const std::string& team,
                                  GameObjectId source) {
    if (!scene) return nullptr;
    std::vector<GameObjectId>& ids = pool_[prefab];
    std::erase_if(ids, [&](GameObjectId id) { return !scene->findObject(id); }); // destroyed since (unload, clear)
    GameObject* object = nullptr;
    for (GameObjectId id : ids) {
        GameObject* o = scene->findObject(id);
        if (const Projectile* p = o->getComponent<Projectile>(); p && !p->flying()) {
            object = o;
            break;
        }
    }
    if (!object) {
        PrefabLibrary* prefabs = scene->service<PrefabLibrary>();
        if (!prefabs) {
            std::cerr << "ProjectilePool: no PrefabLibrary service for '" << prefab << "'\n";
            return nullptr;
        }
        object = prefabs->instantiate(*scene, prefab);
        if (!object) return nullptr; // the library warned
        if (!object->getComponent<Projectile>()) {
            static std::set<std::string> warned;
            if (warned.insert(prefab).second) std::cerr << "ProjectilePool: prefab '" << prefab << "' has no Projectile component\n";
            scene->destroy(object->id);
            return nullptr;
        }
        ids.push_back(object->id);
    }
    object->transform.position = pos;
    object->getComponent<Projectile>()->launch(*object, dir, team, source);
    return object;
}

std::size_t ProjectilePool::pooled(const std::string& prefab) const {
    auto it = pool_.find(prefab);
    return it == pool_.end() ? 0 : it->second.size();
}
