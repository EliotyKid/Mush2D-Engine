#pragma once

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