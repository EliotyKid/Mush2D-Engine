#pragma once

#include "engine/world/InputBuffer.hpp"

#include <algorithm>

// Jump timing, no scene: coyote time (jump shortly after leaving the ground) and jump buffer
// (a press shortly before landing still jumps). Call tick once per frame, then consumeJump.
// jumpPressed is the press edge (this frame only), not the held button. The owner must not report
// grounded on the frame after a jump: a grounded tick refills the coyote time (a second jump).
struct JumpAssist {
    float coyoteTime = 0.1f;
    // The jump press buffer. Its window is the default until the owner sets it (from the project's action buffer).
    InputBuffer buffer{0.12f};

    void tick(float dt, bool grounded, bool jumpPressed) {
        grounded_ = grounded;
        coyote_ = grounded ? coyoteTime : std::max(0.0f, coyote_ - dt);
        buffer.step(jumpPressed, dt);
    }
    // True when a press is buffered and the body is on the ground or in coyote time; clears both.
    bool consumeJump() {
        if (!buffer.active() || !(grounded_ || coyote_ > 0.0f)) return false;
        buffer.consume();
        coyote_ = 0.0f;
        grounded_ = false;
        return true;
    }
    bool hasBufferedJump() const { return buffer.active(); }
    void clearBuffer() { buffer.consume(); }
    void startCoyote() { coyote_ = coyoteTime; } // wall coyote: just left a wall

private:
    float coyote_ = 0.0f;
    bool grounded_ = false;
};
