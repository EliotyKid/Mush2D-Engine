#include "engine/platform/Actions.hpp"

#include <cmath>
#include <cstdlib>
#include <string>
#include <iostream>

namespace {
// Names (GLFW names without the GLFW_KEY_ prefix) that are not a single letter, digit or F-key.
const std::pair<const char*, int> namedKeys[] = {
    {"Space", GLFW_KEY_SPACE}, {"Left", GLFW_KEY_LEFT}, {"Right", GLFW_KEY_RIGHT}, {"Up", GLFW_KEY_UP},
    {"Down", GLFW_KEY_DOWN}, {"Enter", GLFW_KEY_ENTER}, {"Escape", GLFW_KEY_ESCAPE}, {"Tab", GLFW_KEY_TAB},
    {"Backspace", GLFW_KEY_BACKSPACE}, {"LeftShift", GLFW_KEY_LEFT_SHIFT}, {"RightShift", GLFW_KEY_RIGHT_SHIFT},
    {"LeftControl", GLFW_KEY_LEFT_CONTROL}, {"RightControl", GLFW_KEY_RIGHT_CONTROL},
    {"LeftAlt", GLFW_KEY_LEFT_ALT}, {"RightAlt", GLFW_KEY_RIGHT_ALT},
    {"Comma", GLFW_KEY_COMMA}, {"Period", GLFW_KEY_PERIOD}, {"Slash", GLFW_KEY_SLASH},
    {"Semicolon", GLFW_KEY_SEMICOLON}, {"Apostrophe", GLFW_KEY_APOSTROPHE},
    {"PageUp", GLFW_KEY_PAGE_UP}, {"PageDown", GLFW_KEY_PAGE_DOWN}, {"Home", GLFW_KEY_HOME}, {"End", GLFW_KEY_END},
    {"Insert", GLFW_KEY_INSERT}, {"Delete", GLFW_KEY_DELETE},
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
    if (name.size() >= 2 && name.size() <= 3 && name[0] == 'F') { // F1..F12 (F13+ exist in GLFW but are rare)
        const int n = std::atoi(std::string(name.substr(1)).c_str());
        if (n >= 1 && n <= 12 && std::to_string(n) == name.substr(1)) return GLFW_KEY_F1 + (n - 1);
    }
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
    if (warned_.insert(std::string(name)).second) // report each unknown name once
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
