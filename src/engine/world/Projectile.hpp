#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/Damage.hpp"
#include "engine/world/GameObject.hpp"

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Hurtbox;
struct Segment;

// A straight shot. Moves `speed` u/s along `direction` through the scene's CollisionWorld (one-way segments
// let it pass) for up to `range` u. It breaks on a wall or on its first hit: emits the movement cue "break"
// (effect trigger "move:break") where it broke, hides and waits in the ProjectilePool. On a hit it emits a
// HitEvent (DamageInfo::projectile, direction = its velocity) and breaks one step later, unless the target's
// parry reflected it first. Reaching `range` hides it without a break.
class Projectile : public Component {
    MUSH2D_COMPONENT(Projectile)
public:
    MUSH2D_EXPORT(float, speed, 10.0f)
    MUSH2D_EXPORT(float, range, 8.0f)
    MUSH2D_EXPORT(std::string, team, std::string("enemy"))
    MUSH2D_EXPORT(int, damage, 1)
    MUSH2D_EXPORT(Element, element, Element::Ice)
    MUSH2D_EXPORT(bool, parryable, true)
    MUSH2D_EXPORT(glm::vec2, size, glm::vec2(0.25f, 0.25f)) // the box that hits hurtboxes

    glm::vec2 direction{1.0f, 0.0f}; // unit
    GameObjectId source{};           // who it counts for (DamageInfo::source)
    bool reflected = false;

    // Starts a flight from where the owner is (ProjectilePool::spawn calls it).
    void launch(GameObject& owner, glm::vec2 dir, const std::string& newTeam, GameObjectId newSource);
    // Turns it around once: the new team and source, the same damage. A second call does nothing.
    void reflect(GameObjectId newSource, const std::string& newTeam);
    bool flying() const { return flying_; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    void breakAt(GameObject& owner, Scene2D& scene, glm::vec2 at);
    void land(GameObject& owner); // back to the pool

    bool flying_ = false;
    bool hitPending_ = false; // hit something this step: breaks next step unless reflected
    float traveled_ = 0.0f;
    GameObject* owner_ = nullptr; // set by launch: reflect turns the sprite
    std::vector<Hurtbox*> found_;
    std::vector<const Segment*> segments_;
};

// Scene service: projectiles by prefab, reused once they landed. `scene` must be set (the game does it);
// it needs the PrefabLibrary service. Stale objects (destroyed by a room unload, a clear() on death restore
// or hot reload) are forgotten at the next spawn, never touched. A projectile created while a room spawns
// (scene.spawnRoomIid set, e.g. from an onCreate during LevelLoader::loadRoom) belongs to that room and is
// destroyed when it unloads; otherwise it has no room and stays until clear().
class ProjectilePool {
public:
    Scene2D* scene = nullptr;

    // A flying projectile of `prefab` at pos (world), or nullptr (no scene / PrefabLibrary, unknown prefab,
    // or a prefab without a Projectile component: warns). Speed and damage come from the prefab.
    GameObject* spawn(const std::string& prefab, glm::vec2 pos, glm::vec2 dir, const std::string& team, GameObjectId source);
    std::size_t pooled(const std::string& prefab) const; // objects kept for prefab (flying or not)

private:
    std::map<std::string, std::vector<GameObjectId>> pool_;
};
