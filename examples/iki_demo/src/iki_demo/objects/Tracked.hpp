#pragma once

#include "engine/world/PersistentComponent.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

// Kill/collect: records the removal (permanent or respawn policy) and destroys the object.
inline void removeTracked(Scene2D& scene, WorldState& state, GameObject& o, double time) {
    if (const auto* p = o.getComponent<PersistentComponent>(); p && p->policy != Persistence::None) {
        state.markGone(p->iid, p->spawnPos, time);
    }
    scene.destroy(o.id);
}
