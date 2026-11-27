#pragma once

#include "engine/world/CharacterController.hpp"
#include "engine/world/Component.hpp"
#include "engine/world/JumpAssist.hpp"

#include <string>

// Walks and jumps its owner, reading input actions by name. The names are data: a prefab can change them
// (for example "actionJump"), and the project's project.json decides which keys and buttons they mean.
// The owner needs a collider; the scene needs an ActionMap service. Without either, the mover does nothing.
class PlatformerMover : public Component {
    MUSH2D_COMPONENT(PlatformerMover)
public:
    // Which input actions drive the mover.
    MUSH2D_EXPORT(std::string, actionLeft, "moveLeft")
    MUSH2D_EXPORT(std::string, actionRight, "moveRight")
    MUSH2D_EXPORT(std::string, actionJump, "jump")
    MUSH2D_EXPORT(std::string, actionDrop, "dropThrough") // held while jumping on a one-way platform: drop through

    // Feel, in world units and seconds.
    MUSH2D_EXPORT(float, runSpeed, 5.0f)       // top speed on the ground and in the air
    MUSH2D_EXPORT(float, acceleration, 60.0f)  // how fast the speed reaches runSpeed (or 0)
    MUSH2D_EXPORT(float, gravity, -32.0f)
    MUSH2D_EXPORT(float, jumpSpeed, 11.0f)
    MUSH2D_EXPORT(float, jumpCut, 0.5f)        // releasing jump while rising multiplies the speed by this, once
    MUSH2D_EXPORT(float, coyoteTime, 0.1f)     // a jump still works this long after leaving the ground
    MUSH2D_EXPORT(float, maxFallSpeed, -18.0f)

    // Current velocity (read by tests and by games that add animation).
    glm::vec2 velocity() const { return velocity_; }
    bool grounded() const { return last_.grounded; }

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    glm::vec2 velocity_{0.0f};
    Body body_;         // the controller's body (drop-through state lives here)
    MoveResult last_;   // the controller's last result
    JumpAssist jump_;   // coyote time and the jump buffer
    bool cuttable_ = false; // the current rise comes from a jump, so releasing the button cuts it
};
