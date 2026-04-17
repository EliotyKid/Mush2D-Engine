#pragma once

#include "InputState.hpp"

#include <GLFW/glfw3.h>

class GlfwInput {
public:
    void attachWindow(GLFWwindow* window);
    void beginFrame();
    void poll();
    const InputState& getState() const;

    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

private:
    GLFWwindow* window = nullptr;
    InputState state{};

    bool previousMoveLeft = false;
    bool previousMoveRight = false;
    bool previousMoveUp = false;
    bool previousMoveDown = false;

    bool previousCameraLeft = false;
    bool previousCameraRight = false;
    bool previousCameraUp = false;
    bool previousCameraDown = false;

    bool previousZoomIn = false;
    bool previousZoomOut = false;

    bool previousDash = false;
    bool previousHit = false;
    bool previousToggleFollow = false;
    bool previousRespawn = false;

    bool previousDragCamera = false;

private:
    void updateButton(ButtonState& button, bool currentDown, bool& previousDown);
};