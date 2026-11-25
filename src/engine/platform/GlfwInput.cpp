#include "GlfwInput.hpp"

void GlfwInput::attachWindow(GLFWwindow* newWindow) {
    window = newWindow;
    glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, GlfwInput::scrollCallback);

    glfwGetCursorPos(window, &state.mouseX, &state.mouseY);
}

void GlfwInput::beginFrame() {
    state.scrollDeltaX = 0.0;
    state.scrollDeltaY = 0.0;
    state.mouseDeltaX = 0.0;
    state.mouseDeltaY = 0.0;

    state.moveLeft.pressed = false;
    state.moveLeft.released = false;
    state.moveRight.pressed = false;
    state.moveRight.released = false;
    state.moveUp.pressed = false;
    state.moveUp.released = false;
    state.moveDown.pressed = false;
    state.moveDown.released = false;

    state.cameraLeft.pressed = false;
    state.cameraLeft.released = false;
    state.cameraRight.pressed = false;
    state.cameraRight.released = false;
    state.cameraUp.pressed = false;
    state.cameraUp.released = false;
    state.cameraDown.pressed = false;
    state.cameraDown.released = false;

    state.zoomIn.pressed = false;
    state.zoomIn.released = false;
    state.zoomOut.pressed = false;
    state.zoomOut.released = false;

    state.jump.pressed = false;
    state.jump.released = false;
    state.dash.pressed = false;
    state.dash.released = false;
    state.hit.pressed = false;
    state.hit.released = false;
    state.toggleFollow.pressed = false;
    state.toggleFollow.released = false;
    state.respawn.pressed = false;
    state.respawn.released = false;

    state.dragCamera.pressed = false;
    state.dragCamera.released = false;
}

void GlfwInput::updateButton(ButtonState& button, bool currentDown, bool& previousDown) {
    button.down = currentDown;
    button.pressed = currentDown && !previousDown;
    button.released = !currentDown && previousDown;
    previousDown = currentDown;
}

void GlfwInput::poll() {
    if (!window) {
        return;
    }

    updateButton(state.moveLeft, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS, previousMoveLeft);
    updateButton(state.moveRight, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS, previousMoveRight);
    updateButton(state.moveUp, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS, previousMoveUp);
    updateButton(state.moveDown, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS, previousMoveDown);

    updateButton(state.cameraLeft, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS, previousCameraLeft);
    updateButton(state.cameraRight, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS, previousCameraRight);
    updateButton(state.cameraUp, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS, previousCameraUp);
    updateButton(state.cameraDown, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS, previousCameraDown);

    updateButton(state.zoomIn, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS, previousZoomIn);
    updateButton(state.zoomOut, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS, previousZoomOut);

    updateButton(state.jump, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS, previousJump);
    updateButton(state.dash, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS, previousDash);
    updateButton(state.hit, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS, previousHit);
    updateButton(state.toggleFollow, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS, previousToggleFollow);
    updateButton(state.respawn, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS, previousRespawn);
    updateButton(state.attack, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS, previousAttack);
    updateButton(state.shoot, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS, previousShoot);
    updateButton(state.guard, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS, previousGuard);

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