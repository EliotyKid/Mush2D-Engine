#pragma once

#include "InputState.hpp"
#include "engine/platform/Actions.hpp"

#include <GLFW/glfw3.h>

class GlfwInput {
public:
    void attachWindow(GLFWwindow* window);
    void beginFrame();
    void poll();
    const InputState& getState() const;
    // The named actions (keyboard and gamepad on the same action). Updated by poll().
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

    bool previousJump = false;
    bool previousDash = false;
    bool previousHit = false;
    bool previousToggleFollow = false;
    bool previousRespawn = false;
    bool previousAttack = false;
    bool previousShoot = false;
    bool previousGuard = false;

    bool previousDragCamera = false;

private:
    void updateButton(ButtonState& button, bool currentDown, bool& previousDown);
};