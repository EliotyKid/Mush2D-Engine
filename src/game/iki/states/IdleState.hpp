#pragma once

#include "engine/world/StateMachine.hpp"
#include "game/objects/IkiObject.hpp"

// On the ground, no direction held: slows down with groundDecel. Z jumps (buffer and coyote: JumpAssist).
struct IdleState : State<IkiObject> {
    const char* name() const override { return "idle"; }
    const char* update(IkiObject& iki, float dt) override {
        iki.runHorizontal(dt, iki.moveX());
        iki.tryJump();
        iki.applyGravity(dt);
        iki.moveAndCollide(dt);
        return iki.nextLocomotion(name());
    }
};
