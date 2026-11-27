#include "GlfwInput.hpp"

void GlfwInput::attachWindow(GLFWwindow* newWindow) {
    window = newWindow;
    glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, GlfwInput::scrollCallback);

    glfwGetCursorPos(window, &state.mouseX, &state.mouseY);
}

// Clears the per-frame deltas (mouse motion, wheel). The button edges are recomputed by poll().
void GlfwInput::beginFrame() {
    state.scrollDeltaX = 0.0;
    state.scrollDeltaY = 0.0;
    state.mouseDeltaX = 0.0;
    state.mouseDeltaY = 0.0;
}

void GlfwInput::updateButton(ButtonState& button, bool currentDown, bool& previousDown) {
    button.down = currentDown;
    button.pressed = currentDown && !previousDown;
    button.released = !currentDown && previousDown;
    previousDown = currentDown;
}

void GlfwInput::poll(bool keepStepEdges) {
    if (!window) {
        return;
    }

    updateButton(state.cameraLeft, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS, previousCameraLeft);
    updateButton(state.cameraRight, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS, previousCameraRight);
    updateButton(state.cameraUp, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS, previousCameraUp);
    updateButton(state.cameraDown, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS, previousCameraDown);

    updateButton(state.zoomIn, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS, previousZoomIn);
    updateButton(state.zoomOut, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS, previousZoomOut);

    updateButton(state.toggleFollow, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS, previousToggleFollow);

    actions_.update(readRaw(), keepStepEdges); // the named actions see the same frame

    updateButton(
        state.dragCamera,
        !mouseBlocked && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS,
        previousDragCamera
    );

    double newMouseX = 0.0;
    double newMouseY = 0.0;
    glfwGetCursorPos(window, &newMouseX, &newMouseY);

    state.mouseDeltaX = newMouseX - state.mouseX;
    state.mouseDeltaY = newMouseY - state.mouseY;

    state.mouseX = newMouseX;
    state.mouseY = newMouseY;
}

RawInput GlfwInput::readRaw() const {
    RawInput raw;
    if (!window) return raw;
    if (!keyboardBlocked) // the UI has the keyboard: every key stays released
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) raw.keys[k] = glfwGetKey(window, k) == GLFW_PRESS;
    // The gamepad is the first joystick that has a standard mapping; none plugged in leaves padConnected false.
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        GLFWgamepadstate pad;
        if (glfwJoystickIsGamepad(jid) && glfwGetGamepadState(jid, &pad)) {
            raw.padConnected = true;
            for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b) raw.padButtons[b] = pad.buttons[b] == GLFW_PRESS;
            for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a) raw.padAxes[a] = pad.axes[a];
            break;
        }
    }
    return raw;
}

const InputState& GlfwInput::getState() const {
    return state;
}

void GlfwInput::scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    auto* input = static_cast<GlfwInput*>(glfwGetWindowUserPointer(window));
    if (!input || input->mouseBlocked) {
        return;
    }

    input->state.scrollDeltaX += xOffset;
    input->state.scrollDeltaY += yOffset;
}