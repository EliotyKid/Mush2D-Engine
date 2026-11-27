#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// Hanging from an ice ceiling, no gravity. Left / right move at ceilingMoveSpeed. Leaves by: down or Z (falls,
// no jump), holding toward an ice wall (CeilingHang -> WallCling), or the end of the ice (falls).
struct CeilingHangState : State<IkiObject> {
    const char* name() const override { return "ceiling"; }
    void enter(IkiObject& iki) override {
        iki.velocity_ = glm::vec2(0.0f);
        iki.jumpCuttable = false;
        iki.grabCue(iki.headOffset()); // ice_stake at the head
    }
    const char* update(IkiObject& iki, float dt) override {
        if (iki.downHeld() || iki.jump.hasBufferedJump()) {
            iki.letGoCeiling();
            return "fall";
        }
        const float x = iki.moveX();
        if (x != 0.0f) iki.facing = x;
        iki.velocity_ = {x * iki.ceilingMoveSpeed, iki.ceilingStickSpeed}; // the ceiling stops the stick
        const float x0 = iki.transform.position.x;
        iki.moveAndCollide(dt, false);
        iki.climbCue(dt, std::abs(iki.transform.position.x - x0) > 1e-4f, iki.headOffset());
        const int side = iki.moveSide();
        if (side != 0 && iki.iceWall(side, true)) {
            iki.wallSide = side;
            return "wall";
        }
        if (iki.grounded) return iki.nextLocomotion("");
        if (iki.iceCeiling()) return nullptr;
        iki.letGoCeiling();
        return "fall";
    }
};
