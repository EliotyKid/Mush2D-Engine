#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/GameObject.hpp"

// Toggles the owner's visibility `rate` times a second for a while (hidden first), then restores the
// visibility it had when the blink began. A hidden object stays hidden.
class Blink : public Component {
    MUSH2D_COMPONENT(Blink)
public:
    MUSH2D_EXPORT(float, rate, 16.0f)

    // Restarts the time; a blink already running keeps the visibility it saved.
    void start(float seconds) {
        if (!active()) saved_ = false;
        remaining_ = seconds;
        elapsed_ = 0.0f;
    }
    bool active() const { return remaining_ > 0.0f; }

    void onStep(GameObject& owner, Scene2D&, float dt) override {
        if (!active()) return;
        if (!saved_) wasVisible_ = owner.visible, saved_ = true;
        elapsed_ += dt;
        remaining_ -= dt;
        owner.visible = active() ? wasVisible_ && int(elapsed_ * rate) % 2 == 1 : wasVisible_;
    }
    void onDetach(GameObject& owner, Scene2D&) override {
        if (active() && saved_) owner.visible = wasVisible_;
    }

private:
    float remaining_ = 0.0f, elapsed_ = 0.0f;
    bool wasVisible_ = true, saved_ = false;
};
