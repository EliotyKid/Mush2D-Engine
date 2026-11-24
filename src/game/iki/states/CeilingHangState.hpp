#pragma once

#include "engine/world/StateMachine.hpp"
#include "game/objects/IkiObject.hpp"

// Hanging from an ice ceiling, no gravity. Left / right move at ceilingMoveSpeed. Leaves by: down or Z (falls,
// no jump), holding toward an ice wall (CeilingHang -> WallCling), or the end of the ice (falls).
struct CeilingHangState : State<IkiObject> {
    static constexpr float Stick = 8.0f; // u/s up: snaps into the ceiling from the grab reach; the ceiling stops it

    const char* name() const override { return "ceiling"; }
    void enter(IkiObject& iki) override {
        iki.velocity_ = glm::vec2(0.0f);
        iki.jumpCuttable = false;
    }
    const char* update(IkiObject& iki, float dt) override {
        if (iki.downHeld() || iki.jump.hasBufferedJump()) {
            iki.letGoCeiling();
            return "fall";
        }
        const float x = iki.moveX();
        if (x != 0.0f) iki.facing = x;
        iki.velocity_ = {x * iki.ceilingMoveSpeed, Stick};
        const float x0 = iki.transform.position.x;
        iki.moveAndCollide(dt, false);
        iki.climbCue(dt, std::abs(iki.transform.position.x - x0) > 1e-4f, {0.0f, 1.6f});
        const int side = x > 0.0f ? 1 : x < 0.0f ? -1 : 0;
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
