#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/ObjectData.hpp"

#include <glm/glm.hpp>

// Squash and stretch on two independent springs (x, y) that settle back to {1, 1}. onStep moves the springs;
// apply() scales a sprite already sized for this frame (call it after SpriteAnimator wrote drawSize and
// offset), keeping the sprite's bottom where it was. transform.scale is never touched.
class SquashStretch : public Component {
    MUSH2D_COMPONENT(SquashStretch)
public:
    MUSH2D_EXPORT(float, stiffness, 300.0f)
    MUSH2D_EXPORT(float, damping, 18.0f)

    // Jumps to `scale` (e.g. {0.8, 1.25} stretches) and springs back from there.
    void kick(glm::vec2 scale) { scale_ = scale; velocity_ = glm::vec2(0.0f); }
    void apply(SpriteData& sprite) const;
    glm::vec2 current() const { return scale_; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    glm::vec2 scale_{1.0f}, velocity_{0.0f};
};
