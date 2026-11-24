#pragma once

#include "engine/world/StateMachine.hpp"
#include "game/objects/IkiObject.hpp"

// In the air, not rising. Just after leaving a ledge, coyote time still lets Z jump.
struct FallState : State<IkiObject> {
    const char* name() const override { return "fall"; }
    const char* update(IkiObject& iki, float dt) override {
        iki.runHorizontal(dt, iki.moveX());
        iki.tryJump();
        iki.applyGravity(dt);
        iki.moveAndCollide(dt);
        return iki.nextLocomotion(name());
    }
};
