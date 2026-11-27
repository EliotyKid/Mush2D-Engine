#pragma once

#include <algorithm>

// Remembers one press for a short time, so a press made a little too early still counts (a jump pressed just
// before landing). Call step once per scene step with that step's dt: a frozen step (hit stop) runs no step,
// or passes dt 0, so the press does not age while the scene is frozen.
struct InputBuffer {
    float window = 0.0f; // seconds a press stays valid (the project's action buffer)

    InputBuffer() = default;
    explicit InputBuffer(float seconds) : window(seconds) {}

    // A press fills the window again; otherwise the time left goes down by dt (never below zero).
    void step(bool pressed, float dt) { left_ = pressed ? window : std::max(0.0f, left_ - dt); }
    // A press is still waiting to be used.
    bool active() const { return left_ > 0.0f; }
    // The press was used (or must be dropped): it counts no more.
    void consume() { left_ = 0.0f; }

private:
    float left_ = 0.0f; // seconds left for the latest press
};
