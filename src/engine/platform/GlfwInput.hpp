#pragma once

#include "InputState.hpp"
#include "engine/platform/Actions.hpp"

#include <GLFW/glfw3.h>

class GlfwInput {
public:
    void attachWindow(GLFWwindow* window);
    void beginFrame();
    void poll();
    // The fixed developer-tool controls (camera, zoom, mouse). Updated by poll().
    const InputState& getState() const;
    // The named actions (keyboard and gamepad on the same action). Updated by poll(), once per frame.
    ActionMap& actions() { return actions_; }
    const ActionMap& actions() const { return actions_; }
    // Reads the devices now into a plain snapshot (keys are released while the UI has the keyboard).
    RawInput readRaw() const;
    bool isKeyDown(int key) const { return !keyboardBlocked && glfwGetKey(window, key) == GLFW_PRESS; }
    // UI (ImGui) captured the keyboard / mouse: the game sees them as idle.
    void setBlocked(bool keyboard, bool mouse) { keyboardBlocked = keyboard; mouseBlocked = mouse; }

    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

private:
    GLFWwindow* window = nullptr;
    InputState state{};
    ActionMap actions_;
    bool keyboardBlocked = false;
    bool mouseBlocked = false;

    // Last frame's "held" for each tool control, to find the frame it was pressed or released.
    bool previousCameraLeft = false;
    bool previousCameraRight = false;
    bool previousCameraUp = false;
    bool previousCameraDown = false;

    bool previousZoomIn = false;
    bool previousZoomOut = false;

    bool previousToggleFollow = false;

    bool previousDragCamera = false;

private:
    void updateButton(ButtonState& button, bool currentDown, bool& previousDown);
};