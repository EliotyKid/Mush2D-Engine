#pragma once

#include "engine/assets/AtlasTypes.hpp"
#include "engine/world/GameObject.hpp"

constexpr float ArtPixelsPerUnit = 32.0f;

// Sizes the sprite from the frame (px / 32, mirrored when flip) and offsets it so the frame pivot
// lands on transform.position + anchorOffset.
inline void placeFrameAtPivot(GameObject& o, const AtlasFrame& f, glm::vec2 anchorOffset, bool flip) {
    if (!o.sprite) return;
    const float sx = flip ? -1.0f : 1.0f;
    o.transform.scale = {sx * float(f.w) / ArtPixelsPerUnit, float(f.h) / ArtPixelsPerUnit};
    o.sprite->offset = anchorOffset +
        glm::vec2(sx * (float(f.w) * 0.5f - float(f.pivotX)), float(f.pivotY) - float(f.h) * 0.5f) / ArtPixelsPerUnit;
}
