#pragma once

// Prefabs: assets/prefabs/<name>.prefab.json describes an object (type, attributes, sprite, collider,
// animations, components, children) and may inherit from another prefab through "base".
//
//     { "version": 1, "base": "enemy_base", "type": "Slime",
//       "props": { "speed": 2.5 }, "extra": { "lootChance": 0.3 },
//       "sprite": { "atlas": "hero_animals", "frame": "fox_idle 0", "animation": "fox_idle", "layer": 1,
//                   "color": [1, 1, 1, 1], "depth": 0, "additive": false },
//       "collider": { "size": [0.8, 0.8], "offset": [0, 0], "isTrigger": false },
//       "animations": { "idle": "slime_idle", "walk": "slime_walk" },      // -> SpriteAnimator
//       "effects": { "move:landed": ["land_dust"], "state:move:hurt": [{ "blink": 0.8 }] }, // -> EffectTriggers
//       "components": [ { "type": "LightComponent", "props": { ... } } ],
//       "children": [ { "name": "Glow", "prefab": "glow_small", "position": [0, 0.5] },
//                     { "name": "Hitbox", "type": "GameObject", "collider": { "isTrigger": true } } ] }
//
// Inheritance: "props", "extra", "sprite", "collider", "animations", "animationEvents" and "effects" merge
// key by key over the base; "components" merge by "type" and "children" by "name" ("remove": true drops the
// inherited item). A cycle is refused with a warning, and so is a child whose prefab is one of the prefabs it
// sits in (skipped).
//
// APPLICATION ORDER (same block in Reflect.hpp; later wins):
//   construct -> onCreate (the C++ defaults, including a sprite / collider it assigns outright) -> the prefab's
//   values (base chain first) -> the LDtk entity's fields -> onPrefabApplied (the object reacts to its final
//   values) -> live edits.
// Only the keys present in the JSON are applied, after onCreate. There is no "apply every default" loop: the
// sprite and collider setters create them when missing, so such a loop would give every object a collider.
//
// Problems never stop the game: an unknown attribute, a wrong type or a missing base is a warning (file and
// field) on stderr and in warnings(), and the attribute keeps its C++ value.

#include "engine/world/GameObject.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class Scene2D;

// Sets each key of `props` on obj (the GameObject* or Component* as void*, never the derived class). Only
// those keys. Returns one warning per unknown key or bad value ("speed: expected Float, got \"fast\"").
std::vector<std::string> applyProps(void* obj, const std::string& typeName, const nlohmann::json& props);

// Where an object made from an LDtk entity stands on the entity box's base: its position (the box centre) moved
// down by half extra()["ldtkSize"].y. The position itself when it has no ldtkSize (made in code or by
// instantiate alone). Grounded objects use it in onPrefabApplied, as the old factories used bottomOf.
glm::vec2 ldtkBoxBottom(const GameObject& o);

// LDtk identifier -> prefab name: "SlimeFast" -> "slime_fast".
std::string snakeCase(const std::string& identifier);

// A usable prefab name: a plain file stem of letters, digits, '_' and '-'.
bool validPrefabName(const std::string& name);

// Where a prefab file holds an attribute: {"sprite", "color"} for spriteColor, {"collider", "size"} for
// colliderSize, {"props", name} for the rest.
std::pair<std::string, std::string> prefabFieldOf(const std::string& attribute);

// Loads, merges and instantiates prefabs from one folder. Registered as a Scene2D service, so the LevelLoader
// and nested prefab children find it. Not thread-safe.
class PrefabLibrary {
public:
    explicit PrefabLibrary(std::filesystem::path dir = {}) : dir_(std::move(dir)) {}

    const std::filesystem::path& dir() const { return dir_; }
    // <dir>/<name>.prefab.json exists (names are plain file stems: letters, digits, '_' and '-').
    bool has(const std::string& name) const;

    // The final document, base chain merged ("base" removed), cached until clear(). nullptr + error when it
    // cannot be read. After a clear() (hot reload), a file that no longer parses returns the last good
    // version, with the problem in `error`.
    const nlohmann::json* resolved(const std::string& name, std::string& error);

    // Creates the object tree in the scene (order above). overrides = LDtk fields: a field with an attribute's
    // name sets it (and lands in ldtkOverridden), others go to extra(); null fields are skipped. entityIid is
    // set before any value is applied. nullptr (with warnings) when the prefab or its type is unusable.
    GameObject* instantiate(Scene2D& scene, const std::string& name, const nlohmann::json& overrides = {},
                            const std::string& entityIid = {});

    // Forgets the cache (hot reload): files are read again on next use.
    void clear();

    // Warnings of the last instantiate (also printed to stderr).
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    const nlohmann::json* resolveChain(const std::string& name, std::string& error, std::vector<std::string>& chain);
    GameObject* spawn(Scene2D& scene, const nlohmann::json& doc, const std::string& prefab, const std::string& where,
                      int depth, GameObjectId parent, const nlohmann::json& overrides, const std::string& entityIid,
                      std::vector<std::string>& lineage);
    void warn(std::string message);

    std::filesystem::path dir_;
    std::map<std::string, nlohmann::json> cache_;    // std::map: pointers stay valid while it grows
    std::map<std::string, nlohmann::json> lastGood_; // moved here by clear()
    std::vector<std::string> warnings_;
};
