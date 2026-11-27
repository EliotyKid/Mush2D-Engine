#pragma once

// Reusable combat: any object gets it through prefab components (manual: "Combate").
//   Hitbox     the attack: while active it hits each Hurtbox of another team once and emits HitEvent.
//   Hurtbox    where the object can be hit; lives in the CombatWorld service (its own SpatialGrid).
//   Damageable takes the HitEvents aimed at its owner: guard, invulnerability, multipliers, Resources, DamageTaken.
//   Stunned    a timer other code checks (the parry stuns info.source when it has one).
// Effect triggers raised here: "hit:dealt" on info.source (else the attacker), "hit:taken" (damage > 0),
// "hit:immune", "hit:blocked" and "hit:parried" on the target.

#include "engine/world/Component.hpp"
#include "engine/world/Damage.hpp"
#include "engine/world/EventBus.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/SpatialGrid.hpp"

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

class Hurtbox;

// attacker = the object holding the Hitbox (or the Projectile); target = the object holding the Hurtbox.
struct HitEvent {
    GameObjectId attacker, target;
    DamageInfo info;
};

enum class HitResult { Hit, Blocked, Parried };

// The target lost finalAmount (> 0) health. Emitted by Damageable.
struct DamageTaken {
    GameObjectId target;
    DamageInfo info;
    int finalAmount;
};

// Scene service: the hurtboxes, in a SpatialGrid of their own (the scene's collision grid is not touched).
// Hurtboxes add themselves on attach and remove themselves on detach, which every destroy path runs
// (Scene2D::destroy, room unload, clear() on death restore and hot reload). Not owned.
class CombatWorld {
public:
    void add(Hurtbox& hurtbox); // also refreshes the box of one already added
    void remove(Hurtbox& hurtbox);
    // Replaces `out` with the enabled hurtboxes of active objects whose current box overlaps `box`.
    void query(const Collision2D::AABB& box, std::vector<Hurtbox*>& out);
    std::size_t size() const { return hurtboxes_.size(); }

private:
    SpatialGrid grid_;
    std::unordered_map<GameObjectId, Hurtbox*> hurtboxes_; // one per object: a second one on the same object replaces it
    std::vector<GameObjectId> ids_;
};

// The object at the top of o's parents (o itself for a root).
const GameObject& combatRoot(const Scene2D& scene, const GameObject& o);
// World box of a size/offset pair on `owner`. Mirroring follows the root's facing (its flipX): facing left,
// the offset's x flips and a child's place is mirrored around the root, so a child placed in front of a
// right-facing owner stays in front when it turns.
Collision2D::AABB combatBox(const Scene2D& scene, const GameObject& owner, glm::vec2 size, glm::vec2 offset);

class Hurtbox : public Component {
    MUSH2D_COMPONENT(Hurtbox)
public:
    MUSH2D_EXPORT(glm::vec2, size, glm::vec2(1.0f, 1.0f))
    MUSH2D_EXPORT(glm::vec2, offset, glm::vec2(0.0f, 0.0f))
    MUSH2D_EXPORT(std::string, team, std::string("enemy"))
    // `enabled` (exported by Component): false = never hit, as if it was not in the grid.

    GameObject* owner() const { return owner_; }
    Collision2D::AABB box() const { return owner_ ? combatBox(*scene_, *owner_, size, offset) : Collision2D::AABB{}; }

    void onAttach(GameObject& owner, Scene2D& scene) override;
    void onStep(GameObject& owner, Scene2D& scene, float dt) override; // keeps the grid box current
    void onDetach(GameObject& owner, Scene2D& scene) override;

private:
    GameObject* owner_ = nullptr;
    const Scene2D* scene_ = nullptr;
    CombatWorld* world_ = nullptr; // the one it was added to
};

class Hitbox : public Component {
    MUSH2D_COMPONENT(Hitbox)
public:
    MUSH2D_EXPORT(glm::vec2, size, glm::vec2(1.0f, 1.0f))
    MUSH2D_EXPORT(glm::vec2, offset, glm::vec2(0.0f, 0.0f))
    MUSH2D_EXPORT(int, damage, 1)
    MUSH2D_EXPORT(std::string, element, std::string()) // a name from project.json; empty = none
    MUSH2D_EXPORT(float, knockback, 0.0f)
    MUSH2D_EXPORT(std::string, team, std::string("player"))
    MUSH2D_EXPORT(bool, parryable, true)
    // The attack's window after it starts, for the code driving it when the animation has no hit_on / hit_off.
    MUSH2D_EXPORT(float, activeStart, 0.05f)
    MUSH2D_EXPORT(float, activeEnd, 0.15f)

    // Who the hit counts for (DamageInfo::source). Empty = the owner's root object.
    GameObjectId source{};

    // Starts a new activation: every hurtbox can be hit once again.
    void activate() { active_ = true; hit_.clear(); }
    void deactivate() { active_ = false; }
    bool active() const { return active_; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    bool active_ = false;
    std::vector<GameObjectId> hit_; // hit during this activation
    std::vector<Hurtbox*> found_;
};

class Damageable : public Component {
    MUSH2D_COMPONENT(Damageable)
public:
    MUSH2D_EXPORT(float, invulnerableTime, 0.2f)
    MUSH2D_EXPORT(float, parryStun, 1.0f) // seconds of Stunned for info.source on a parry (if it has Stunned)

    // Element -> damage multiplier (missing = 1, 0 = immune). Prefab section "multipliers": {"<element>": 2.0}.
    std::map<std::string, float> multipliers;
    // Asked after the invulnerability check; Blocked and Parried deal no damage and start no invulnerability.
    // info.parryable false: the guard should answer Blocked (and charge its block); a Parried is turned into
    // Blocked anyway (no stun, no reflection).
    std::function<HitResult(const DamageInfo&)> guard;

    bool invulnerable() const { return invulnerable_ > 0.0f; }
    void clearInvulnerability() { invulnerable_ = 0.0f; } // a respawn
    // Nearest integer of amount * multiplier; at least 1 when the multiplier is > 0; 0 = immune.
    int finalAmount(const DamageInfo& info) const;

    void onAttach(GameObject& owner, Scene2D& scene) override;
    void onStep(GameObject&, Scene2D&, float dt) override { invulnerable_ -= dt; }
    void onDetach(GameObject&, Scene2D&) override { subscription_.reset(); }

private:
    void take(GameObject& owner, Scene2D& scene, const HitEvent& hit);
    EventBus::Subscription subscription_;
    float invulnerable_ = 0.0f;
};

// Stun timer (scene time: a hit stop holds it). Other components check active().
class Stunned : public Component {
    MUSH2D_COMPONENT(Stunned)
public:
    void start(float seconds) { if (seconds > remaining_) remaining_ = seconds; } // keeps the longer stun
    bool active() const { return remaining_ > 0.0f; }
    void onStep(GameObject&, Scene2D&, float dt) override { remaining_ -= dt; }

private:
    float remaining_ = 0.0f;
};
