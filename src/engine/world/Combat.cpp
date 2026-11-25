#include "engine/world/Combat.hpp"

#include "engine/world/Collision.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/Projectile.hpp"
#include "engine/world/Resources.hpp"
#include "engine/world/Scene2D.hpp"

#include <algorithm>
#include <cmath>

Collision2D::AABB combatBox(const GameObject& owner, glm::vec2 size, glm::vec2 offset) {
    if (owner.flipX) offset.x = -offset.x;
    const glm::vec2 center = owner.world().position + offset;
    return {center - 0.5f * size, center + 0.5f * size};
}

void CombatWorld::add(Hurtbox& hurtbox) {
    if (!hurtbox.owner()) return;
    const GameObjectId id = hurtbox.owner()->id;
    hurtboxes_[id] = &hurtbox;
    grid_.update(id, hurtbox.box());
}

void CombatWorld::remove(Hurtbox& hurtbox) {
    if (!hurtbox.owner()) return;
    const GameObjectId id = hurtbox.owner()->id;
    auto it = hurtboxes_.find(id);
    if (it == hurtboxes_.end() || it->second != &hurtbox) return;
    hurtboxes_.erase(it);
    grid_.remove(id);
}

void CombatWorld::query(const Collision2D::AABB& box, std::vector<Hurtbox*>& out) {
    out.clear();
    grid_.query(box, ids_);
    for (GameObjectId id : ids_) {
        Hurtbox* h = hurtboxes_.at(id);
        const GameObject* o = h->owner();
        // Grid boxes may lag one step behind (refreshed in Hurtbox::onStep): test the current box too.
        if (h->enabled && !h->removing() && o->active && !o->pendingDestroy() && Collision2D::intersects(box, h->box()))
            out.push_back(h);
    }
}

void Hurtbox::onAttach(GameObject& owner, Scene2D& scene) {
    owner_ = &owner;
    world_ = scene.service<CombatWorld>();
    if (world_) world_->add(*this);
}

void Hurtbox::onStep(GameObject&, Scene2D& scene, float) {
    if (!world_) world_ = scene.service<CombatWorld>(); // the service was registered after the attach
    if (world_) world_->add(*this);
}

void Hurtbox::onDetach(GameObject&, Scene2D&) {
    if (world_) world_->remove(*this);
    world_ = nullptr;
    owner_ = nullptr;
}

namespace {

GameObjectId rootOf(Scene2D& scene, const GameObject& o) {
    const GameObject* top = &o;
    while (const GameObject* up = scene.findObject(top->parent())) top = up;
    return top->id;
}

// Effect trigger on `o` at its position; direction mirrors the particles.
void fireHit(Scene2D& scene, GameObject& o, const char* key, float strength, int direction) {
    if (EffectTriggers* fx = o.getComponent<EffectTriggers>(); fx && fx->enabled && !fx->removing())
        fx->fire(o, scene, key, strength, o.world().position, direction);
}

} // namespace

void Hitbox::onStep(GameObject& owner, Scene2D& scene, float) {
    CombatWorld* world = active_ ? scene.service<CombatWorld>() : nullptr;
    if (!world) return;
    world->query(combatBox(owner, size, offset), found_);
    const glm::vec2 from = owner.world().position;
    for (Hurtbox* h : found_) {
        GameObject& target = *h->owner();
        if (h->team == team || std::find(hit_.begin(), hit_.end(), target.id) != hit_.end()) continue;
        hit_.push_back(target.id);
        DamageInfo info;
        info.amount = damage;
        const glm::vec2 d = target.world().position - from;
        info.direction = glm::length(d) > 1e-5f ? glm::normalize(d) : glm::vec2(owner.flipX ? -1.0f : 1.0f, 0.0f);
        info.element = element;
        info.knockback = knockback;
        info.source = source ? source : rootOf(scene, owner);
        info.parryable = parryable;
        scene.events().emit(HitEvent{owner.id, target.id, info});
    }
}

int Damageable::finalAmount(const DamageInfo& info) const {
    auto it = multipliers.find(info.element);
    const float m = it == multipliers.end() ? 1.0f : it->second;
    if (!(m > 0.0f) || info.amount <= 0) return 0;
    return std::max(1, static_cast<int>(std::lround(info.amount * m)));
}

void Damageable::onAttach(GameObject& owner, Scene2D& scene) {
    const GameObjectId id = owner.id;
    Scene2D* s = &scene;
    subscription_ = scene.events().subscribe<HitEvent>([this, id, s](const HitEvent& e) {
        if (e.target != id) return;
        if (GameObject* o = s->findObject(id)) take(*o, *s, e);
    });
}

// Spec D3 decision 2. The invulnerability check comes first: a hit during it is ignored, so it cannot be parried.
void Damageable::take(GameObject& owner, Scene2D& scene, const HitEvent& hit) {
    if (!enabled || removing() || invulnerable()) return;
    Resources* resources = owner.getComponent<Resources>();
    if (resources && resources->health() == 0) return; // already dead
    const DamageInfo& info = hit.info;
    const int dir = info.direction.x < 0.0f ? -1 : 1;

    const HitResult result = guard ? guard(info) : HitResult::Hit;
    if (result == HitResult::Blocked) return fireHit(scene, owner, "hit:blocked", 1.0f, dir);
    if (result == HitResult::Parried) {
        if (GameObject* src = scene.findObject(info.source))
            if (Stunned* stun = src->getComponent<Stunned>()) stun->start(parryStun);
        if (GameObject* attacker = scene.findObject(hit.attacker))
            if (Projectile* p = attacker->getComponent<Projectile>(); p && p->flying()) {
                const Hurtbox* mine = owner.getComponent<Hurtbox>();
                p->reflect(owner.id, mine ? mine->team : p->team);
            }
        return fireHit(scene, owner, "hit:parried", 1.0f, dir);
    }

    const int amount = finalAmount(info);
    if (amount == 0) return fireHit(scene, owner, "hit:immune", 1.0f, dir);
    if (resources) resources->damage(amount);
    invulnerable_ = invulnerableTime;
    scene.events().emit(DamageTaken{owner.id, info, amount});
    fireHit(scene, owner, "hit:taken", float(amount), dir);
    GameObject* dealer = scene.findObject(info.source);
    if (!dealer) dealer = scene.findObject(hit.attacker);
    if (dealer) fireHit(scene, *dealer, "hit:dealt", float(amount), dealer->flipX ? -1 : 1);
}
