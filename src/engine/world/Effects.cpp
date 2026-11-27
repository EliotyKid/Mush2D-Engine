#include "engine/world/Effects.hpp"

#include "engine/audio/Audio.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/world/Blink.hpp"
#include "engine/world/Particles.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"
#include "engine/world/SquashStretch.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>

using json = nlohmann::json;

namespace {

constexpr int MaxEffectDepth = 8; // effects including effects: also stops a cycle
constexpr const char* Suffix = ".effect.json";

void warnOnce(const std::string& message) {
    static std::set<std::string> warned;
    if (warned.insert(message).second) std::cerr << "Effects: " << message << '\n';
}

template<typename T>
T& componentOf(GameObject& o) {
    if (T* c = o.getComponent<T>()) return *c;
    return o.addComponent<T>();
}

struct Play {
    Scene2D& scene;
    GameObject& obj;
    float strength;
    glm::vec2 pos;
    int direction;
};

void playItem(const Play& p, const json& item, int depth);

// "scaleByStrength": true multiplies by the strength (not limited to 1); otherwise 1.
float byStrength(const Play& p, const json& action) {
    return action.contains("scaleByStrength") && action["scaleByStrength"] == true ? std::max(p.strength, 0.0f) : 1.0f;
}

// Value checks of the actions whose value effectItemError validates (at load and before playing); empty = fine.
std::string valueError(const std::string& key, const json& v) {
    auto nonNegative = [](const json& j, const char* field) { return j.contains(field) && j[field].is_number() && j[field].get<float>() >= 0.0f; };
    auto unknownKey = [&](const char* a, const char* b) -> std::string {
        for (const auto& [k, unused] : v.items())
            if (k != a && k != b) return key + ": unknown key '" + k + "'";
        return {};
    };
    if (key == "hitstop" && !(v.is_number() && v.get<float>() >= 0.0f)) return "hitstop: expected seconds (a number >= 0)";
    if (key == "shake") {
        if (!v.is_object() || !nonNegative(v, "amplitude") || !nonNegative(v, "duration"))
            return "shake: expected {\"amplitude\": units, \"duration\": seconds} (numbers >= 0)";
        return unknownKey("amplitude", "duration");
    }
    if (key == "flash") {
        const bool color = v.is_object() && v.contains("color") && v["color"].is_array() && v["color"].size() == 3 &&
                           std::all_of(v["color"].begin(), v["color"].end(), [](const json& c) { return c.is_number(); });
        if (!color || !nonNegative(v, "duration"))
            return "flash: expected {\"color\": [r, g, b], \"duration\": seconds}";
        return unknownKey("color", "duration");
    }
    if (key == "sound" && !v.is_string()) return "sound: expected a sound name";
    return {};
}

// "sound" keeps its options beside it: {"sound": "name", "volume": v, "pitch": [min, max]}.
bool isSoundOption(const std::string& key, const json& item) { return (key == "volume" || key == "pitch") && item.contains("sound"); }

std::string soundOptionError(const json& item) {
    if (!item.contains("sound")) return {};
    if (item.contains("volume") && !(item["volume"].is_number() && item["volume"].get<float>() >= 0.0f))
        return "sound: volume expected a number >= 0";
    if (item.contains("pitch")) {
        const json& p = item["pitch"];
        if (!(p.is_array() && p.size() == 2 && p[0].is_number() && p[1].is_number() && p[0].get<float>() > 0.0f &&
              p[0].get<float>() <= p[1].get<float>()))
            return "sound: pitch expected [min, max] (0 < min <= max)";
    }
    return {};
}

// Action name -> what it does with its value.
using Action = std::function<void(const Play&, const json& action, const json& value, int depth)>;
const std::map<std::string, Action>& actions() {
    static const std::map<std::string, Action> table = {
        {"particles", [](const Play& p, const json&, const json& v, int) {
            ParticleConfig config;
            std::string error;
            if (!parseParticleConfig(v, config, error)) return warnOnce(error);
            componentOf<ParticleEmitter>(p.obj).burst(config, p.pos, p.direction);
        }},
        {"squash", [](const Play& p, const json& a, const json& v, int) {
            if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number())
                return warnOnce("squash: expected [x, y]");
            glm::vec2 scale{v[0].get<float>(), v[1].get<float>()};
            if (a.contains("scaleByStrength") && a["scaleByStrength"] == true) scale = 1.0f + (scale - 1.0f) * std::clamp(p.strength, 0.0f, 1.0f);
            componentOf<SquashStretch>(p.obj).kick(scale);
        }},
        {"blink", [](const Play& p, const json& a, const json& v, int) {
            if (!v.is_number()) return warnOnce("blink: expected seconds");
            componentOf<Blink>(p.obj).start(v.get<float>() * byStrength(p, a));
        }},
        {"effect", [](const Play& p, const json&, const json& v, int depth) {
            if (!v.is_string()) return warnOnce("effect: expected an effect name");
            playItem(p, v, depth);
        }},
        {"hitstop", [](const Play& p, const json& a, const json& v, int) { p.scene.hitstop(v.get<float>() * byStrength(p, a)); }},
        {"shake", [](const Play& p, const json& a, const json& v, int) {
            Camera2D* camera = p.scene.service<Camera2D>();
            if (!camera) return warnOnce("no Camera2D service for a shake");
            const float k = byStrength(p, a);
            camera->addShake(v["amplitude"].get<float>() * k, v["duration"].get<float>() * k);
        }},
        {"flash", [](const Play& p, const json& a, const json& v, int) {
            const json& c = v["color"];
            const float seconds = v["duration"].get<float>() * byStrength(p, a);
            if (seconds > 0.0f) // 0 (or strength 0) is no flash at all
                componentOf<Flash>(p.obj).start({c[0].get<float>(), c[1].get<float>(), c[2].get<float>()}, seconds, p.obj);
        }},
        {"sound", [](const Play& p, const json& a, const json& v, int) {
            Audio* audio = p.scene.service<Audio>();
            if (!audio) return; // no audio (tests, tools): silent, as without a device
            float pitch = 1.0f;
            if (a.contains("pitch")) {
                static std::minstd_rand rng{12345};
                pitch = std::uniform_real_distribution<float>(a["pitch"][0].get<float>(), a["pitch"][1].get<float>())(rng);
            }
            audio->playSound(v.get<std::string>(), a.value("volume", 1.0f), pitch); // global: no position
        }},
    };
    return table;
}

bool isModifier(const std::string& key) { return key == "scaleByStrength" || key == "delay"; }

} // namespace

std::string effectItemError(const json& item) {
    if (item.is_string()) return {};
    if (!item.is_object()) return "expected an effect name or an action, got " + item.dump();
    if (item.contains("delay") && (!item["delay"].is_number() || item["delay"].get<float>() < 0.0f))
        return "delay: expected seconds (a number >= 0)";
    if (item.contains("scaleByStrength") && !item["scaleByStrength"].is_boolean())
        return "scaleByStrength: expected true or false";
    bool hasAction = false;
    for (const auto& [key, value] : item.items()) {
        if (isModifier(key) || isSoundOption(key, item)) continue;
        if (!actions().count(key)) return "unknown action '" + key + "'";
        if (std::string error = valueError(key, value); !error.empty()) return error;
        hasAction = true;
    }
    if (std::string error = soundOptionError(item); !error.empty()) return error;
    return hasAction ? std::string() : "no action in " + item.dump();
}

namespace {

// Effect name: its actions one level deeper. Object: one action, now or after "delay" seconds.
void playItem(const Play& p, const json& item, int depth) {
    if (item.is_string()) {
        const std::string name = item.get<std::string>();
        if (depth >= MaxEffectDepth)
            return warnOnce("effect '" + name + "' nested deeper than " + std::to_string(MaxEffectDepth) + " (a cycle?), skipped");
        EffectLibrary* library = p.scene.service<EffectLibrary>();
        const Effect* effect = library ? library->find(name) : nullptr;
        if (!library) warnOnce("no EffectLibrary service for effect '" + name + "'");
        if (effect)
            for (const json& action : effect->actions) playItem(p, action, depth + 1);
        return;
    }
    if (const std::string error = effectItemError(item); !error.empty()) return warnOnce(error + ", ignored");
    if (const float delay = item.value("delay", 0.0f); delay > 0.0f) { // the depth goes along: a delayed self-include stops too
        json now = item;
        now.erase("delay");
        return componentOf<EffectTriggers>(p.obj).queue(std::move(now), delay, p.strength, p.pos, p.direction, depth);
    }
    for (const auto& [key, value] : item.items())
        if (!isModifier(key) && !isSoundOption(key, item)) actions().at(key)(p, item, value, depth);
}

} // namespace

void emitMovementCue(Scene2D& scene, const GameObject& obj, std::string name, float strength, std::optional<glm::vec2> pos) {
    scene.events().emit(MovementCue{obj.id, std::move(name), strength, pos, obj.flipX ? -1 : 1});
}

void EffectLibrary::scan(const std::filesystem::path& dir) {
    std::map<std::string, Effect> old = std::move(effects_);
    effects_.clear(); // a removed file disappears; reload keeps the last good version of an invalid one
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        const std::string fileName = entry.path().filename().string();
        if (!fileName.ends_with(Suffix)) continue;
        const std::string name = fileName.substr(0, fileName.size() - std::string(Suffix).size());
        if (auto it = old.find(name); it != old.end()) effects_[name] = std::move(it->second);
        reload(entry.path());
    }
    if (ec) { // the folder cannot be listed (gone, locked by a sync): an empty map would drop all the juice
        effects_ = std::move(old);
        std::cerr << "Effects: cannot list " << dir.string() << ": " << ec.message() << " (kept the effects)\n";
    }
}

const Effect* EffectLibrary::find(const std::string& name) {
    if (auto it = effects_.find(name); it != effects_.end()) return &it->second;
    if (warned_.insert(name).second) std::cerr << "Effects: unknown effect '" << name << "'\n";
    return nullptr;
}

void EffectLibrary::reload(const std::filesystem::path& file) {
    const std::string fileName = file.filename().string();
    const std::string name = fileName.substr(0, fileName.size() - std::min(fileName.size(), std::string(Suffix).size()));
    auto fail = [&](const std::string& why) { std::cerr << "Effects: " << fileName << ": " << why << " (kept the last good version)\n"; };
    std::ifstream in(file);
    if (!in) return fail("cannot be read");
    const json doc = json::parse(in, nullptr, false);
    if (doc.is_discarded()) return fail("invalid JSON");
    if (!doc.is_object() || !doc.contains("actions") || !doc["actions"].is_array()) return fail("expected {\"actions\": [...]}");
    if (doc.contains("version") && doc["version"] != 1) return fail("unsupported version " + doc["version"].dump());
    Effect effect;
    for (const json& action : doc["actions"]) {
        std::string error = action.is_object() ? effectItemError(action) : "expected an action, got " + action.dump();
        if (ParticleConfig unused; error.empty() && action.contains("particles"))
            parseParticleConfig(action["particles"], unused, error); // a typo is found now, not at the first trigger
        if (error.empty()) effect.actions.push_back(action);
        else std::cerr << "Effects: " << fileName << ": " << error << ", action ignored\n";
    }
    effects_[name] = std::move(effect);
    warned_.erase(name);
}

void playEffect(Scene2D& scene, GameObject& obj, const json& item, float strength, glm::vec2 pos, int direction, int depth) {
    playItem({scene, obj, strength, pos, direction}, item, depth);
}

glm::vec2 effectFeet(const GameObject& obj) {
    const glm::vec2 p = obj.world().position;
    if (!obj.collider) return p;
    return p + obj.collider->offset - glm::vec2(0.0f, 0.5f * obj.collider->size.y);
}

void EffectTriggers::trigger(GameObject& owner, Scene2D& scene, const std::string& key, float strength,
                          std::optional<glm::vec2> pos, int direction) {
    auto it = triggers.find(key);
    if (it == triggers.end()) return;
    const glm::vec2 at = pos ? *pos : effectFeet(owner);
    for (const json& item : it->second) playEffect(scene, owner, item, strength, at, direction);
}

void EffectTriggers::queue(json action, float delay, float strength, glm::vec2 pos, int direction, int depth) {
    delayed_.push_back({delay, std::move(action), strength, pos, direction, depth});
}

void EffectTriggers::onAttach(GameObject& owner, Scene2D& scene) {
    const GameObjectId id = owner.id;
    Scene2D* s = &scene;
    // Handlers find the owner again: events are delivered later, at a flush.
    auto run = [this, id, s](const std::string& key, float strength, std::optional<glm::vec2> pos, std::optional<int> dir) {
        GameObject* o = s->findObject(id);
        if (o && enabled && !removing()) trigger(*o, *s, key, strength, pos, dir ? *dir : (o->flipX ? -1 : 1));
    };
    subscriptions_.push_back(scene.events().subscribe<StateChanged>([run, id](const StateChanged& e) {
        if (e.object == id) run("state:" + e.label + ":" + e.to, 1.0f, {}, {});
    }));
    subscriptions_.push_back(scene.events().subscribe<AnimationEvent>([run, id](const AnimationEvent& e) {
        if (e.object == id) run("anim:" + e.name, 1.0f, {}, {});
    }));
    subscriptions_.push_back(scene.events().subscribe<MovementCue>([run, id](const MovementCue& e) {
        if (e.object == id) run("move:" + e.name, e.strength, e.position, e.direction);
    }));
}

void EffectTriggers::onStep(GameObject& owner, Scene2D& scene, float dt) {
    if (delayed_.empty()) return;
    std::vector<Delayed> due; // played after the loop: an action may queue more
    for (Delayed& d : delayed_)
        if ((d.remaining -= dt) <= 0.0f) due.push_back(std::move(d));
    std::erase_if(delayed_, [](const Delayed& d) { return d.remaining <= 0.0f; });
    for (const Delayed& d : due) playEffect(scene, owner, d.action, d.strength, d.pos, d.direction, d.depth);
}
