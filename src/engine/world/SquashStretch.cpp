#include "engine/world/SquashStretch.hpp"

#include "engine/world/GameObject.hpp"

#include <cmath>

void SquashStretch::apply(SpriteData& sprite) {
    // Changed since our last write = resized by someone else (a new frame): that is the new base.
    if (!wrote_ || sprite.drawSize != wroteSize_ || sprite.offset != wroteOffset_) {
        baseSize_ = sprite.drawSize;
        baseOffset_ = sprite.offset;
    }
    sprite.drawSize = baseSize_ * scale_;
    sprite.offset.x = baseOffset_.x * scale_.x;                                       // spreads from the feet (the pivot)
    sprite.offset.y = baseOffset_.y + std::abs(baseSize_.y) * (scale_.y - 1.0f) * 0.5f; // the bottom edge stays put
    wroteSize_ = sprite.drawSize;
    wroteOffset_ = sprite.offset;
    wrote_ = true;
}

void SquashStretch::onStep(GameObject& owner, Scene2D&, float dt) {
    // Semi-implicit Euler: stable for these springs at frame-sized steps.
    velocity_ += (-stiffness * (scale_ - 1.0f) - damping * velocity_) * dt;
    scale_ += velocity_ * dt;
    if (owner.sprite) apply(*owner.sprite);
}
