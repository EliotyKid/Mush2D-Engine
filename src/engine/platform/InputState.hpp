#pragma once

struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

// The engine's fixed developer-tool controls (free camera, zoom, follow toggle, mouse drag and wheel). They are
// the same in every game and run in real time, so they are not project actions. What the player does lives in
// the named actions (Actions.hpp), declared by the project.
struct InputState {
    ButtonState cameraLeft;  // A
    ButtonState cameraRight; // D
    ButtonState cameraUp;    // W
    ButtonState cameraDown;  // S

    ButtonState zoomIn;  // E
    ButtonState zoomOut; // Q

    ButtonState toggleFollow; // F: the camera follows the player or stays free

    ButtonState dragCamera; // middle mouse button

    double mouseX = 0.0;
    double mouseY = 0.0;
    double mouseDeltaX = 0.0;
    double mouseDeltaY = 0.0;

    double scrollDeltaX = 0.0;
    double scrollDeltaY = 0.0;
};
