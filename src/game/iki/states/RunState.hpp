#pragma once

#include "engine/world/StateMachine.hpp"
#include "game/objects/IkiObject.hpp"

// On the ground with a direction held: speeds up to runSpeed with groundAccel.
struct RunState : State<IkiObject> {
    const char* name() const override { return "run"; }
    const char* update(IkiObject& iki, float dt) override {
        iki.runHorizontal(dt, iki.moveX());
        iki.tryJump();
        iki.applyGravity(dt);
        iki.moveAndCollide(dt);
        return iki.nextLocomotion(name());
    }
};
