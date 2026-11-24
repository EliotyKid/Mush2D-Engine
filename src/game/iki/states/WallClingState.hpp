#pragma once

#include "engine/world/StateMachine.hpp"
#include "game/objects/IkiObject.hpp"

// Clinging to an ice wall (iki.wallSide), no gravity, no time limit. Up climbs and down descends at
// wallClimbSpeed; letting go of the direction does not let go of the wall. Leaves by: Z (wall jump), the floor,
// up alone into an ice ceiling (inner corner), or the end of the ice: a ledge hop when climbing up with
// nothing above, else a fall with wall coyote time.
struct WallClingState : State<IkiObject> {
    const char* name() const override { return "wall"; }
    void enter(IkiObject& iki) override {
        iki.velocity_ = glm::vec2(0.0f);
        iki.facing = float(iki.wallSide);
        iki.jumpCuttable = false;
        iki.grabCue(iki.handsOffset(iki.wallSide)); // ice_stake at the hands
    }
    const char* update(IkiObject& iki, float dt) override {
        const float side = float(iki.wallSide);
        if (iki.jump.hasBufferedJump()) {
            iki.wallJump();
            iki.moveAndCollide(dt, false);
            return "jump";
        }
        const float climb = (float(iki.upHeld()) - float(iki.downHeld())) * iki.wallClimbSpeed;
        // The first step closes the grab gap: GrabGap is probeWall's default reach, so the gap is at most that.
        // After that no push into the wall: pressed against a ledge's corner, the controller would stand the
        // Iki on it.
        constexpr float GrabGap = 0.05f;
        const bool first = iki.states().timeInState() <= dt;
        iki.velocity_ = {first ? side * GrabGap / dt : 0.0f, climb};
        const float y0 = iki.transform.position.y;
        iki.moveAndCollide(dt, false);
        iki.climbCue(dt, std::abs(iki.transform.position.y - y0) > 1e-4f, iki.handsOffset(iki.wallSide));
        if (iki.grounded) return iki.nextLocomotion(""); // the wall's foot
        // Up alone in an ice corner goes to the ceiling; holding toward the wall keeps the wall (no flicker).
        if (climb > 0.0f && iki.moveX() * side <= 0.0f && iki.canGrabCeiling()) return "ceiling";
        if (iki.iceWall(iki.wallSide, false)) return nullptr;
        if (climb > 0.0f && !iki.anyWall(iki.wallSide)) {
            iki.hopLedge();
            return "jump";
        }
        iki.letGoWall();
        return "fall";
    }
};
