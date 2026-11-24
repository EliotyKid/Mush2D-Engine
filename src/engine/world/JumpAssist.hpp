#pragma once

#include <algorithm>

// Jump timing, no scene: coyote time (jump shortly after leaving the ground) and jump buffer
// (a press shortly before landing still jumps). Call tick once per frame, then consumeJump.
// jumpPressed is the press edge (this frame only), not the held button. The owner must not report
// grounded on the frame after a jump: a grounded tick refills the coyote time (a second jump).
struct JumpAssist {
    float coyoteTime = 0.1f, jumpBuffer = 0.12f;

    void tick(float dt, bool grounded, bool jumpPressed) {
        grounded_ = grounded;
        coyote_ = grounded ? coyoteTime : std::max(0.0f, coyote_ - dt);
        buffer_ = jumpPressed ? jumpBuffer : std::max(0.0f, buffer_ - dt);
    }
    // True when a press is buffered and the body is on the ground or in coyote time; clears both.
    bool consumeJump() {
        if (buffer_ <= 0.0f || !(grounded_ || coyote_ > 0.0f)) return false;
        buffer_ = coyote_ = 0.0f;
        grounded_ = false;
        return true;
    }
    bool hasBufferedJump() const { return buffer_ > 0.0f; }
    void clearBuffer() { buffer_ = 0.0f; }
    void startCoyote() { coyote_ = coyoteTime; } // wall coyote: just left a wall

private:
    float coyote_ = 0.0f, buffer_ = 0.0f;
    bool grounded_ = false;
};
