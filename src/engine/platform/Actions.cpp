#include "engine/platform/Actions.hpp"

#include <cmath>
#include <cstdlib>
#include <string>
#include <iostream>

namespace {
// Names (GLFW names in CamelCase, without GLFW_KEY_) of every key that is not a letter, digit, F-key or Kp digit.
const std::pair<const char*, int> namedKeys[] = {
    {"Space", GLFW_KEY_SPACE},
    {"Apostrophe", GLFW_KEY_APOSTROPHE},
    {"Comma", GLFW_KEY_COMMA},
    {"Minus", GLFW_KEY_MINUS},
    {"Period", GLFW_KEY_PERIOD},
    {"Slash", GLFW_KEY_SLASH},
    {"Semicolon", GLFW_KEY_SEMICOLON},
    {"Equal", GLFW_KEY_EQUAL},
    {"LeftBracket", GLFW_KEY_LEFT_BRACKET},
    {"Backslash", GLFW_KEY_BACKSLASH},
    {"RightBracket", GLFW_KEY_RIGHT_BRACKET},
    {"GraveAccent", GLFW_KEY_GRAVE_ACCENT},
    {"Escape", GLFW_KEY_ESCAPE},
    {"Enter", GLFW_KEY_ENTER},
    {"Tab", GLFW_KEY_TAB},
    {"Backspace", GLFW_KEY_BACKSPACE},
    {"Insert", GLFW_KEY_INSERT},
    {"Delete", GLFW_KEY_DELETE},
    {"Right", GLFW_KEY_RIGHT},
    {"Left", GLFW_KEY_LEFT},
    {"Down", GLFW_KEY_DOWN},
    {"Up", GLFW_KEY_UP},
    {"PageUp", GLFW_KEY_PAGE_UP},
    {"PageDown", GLFW_KEY_PAGE_DOWN},
    {"Home", GLFW_KEY_HOME},
    {"End", GLFW_KEY_END},
    {"CapsLock", GLFW_KEY_CAPS_LOCK},
    {"ScrollLock", GLFW_KEY_SCROLL_LOCK},
    {"NumLock", GLFW_KEY_NUM_LOCK},
    {"PrintScreen", GLFW_KEY_PRINT_SCREEN},
    {"Pause", GLFW_KEY_PAUSE},
    {"Menu", GLFW_KEY_MENU},
    {"KpDecimal", GLFW_KEY_KP_DECIMAL},
    {"KpDivide", GLFW_KEY_KP_DIVIDE},
    {"KpMultiply", GLFW_KEY_KP_MULTIPLY},
    {"KpSubtract", GLFW_KEY_KP_SUBTRACT},
    {"KpAdd", GLFW_KEY_KP_ADD},
    {"KpEnter", GLFW_KEY_KP_ENTER},
    {"KpEqual", GLFW_KEY_KP_EQUAL},
    {"LeftShift", GLFW_KEY_LEFT_SHIFT},
    {"LeftControl", GLFW_KEY_LEFT_CONTROL},
    {"LeftAlt", GLFW_KEY_LEFT_ALT},
    {"LeftSuper", GLFW_KEY_LEFT_SUPER},
    {"RightShift", GLFW_KEY_RIGHT_SHIFT},
    {"RightControl", GLFW_KEY_RIGHT_CONTROL},
    {"RightAlt", GLFW_KEY_RIGHT_ALT},
    {"RightSuper", GLFW_KEY_RIGHT_SUPER},
};

const std::pair<const char*, int> padButtons[] = {
    {"A", GLFW_GAMEPAD_BUTTON_A}, {"B", GLFW_GAMEPAD_BUTTON_B}, {"X", GLFW_GAMEPAD_BUTTON_X}, {"Y", GLFW_GAMEPAD_BUTTON_Y},
    {"L1", GLFW_GAMEPAD_BUTTON_LEFT_BUMPER}, {"R1", GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER},
    {"Back", GLFW_GAMEPAD_BUTTON_BACK}, {"Start", GLFW_GAMEPAD_BUTTON_START}, {"Guide", GLFW_GAMEPAD_BUTTON_GUIDE},
    {"LeftStick", GLFW_GAMEPAD_BUTTON_LEFT_THUMB}, {"RightStick", GLFW_GAMEPAD_BUTTON_RIGHT_THUMB},
    {"DpadUp", GLFW_GAMEPAD_BUTTON_DPAD_UP}, {"DpadRight", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"DpadDown", GLFW_GAMEPAD_BUTTON_DPAD_DOWN}, {"DpadLeft", GLFW_GAMEPAD_BUTTON_DPAD_LEFT},
};

struct AxisName { const char* name; int axis, sign; };
const AxisName padAxes[] = {
    {"L2", GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, 1}, {"R2", GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, 1},
    {"LeftStickLeft", GLFW_GAMEPAD_AXIS_LEFT_X, -1}, {"LeftStickRight", GLFW_GAMEPAD_AXIS_LEFT_X, 1},
    {"LeftStickUp", GLFW_GAMEPAD_AXIS_LEFT_Y, -1}, {"LeftStickDown", GLFW_GAMEPAD_AXIS_LEFT_Y, 1}, // GLFW: down is +
    {"RightStickLeft", GLFW_GAMEPAD_AXIS_RIGHT_X, -1}, {"RightStickRight", GLFW_GAMEPAD_AXIS_RIGHT_X, 1},
    {"RightStickUp", GLFW_GAMEPAD_AXIS_RIGHT_Y, -1}, {"RightStickDown", GLFW_GAMEPAD_AXIS_RIGHT_Y, 1},
};

bool isTrigger(int axis) { return axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER; }

// A stick axis value with the dead zone applied (inside the zone it is 0).
float deadZoned(float v, float zone) { return std::fabs(v) < zone ? 0.0f : v; }
}

int keyByName(std::string_view name) {
    if (name.size() == 1) {
        const char c = name[0];
        if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
    }
    // Numbered families: F1..F25 and Kp0..Kp9 (the number part must be plain digits).
    auto number = [&](std::string_view prefix, int lo, int hi) {
        if (name.size() <= prefix.size() || name.substr(0, prefix.size()) != prefix) return -1;
        const std::string digits(name.substr(prefix.size()));
        const int n = std::atoi(digits.c_str());
        return n >= lo && n <= hi && std::to_string(n) == digits ? n : -1;
    };
    if (int n = number("F", 1, 25); n >= 0) return GLFW_KEY_F1 + (n - 1);
    if (int n = number("Kp", 0, 9); n >= 0) return GLFW_KEY_KP_0 + n;
    for (const auto& [n, code] : namedKeys)
        if (name == n) return code;
    return -1;
}

int gamepadButtonByName(std::string_view name) {
    for (const auto& [n, code] : padButtons)
        if (name == n) return code;
    return -1;
}

bool gamepadAxisByName(std::string_view name, int& axis, int& sign) {
    for (const AxisName& a : padAxes)
        if (name == a.name) { axis = a.axis; sign = a.sign; return true; }
    return false;
}

const ActionState& ActionFrame::get(std::string_view name) const {
    static const ActionState stopped;
    auto it = states_.find(name);
    if (it != states_.end()) return it->second;
    if (warned_->insert(std::string(name)).second) // report each unknown name once
        std::cerr << "Input: unknown action '" << name << "'\n";
    return stopped;
}

void ActionFrame::keepEdges(const ActionFrame& earlier) {
    for (const auto& [name, e] : earlier.states_) {
        ActionState& s = states_[name];
        s.pressed |= e.pressed;
        s.released |= e.released;
    }
}

void ActionMap::configure(const std::map<std::string, ActionBinding, std::less<>>& actions, const InputSettings& settings) {
    bindings_ = actions;
    settings_ = settings;
    // Keep the state of actions that still exist; drop the others; add new ones stopped.
    std::map<std::string, ActionState, std::less<>> next;
    for (const auto& [name, binding] : bindings_) {
        auto old = frame_.states_.find(name);
        next[name] = old != frame_.states_.end() ? old->second : ActionState{};
    }
    frame_.states_ = std::move(next);
}

void ActionMap::update(const RawInput& raw) {
    for (const auto& [name, b] : bindings_) {
        bool keyDown = false, padDown = false;
        for (int k : b.keys)
            if (k >= 0 && k < int(raw.keys.size()) && raw.keys[k]) keyDown = true;
        if (raw.padConnected) { // a missing pad counts as all released, whatever is left in the arrays
            for (int btn : b.buttons)
                if (btn >= 0 && btn < int(raw.padButtons.size()) && raw.padButtons[btn]) padDown = true;
            for (const auto& [axis, sign] : b.axes) {
                if (axis < 0 || axis >= int(raw.padAxes.size())) continue;
                const float limit = isTrigger(axis) ? settings_.triggerThreshold : settings_.stickDeadzone;
                if (raw.padAxes[axis] * float(sign) > limit) padDown = true;
            }
        }
        ActionState& s = frame_.states_[name];
        const bool wasDown = s.down;
        s.down = keyDown || padDown; // any source holds the action
        s.pressed = s.down && !wasDown; // edges come from the combined state: two sources give one press
        s.released = !s.down && wasDown;
        if (s.pressed) frame_.lastDevice_ = keyDown ? ActionFrame::Device::Keyboard : ActionFrame::Device::Gamepad;
    }
    frame_.stick_ = {0.0f, 0.0f};
    if (raw.padConnected) {
        frame_.stick_.x = deadZoned(raw.padAxes[GLFW_GAMEPAD_AXIS_LEFT_X], settings_.stickDeadzone);
        frame_.stick_.y = -deadZoned(raw.padAxes[GLFW_GAMEPAD_AXIS_LEFT_Y], settings_.stickDeadzone); // GLFW: down is +
    }
}

float ActionMap::bufferWindow(std::string_view name) const {
    auto it = bindings_.find(name);
    return it == bindings_.end() ? 0.0f : it->second.buffer;
}
