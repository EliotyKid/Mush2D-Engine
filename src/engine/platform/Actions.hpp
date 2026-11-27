#pragma once

#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Input actions: the game names what the player can do ("jump"), the project.json says which keys and gamepad
// controls trigger it. The engine never knows the names; it only reads this data.

// The controls of one action. Any of them held makes the action held.
struct ActionBinding {
    std::vector<int> keys;                // GLFW key codes
    std::vector<int> buttons;             // GLFW gamepad button codes
    std::vector<std::pair<int, int>> axes; // (GLFW gamepad axis, sign +1 or -1): held when axis * sign passes the limit
    float buffer = 0.0f;                  // seconds a press stays valid for the game (a jump buffer, for example)
};

// Engine-wide gamepad limits (defaults here; a project.json can change them).
struct InputSettings {
    float triggerThreshold = 0.5f; // how far a trigger (L2/R2) must go to count as pressed
    float stickDeadzone = 0.25f;   // how far a stick must go to count (also its "direction buttons")
};

// A raw snapshot of the devices for one frame. The only thing ActionMap reads, so tests can fake it.
struct RawInput {
    std::array<bool, GLFW_KEY_LAST + 1> keys{};
    bool padConnected = false;
    std::array<bool, GLFW_GAMEPAD_BUTTON_LAST + 1> padButtons{};
    std::array<float, GLFW_GAMEPAD_AXIS_LAST + 1> padAxes{};
};

// The state of one action in one frame.
struct ActionState {
    bool down = false;     // held right now
    bool pressed = false;  // went from up to down this frame
    bool released = false; // went from down to up this frame
};

// What every action did in one frame. Read it with get/down/pressed/released.
class ActionFrame {
public:
    enum class Device { Keyboard, Gamepad };

    // The state of `name`. An unknown name gives a stopped state and warns once per name (only in a frame an
    // ActionMap filled: a hand-built frame, or the empty one before the first input, stays silent).
    const ActionState& get(std::string_view name) const;
    // Sets one action's state by hand (tests, or a game that fakes input).
    void set(std::string_view name, const ActionState& state) { states_[std::string(name)] = state; }
    bool down(std::string_view name) const { return get(name).down; }
    bool pressed(std::string_view name) const { return get(name).pressed; }
    bool released(std::string_view name) const { return get(name).released; }

    // The left stick after the dead zone: x to the right, y up, each from -1 to 1.
    glm::vec2 stick() const { return stick_; }
    // The device behind the latest press (for showing the right button icons).
    Device lastDevice() const { return lastDevice_; }

    // Adds `earlier`'s presses and releases to this frame, so a press made in a frame the scene froze
    // (hit stop) is still there when a step reads it. A press and release inside the freeze plays as a one-frame tap.
    void keepEdges(const ActionFrame& earlier);

private:
    friend class ActionMap;
    std::map<std::string, ActionState, std::less<>> states_;
    glm::vec2 stick_{0.0f};
    Device lastDevice_ = Device::Keyboard;
    bool fromMap_ = false; // filled by an ActionMap: unknown names are a mistake worth a warning
    // Unknown names already reported. Shared by every copy of this frame, so a copy never warns again.
    std::shared_ptr<std::set<std::string, std::less<>>> warned_ = std::make_shared<std::set<std::string, std::less<>>>();
};

// Turns raw devices into actions, one update per frame.
class ActionMap {
public:
    ActionMap() { frame_.fromMap_ = true; } // its frame lists every declared action: other names warn
    // Sets the bindings and limits. Actions that keep their name keep their current state.
    void configure(const std::map<std::string, ActionBinding, std::less<>>& actions, const InputSettings& settings);
    // Reads one raw snapshot and computes the new frame (edges come from the change of the combined "down").
    void update(const RawInput& raw);
    const ActionFrame& frame() const { return frame_; }
    // The press buffer of `name` in seconds (0 for an unknown name).
    float bufferWindow(std::string_view name) const;

    // Live values for the F1 panel to edit.
    std::map<std::string, ActionBinding, std::less<>>& bindings() { return bindings_; }
    InputSettings& settings() { return settings_; }

private:
    std::map<std::string, ActionBinding, std::less<>> bindings_;
    InputSettings settings_;
    ActionFrame frame_;
};

// Name tables for project.json. They return -1 (or false) for an unknown name.
int keyByName(std::string_view name);           // "Z", "Left", "Space", "LeftShift"...
int gamepadButtonByName(std::string_view name); // "A", "R1", "DpadDown"...
// "L2", "R2", "LeftStickLeft"... : the axis and the sign (+1/-1) that count as pressed.
bool gamepadAxisByName(std::string_view name, int& axis, int& sign);
