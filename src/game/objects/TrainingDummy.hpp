#pragma once

#include "engine/world/Combat.hpp"
#include "engine/world/Effects.hpp"
#include "engine/world/Resources.hpp"
#include "engine/world/Scene2D.hpp"

// A training dummy breaks at 0 health (spec D3 decision 10): movement cue "break" (effect trigger
// "move:break"), its Hurtbox disabled (out of the combat grid) and its sprite hidden. After respawnTime
// seconds of scene time it is back with full health. Nothing goes to the WorldState.
// Prefabs: assets/prefabs/training_dummy*.prefab.json.
class TrainingDummy : public Component {
    MUSH2D_COMPONENT(TrainingDummy)
public:
    MUSH2D_EXPORT(float, respawnTime, 3.0f)

    bool broken() const { return broken_; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override {
        Resources* resources = owner.getComponent<Resources>();
        Hurtbox* hurtbox = owner.getComponent<Hurtbox>();
        if (!broken_) {
            if (!resources || resources->health() > 0) return;
            broken_ = true;
            remaining_ = respawnTime;
            if (hurtbox) hurtbox->enabled = false;
            owner.visible = false;
            emitMovementCue(scene, owner, "break", 1.0f, owner.world().position);
            return;
        }
        if ((remaining_ -= dt) > 0.0f) return;
        broken_ = false;
        if (resources) resources->refill();
        if (hurtbox) hurtbox->enabled = true;
        owner.visible = true;
    }

private:
    bool broken_ = false;
    float remaining_ = 0.0f;
};
