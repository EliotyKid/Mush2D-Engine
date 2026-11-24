#pragma once

#include "engine/platform/InputState.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"

#include <glm/glm.hpp>

// What game objects need from the player, whatever its type. The player is the object tagged "player";
// dynamic_cast<IPlayer*> on it reaches these methods.
struct IPlayer {
    virtual ~IPlayer() = default;
    virtual void applyInput(const InputState& input) = 0;
    virtual void respawnAt(glm::vec2 position) = 0;        // teleport: no velocity, ground or slide carried over
    virtual bool downPressedThisFrame() const = 0;         // levers read it during the step
    virtual void setCheckpoint(glm::vec2 position) = 0;    // Checkpoint touched (Sandbox and Test respawn there)
    virtual void respawnAtCheckpoint() = 0;
};

// The first live object tagged "player", or null.
inline GameObject* findPlayer(Scene2D& scene) {
    const auto players = scene.findObjectsByTag("player");
    return players.empty() ? nullptr : players.front();
}
