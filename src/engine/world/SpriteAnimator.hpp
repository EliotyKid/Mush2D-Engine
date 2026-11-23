#pragma once

#include "engine/assets/AtlasTypes.hpp"
#include "engine/world/Component.hpp"
#include "engine/world/GameObject.hpp"

#include <cstddef>
#include <map>
#include <string>

constexpr float ArtPixelsPerUnit = 32.0f;

// Sizes the sprite from the frame (sprite.drawSize = px / 32, mirrored when flip) and offsets it so the frame
// pivot lands on transform.position + anchorOffset. transform.scale is left alone (it multiplies the size).
inline void placeFrameAtPivot(GameObject& o, const AtlasFrame& f, glm::vec2 anchorOffset, bool flip) {
    if (!o.sprite) return;
    const float sx = flip ? -1.0f : 1.0f;
    o.sprite->drawSize = {sx * float(f.w) / ArtPixelsPerUnit, float(f.h) / ArtPixelsPerUnit};
    o.sprite->offset = anchorOffset +
        glm::vec2(sx * (float(f.w) * 0.5f - float(f.pivotX)), float(f.pivotY) - float(f.h) * 0.5f) / ArtPixelsPerUnit;
}

// Loops the animation of the current state (prefab "animations": state -> animation) on the owner's sprite,
// standing with the frame pivot on transform.position and mirrored by the owner's flipX. An animation is a
// name in the sprite's atlas (spriteAtlas) or an "atlas/animation" ref. Needs the AssetRegistry scene service
// and a sprite on the owner; otherwise (or for an unknown state / animation) it does nothing.
class SpriteAnimator : public Component {
    MUSH2D_COMPONENT(SpriteAnimator)
public:
    MUSH2D_EXPORT(std::string, state, "idle")
    std::map<std::string, std::string> animations;

    // Restarts the animation when the state changes.
    void setState(const std::string& newState) { state = newState; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    std::string playing_; // state shown last; a different `state` restarts at frame 0
    std::size_t frame_ = 0;
    float timer_ = 0.0f;
};
