#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// Hit (IkiObject::hurt set the knockback): no control for hurtDuration, then back to moving.
struct HurtState : State<IkiObject> {
    const char* name() const override { return "hurt"; }
    const char* update(IkiObject& iki, float dt) override {
        iki.runHorizontal(dt, 0.0f);
        iki.applyGravity(dt);
        iki.moveAndCollide(dt);
        if (iki.states().timeInState() < iki.hurtDuration) return nullptr;
        return iki.nextLocomotion(name());
    }
};
