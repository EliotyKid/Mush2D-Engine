#include "PlayerStateSystem.hpp"

#include <glm/glm.hpp>

#include <cmath>

void PlayerStateSystem::update(Scene2D& scene, GameObjectId playerId, float deltaTime) {
    auto smIt = scene.playerStateMachines.find(playerId);
    if (smIt == scene.playerStateMachines.end()) {
        return;
    }

    auto playerIt = scene.players.find(playerId);
    if (playerIt == scene.players.end()) {
        return;
    }

    PlayerStateMachineComponent& sm = smIt->second;
    PlayerComponent& player = playerIt->second;

    sm.stateChangedThisFrame = false;
    sm.stateTime += deltaTime;

    PlayerStateId desiredState =
        glm::length(player.velocity) > 0.001f
        ? PlayerStateId::Walk
        : PlayerStateId::Idle;

    if (desiredState != sm.currentState) {
        onLeave(scene, playerId, sm.currentState);
        changeState(sm, desiredState);
        onEnter(scene, playerId, sm.currentState);
    }

    onStep(scene, playerId, sm.currentState, deltaTime);
}

void PlayerStateSystem::changeState(
    PlayerStateMachineComponent& sm,
    PlayerStateId newState
) {
    sm.previousState = sm.currentState;
    sm.hasPreviousState = true;

    sm.currentState = newState;
    sm.stateTime = 0.0f;
    sm.stateChangedThisFrame = true;
    sm.history.push_back(newState);
}

void PlayerStateSystem::onEnter(Scene2D& scene, GameObjectId playerId, PlayerStateId state) {
    auto spriteIt = scene.sprites.find(playerId);
    if (spriteIt == scene.sprites.end()) {
        return;
    }

    SpriteComponent& sprite = spriteIt->second;

    switch (state) {
        case PlayerStateId::Idle:
            sprite.color = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
            break;

        case PlayerStateId::Walk:
            sprite.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            break;
    }
}

void PlayerStateSystem::onStep(Scene2D& scene, GameObjectId playerId, PlayerStateId state, float) {
    auto object = scene.findObject(playerId);
    if (!object) {
        return;
    }

    auto playerIt = scene.players.find(playerId);
    if (playerIt == scene.players.end()) {
        return;
    }

    PlayerComponent& player = playerIt->second;

    switch (state) {
        case PlayerStateId::Idle:
            break;

        case PlayerStateId::Walk:
            if (player.velocity.x < 0.0f) {
                object->transform.scale.x = -std::abs(object->transform.scale.x);
            } else if (player.velocity.x > 0.0f) {
                object->transform.scale.x = std::abs(object->transform.scale.x);
            }
            break;
    }
}

void PlayerStateSystem::onLeave(Scene2D& scene, GameObjectId playerId, PlayerStateId) {
    auto spriteIt = scene.sprites.find(playerId);
    if (spriteIt == scene.sprites.end()) {
        return;
    }

    // Sem ação específica por enquanto.
}