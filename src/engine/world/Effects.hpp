#pragma once

// Reusable effects: assets/effects/<name>.effect.json holds a list of actions, used by name from any prefab.
//
//     {"version": 1, "actions": [
//       {"particles": {"count": 6, "speed": [1, 3], "angle": [100, 170], ...}},   // ParticleConfig (Particles.hpp)
//       {"squash": [1.3, 0.7], "scaleByStrength": true},                          // SquashStretch::kick
//       {"blink": 0.8},                                                           // Blink::start(seconds)
//       {"effect": "other_effect"},                                               // includes another effect
//       {"squash": [0.8, 1.25], "delay": 0.033}]}                                 // any action, later
//
// A prefab's "effects" section maps triggers to lists of items (an effect name or an inline action):
//     "effects": { "state:move:wall": ["ice_stake", {"squash": [1.15, 0.9]}], "anim:step": ["run_dust"],
//                  "move:landed": ["land_dust"] }
// The EffectTriggers component listens to StateChanged, AnimationEvent and MovementCue for its owner and
// creates the ParticleEmitter, SquashStretch and Blink it needs. Problems are warnings on stderr, once each.

#include "engine/world/Component.hpp"
#include "engine/world/EventBus.hpp"
#include "engine/world/GameObject.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// Emitted on scene.events() by every machine registered with GameObject::registerStateMachine (start included,
// from = ""). Trigger key "state:<label>:<to>".
struct StateChanged {
    GameObjectId object;
    std::string label, from, to;
};

// A movement worth juice ("landed", "jumped", ...). Trigger key "move:<name>". No position = the owner's feet.
struct MovementCue {
    GameObjectId object;
    std::string name;
    float strength = 1.0f;
    std::optional<glm::vec2> position;
    int direction = 1; // -1 = the object faces left (flipX)
};

// Emits a MovementCue for obj, facing where obj faces.
void emitMovementCue(Scene2D& scene, const GameObject& obj, std::string name, float strength = 1.0f,
                     std::optional<glm::vec2> pos = {});

struct Effect {
    std::vector<nlohmann::json> actions;
};

// <dir>/<name>.effect.json by name. Registered as a Scene2D service. Not thread-safe.
class EffectLibrary {
public:
    // Loads (reloads) every *.effect.json in dir.
    void scan(const std::filesystem::path& dir);
    // nullptr for an unknown name (warns once per name).
    const Effect* find(const std::string& name);
    // An invalid file keeps the previous version and warns; an unknown action warns and is dropped.
    void reload(const std::filesystem::path& file);

private:
    std::map<std::string, Effect> effects_;
    std::set<std::string> warned_;
};

// Plays `item` (an effect name or one inline action) on obj: strength scales "scaleByStrength" squashes,
// pos is where particles start, direction mirrors them. Effects come from the EffectLibrary service.
// depth: effect nesting so far (a delayed action keeps its own); deeper than 8 is skipped with a warning.
void playEffect(Scene2D& scene, GameObject& obj, const nlohmann::json& item, float strength, glm::vec2 pos,
                int direction, int depth = 0);

// Empty when `item` is playable (an effect name, or an object with one known action and modifiers of the
// right type: "delay" a number >= 0, "scaleByStrength" true or false); otherwise why not.
std::string effectItemError(const nlohmann::json& item);

// Where an effect lands when its trigger has no position: the bottom of the collider, else the position.
glm::vec2 effectFeet(const GameObject& obj);

// Prefab "effects": trigger key -> items, played when the owner's trigger fires.
class EffectTriggers : public Component {
    MUSH2D_COMPONENT(EffectTriggers)
public:
    std::map<std::string, std::vector<nlohmann::json>> triggers;

    // Fires a trigger by key (also what the events do).
    void fire(GameObject& owner, Scene2D& scene, const std::string& key, float strength,
              std::optional<glm::vec2> pos, int direction);
    // An action with "delay": played by onStep once `delay` seconds have passed.
    void queue(nlohmann::json action, float delay, float strength, glm::vec2 pos, int direction, int depth = 0);

    void onAttach(GameObject& owner, Scene2D& scene) override;
    void onStep(GameObject& owner, Scene2D& scene, float dt) override;
    void onDetach(GameObject&, Scene2D&) override { subscriptions_.clear(); }

private:
    struct Delayed {
        float remaining;
        nlohmann::json action;
        float strength;
        glm::vec2 pos;
        int direction;
        int depth;
    };
    std::vector<EventBus::Subscription> subscriptions_;
    std::vector<Delayed> delayed_;
};
