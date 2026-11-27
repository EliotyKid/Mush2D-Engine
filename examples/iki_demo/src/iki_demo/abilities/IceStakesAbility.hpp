#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

#include <string>

// Ice stakes: the Iki clings to IceClimb walls and hangs from IceClimb ceilings (states "wall" and "ceiling").
// The model for every ability: a component on the Iki's prefab that is active once the global WorldState flag
// `unlockFlag` is set (an ItemObject with kind "ability.ice.stakes" sets it). Without it, ice is a plain surface.
class IceStakesAbility : public Component {
    MUSH2D_COMPONENT(IceStakesAbility)
public:
    MUSH2D_EXPORT(std::string, unlockFlag, "ability.ice.stakes")

    // False without the WorldState service.
    bool unlocked(const Scene2D& scene) const {
        const WorldState* state = scene.service<WorldState>();
        return state && state->flag(unlockFlag);
    }
};
