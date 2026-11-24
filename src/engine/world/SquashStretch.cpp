#include "engine/world/SquashStretch.hpp"

#include <cmath>

void SquashStretch::apply(SpriteData& sprite) const {
    const float height = std::abs(sprite.drawSize.y);
    sprite.drawSize *= scale_;
    sprite.offset.x *= scale_.x;                          // spreads from the feet (the pivot)
    sprite.offset.y += height * (scale_.y - 1.0f) * 0.5f; // the bottom edge stays put
}

void SquashStretch::onStep(GameObject&, Scene2D&, float dt) {
    // Semi-implicit Euler: stable for these springs at frame-sized steps.
    velocity_ += (-stiffness * (scale_ - 1.0f) - damping * velocity_) * dt;
    scale_ += velocity_ * dt;
}
