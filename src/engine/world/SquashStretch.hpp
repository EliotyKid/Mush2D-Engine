#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/ObjectData.hpp"

#include <glm/glm.hpp>

// Squash and stretch on two independent springs (x, y) that settle back to {1, 1}. onStep moves the springs
// and applies them to the owner's sprite; SpriteAnimator applies them again right after it places a frame, so
// the order of the components does not matter. apply() scales the sprite's size for this frame, keeping the
// sprite's bottom where it was, and is idempotent: it remembers what it wrote, so calling it again, or on a
// sprite nothing resized since, starts from the unsquashed size. transform.scale is never touched.
class SquashStretch : public Component {
    MUSH2D_COMPONENT(SquashStretch)
public:
    MUSH2D_EXPORT(float, stiffness, 300.0f)
    MUSH2D_EXPORT(float, damping, 18.0f)

    // Jumps to `scale` (e.g. {0.8, 1.25} stretches) and springs back from there.
    void kick(glm::vec2 scale) { scale_ = scale; velocity_ = glm::vec2(0.0f); }
    void apply(SpriteData& sprite);
    glm::vec2 current() const { return scale_; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    glm::vec2 scale_{1.0f}, velocity_{0.0f};
    glm::vec2 baseSize_{0.0f}, baseOffset_{0.0f};       // the sprite before the squash
    glm::vec2 wroteSize_{0.0f}, wroteOffset_{0.0f};     // what apply() left in it
    bool wrote_ = false;
};
