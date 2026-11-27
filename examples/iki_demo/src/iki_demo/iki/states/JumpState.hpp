#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// Rising. Letting go of Z cuts the jump once (variable height); a stomp bounce or a knockback is never cut.
struct JumpState : State<IkiObject> {
    const char* name() const override { return "jump"; }
    const char* update(IkiObject& iki, float dt) override {
        iki.runHorizontal(dt, iki.moveX());
        iki.tryJump();
        if (iki.jumpCuttable && !iki.jumpHeld() && iki.velocity_.y > 0.0f) {
            iki.velocity_.y *= iki.jumpCutFactor;
            iki.jumpCuttable = false;
        }
        iki.applyGravity(dt);
        iki.moveAndCollide(dt);
        if (const char* grab = iki.nextGrab()) return grab;
        return iki.nextLocomotion(name());
    }
};
