#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Ldtk.hpp"

#include <string>

#include <glm/glm.hpp>

enum class Persistence { None, Permanent, Respawn };

// Enemy field "respawn" (bool) wins: true -> Respawn, false -> Permanent. Else "persistence":
// "permanent" | "respawn" | "none". Missing or unknown -> None.
Persistence parsePersistence(const LdtkEntity&);
// Field "respawnDistance" (world units), 12 when missing.
float parseRespawnDistance(const LdtkEntity&);

// Attached by factories to objects made from an LDtk entity; the game's removeTracked records it on removal.
struct PersistentComponent : Component {
    std::string iid;
    Persistence policy = Persistence::None;
    float respawnDistance = 12;
    glm::vec2 spawnPos{0.0f}; // where the factory placed it (respawn distance is measured from here)
};

// Call from a factory, after createObject. No-op for entities without an iid.
inline void trackEntity(GameObject& o, const LdtkEntity& e, glm::vec2 spawnPos) {
    if (e.iid.empty()) return;
    PersistentComponent& p = o.addComponent<PersistentComponent>();
    p.iid = e.iid;
    p.policy = parsePersistence(e);
    p.respawnDistance = parseRespawnDistance(e);
    p.spawnPos = spawnPos;
}
