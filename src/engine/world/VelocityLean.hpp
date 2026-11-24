#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/GameObject.hpp"

#include <algorithm>
#include <cmath>

// Tilts the owner a little with its horizontal speed, measured from how far its world position moved:
// transform.rotation eases to -sign(vx) * maxDegrees * min(|vx| / fullSpeed, 1) (running right leans right).
// SpriteBatch turns the sprite around the object's origin (the feet).
class VelocityLean : public Component {
    MUSH2D_COMPONENT(VelocityLean)
public:
    MUSH2D_EXPORT(float, maxDegrees, 4.0f)
    MUSH2D_EXPORT(float, fullSpeed, 5.0f)

    void onStep(GameObject& owner, Scene2D&, float dt) override {
        const float x = owner.world().position.x;
        if (dt <= 0.0f) return;
        const float vx = hasLast_ ? (x - lastX_) / dt : 0.0f;
        lastX_ = x;
        hasLast_ = true;
        const float amount = fullSpeed > 0.0f ? std::min(std::abs(vx) / fullSpeed, 1.0f) : 1.0f;
        const float target = vx == 0.0f ? 0.0f : -std::copysign(glm::radians(maxDegrees) * amount, vx);
        owner.transform.rotation += (target - owner.transform.rotation) * std::min(1.0f, Smoothing * dt);
    }

private:
    static constexpr float Smoothing = 12.0f; // 1 / seconds: ~0.25 s to settle
    float lastX_ = 0.0f;
    bool hasLast_ = false;
};
