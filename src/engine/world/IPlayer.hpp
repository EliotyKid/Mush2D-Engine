#pragma once

#include "engine/platform/Actions.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"

#include <glm/glm.hpp>

// What game objects need from the player, whatever its type. The player is the object tagged "player";
// dynamic_cast<IPlayer*> on it reaches these methods.
struct IPlayer {
    virtual ~IPlayer() = default;
    virtual void applyInput(const ActionFrame& input) = 0; // this frame's named actions, before the step
    virtual void respawnAt(glm::vec2 position) = 0;        // teleport: no velocity, ground or slide carried over
    virtual bool interactPressed() const = 0;              // the "interact" action was pressed (levers read it in the step)
    virtual void setCheckpoint(glm::vec2 position) = 0;    // Checkpoint touched (Sandbox and Test respawn there)
    virtual void respawnAtCheckpoint() = 0;
};

// The first live object tagged "player", or null.
inline GameObject* findPlayer(Scene2D& scene) {
    const auto players = scene.findObjectsByTag("player");
    return players.empty() ? nullptr : players.front();
}
