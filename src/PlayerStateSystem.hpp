#pragma once

#include "Scene2D.hpp"

class PlayerStateSystem {
public:
    void update(Scene2D& scene, GameObjectId playerId, float deltaTime);

private:
    void changeState(
        PlayerStateMachineComponent& sm,
        PlayerStateId newState
    );

    void onEnter(Scene2D& scene, GameObjectId playerId, PlayerStateId state);
    void onStep(Scene2D& scene, GameObjectId playerId, PlayerStateId state, float deltaTime);
    void onLeave(Scene2D& scene, GameObjectId playerId, PlayerStateId state);
};