#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Ldtk.hpp"

#include <string>

#include <glm/glm.hpp>

MUSH2D_ENUM(Persistence, None, Permanent, Respawn)

// Enemy field "respawn" (bool) wins: true -> Respawn, false -> Permanent. Else "persistence":
// "permanent" | "respawn" | "none". Missing or unknown -> None.
Persistence parsePersistence(const LdtkEntity&);
// Field "respawnDistance" (world units), 12 when missing.
float parseRespawnDistance(const LdtkEntity&);

// Attached to objects made from an LDtk entity (trackEntity); the game's removeTracked records it on removal.
// A prefab may list it with a policy; the entity's fields "respawn" / "persistence" / "respawnDistance" win.
struct PersistentComponent : Component {
    MUSH2D_COMPONENT(PersistentComponent)
    MUSH2D_EXPORT(Persistence, policy, Persistence::None)
    MUSH2D_EXPORT(float, respawnDistance, 12.0f)
    std::string iid;
    glm::vec2 spawnPos{0.0f}; // where the object was placed (respawn distance is measured from here)
};

// Call after the object is created and placed (factories; LevelLoader for prefabs). Reuses a
// PersistentComponent the object already has. No-op for entities without an iid.
inline void trackEntity(GameObject& o, const LdtkEntity& e, glm::vec2 spawnPos) {
    if (e.iid.empty()) return;
    PersistentComponent* p = o.getComponent<PersistentComponent>();
    if (!p) p = &o.addComponent<PersistentComponent>();
    p->iid = e.iid;
    if (const Persistence policy = parsePersistence(e); policy != Persistence::None) p->policy = policy;
    if (e.fields.contains("respawnDistance") && e.fields["respawnDistance"].is_number()) p->respawnDistance = parseRespawnDistance(e);
    p->spawnPos = spawnPos;
}
