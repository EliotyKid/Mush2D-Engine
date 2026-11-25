#pragma once

#include "game/objects/TrainingDummy.hpp"

#include "engine/world/Combat.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/Projectile.hpp"
#include "engine/world/Resources.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/World.hpp"

#include <string>

#include <glm/glm.hpp>

// Every shootInterval seconds (scene time) fires `projectile` from the ProjectilePool at the player while the
// player is in reach: within `range` u, alive (Resources health > 0, when it has Resources) and, with a World
// service and an LDtk room, standing in the shooter's own room (a neighbour room is loaded and may be in range
// through a doorway). Aims from its Hurtbox centre to the player's (positions without one), turns to face the
// shot and emits the movement cue "shoot". Silent while Stunned or broken (TrainingDummy). Stunned, broken or
// the player out of reach restarts the wait: the next shot comes a full interval later. No state machine.
class DummyShooter : public Component {
    MUSH2D_COMPONENT(DummyShooter)
public:
    MUSH2D_EXPORT(float, shootInterval, 2.5f)
    MUSH2D_EXPORT(float, range, 10.0f)
    MUSH2D_EXPORT(std::string, projectile, std::string("dummy_bolt"))

    void onStep(GameObject& owner, Scene2D& scene, float dt) override {
        const Stunned* stun = owner.getComponent<Stunned>();
        const TrainingDummy* dummy = owner.getComponent<TrainingDummy>();
        if ((stun && stun->active()) || (dummy && dummy->broken())) {
            elapsed_ = 0.0f;
            return;
        }
        GameObject* player = findPlayer(scene);
        auto* pool = scene.service<ProjectilePool>();
        const glm::vec2 from = centre(owner);
        const glm::vec2 aim = player ? centre(*player) - from : glm::vec2(0.0f);
        if (!pool || !player || !inReach(scene, owner, *player, glm::length(aim))) {
            elapsed_ = 0.0f;
            return;
        }
        if ((elapsed_ += dt) < shootInterval) return;
        elapsed_ = 0.0f;
        owner.flipX = aim.x < 0.0f;
        const Hurtbox* own = owner.getComponent<Hurtbox>();
        pool->spawn(projectile, from, aim, own ? own->team : std::string("enemy"), owner.id);
        emitMovementCue(scene, owner, "shoot");
    }

private:
    bool inReach(const Scene2D& scene, const GameObject& owner, const GameObject& player, float distance) const {
        if (distance > range || distance < 1e-4f) return false;
        if (const auto* r = player.getComponent<Resources>(); r && r->health() == 0) return false;
        const World* world = scene.service<World>();
        if (!world || owner.roomIid.empty()) return true;
        const int mine = world->roomOf(owner.roomIid);
        return mine < 0 || world->roomAt(player.world().position) == mine;
    }
    static glm::vec2 centre(const GameObject& o) {
        const Hurtbox* h = o.getComponent<Hurtbox>();
        if (!h || !h->owner()) return o.world().position;
        const Collision2D::AABB box = h->box();
        return 0.5f * (box.min + box.max);
    }
    float elapsed_ = 0.0f;
};
