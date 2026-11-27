#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// Health 0: no control; the body still falls and stops. Only IkiObject::respawn leaves it.
struct DeadState : State<IkiObject> {
    const char* name() const override { return "dead"; }
    const char* update(IkiObject& iki, float dt) override {
        iki.runHorizontal(dt, 0.0f);
        iki.applyGravity(dt);
        iki.moveAndCollide(dt);
        return nullptr;
    }
};
