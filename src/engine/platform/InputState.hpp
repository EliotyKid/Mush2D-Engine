#pragma once

#include <initializer_list>

struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct InputState {
    ButtonState moveLeft;
    ButtonState moveRight;
    ButtonState moveUp;
    ButtonState moveDown;

    ButtonState cameraLeft;
    ButtonState cameraRight;
    ButtonState cameraUp;
    ButtonState cameraDown;

    ButtonState zoomIn;
    ButtonState zoomOut;

    ButtonState jump; // Z (platformer: the up arrow also jumps)
    ButtonState dash;
    ButtonState hit;
    ButtonState toggleFollow;
    ButtonState respawn;

    ButtonState dragCamera;

    double mouseX = 0.0;
    double mouseY = 0.0;
    double mouseDeltaX = 0.0;
    double mouseDeltaY = 0.0;

    double scrollDeltaX = 0.0;
    double scrollDeltaY = 0.0;
};

// Adds earlier's presses and releases to now: input read in frames the scene did not step (hit stop) keeps
// its edges until a step reads them.
inline void keepEdges(InputState& now, const InputState& earlier) {
    for (ButtonState InputState::*b : {&InputState::moveLeft, &InputState::moveRight, &InputState::moveUp, &InputState::moveDown,
                                       &InputState::cameraLeft, &InputState::cameraRight, &InputState::cameraUp,
                                       &InputState::cameraDown, &InputState::zoomIn, &InputState::zoomOut, &InputState::jump,
                                       &InputState::dash, &InputState::hit, &InputState::toggleFollow, &InputState::respawn,
                                       &InputState::dragCamera}) {
        (now.*b).pressed |= (earlier.*b).pressed;
        (now.*b).released |= (earlier.*b).released;
    }
}