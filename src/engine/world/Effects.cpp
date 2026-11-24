#include "engine/world/Effects.hpp"

#include "engine/world/Blink.hpp"
#include "engine/world/Particles.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"
#include "engine/world/SquashStretch.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>

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

// Action name -> what it does with its value. The D3 adds sound, shake, hitstop and flash here.
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
            if (a.value("scaleByStrength", false)) scale = 1.0f + (scale - 1.0f) * std::clamp(p.strength, 0.0f, 1.0f);
            componentOf<SquashStretch>(p.obj).kick(scale);
        }},
        {"blink", [](const Play& p, const json&, const json& v, int) {
            if (!v.is_number()) return warnOnce("blink: expected seconds");
            componentOf<Blink>(p.obj).start(v.get<float>());
        }},
        {"effect", [](const Play& p, const json&, const json& v, int depth) {
            if (!v.is_string()) return warnOnce("effect: expected an effect name");
            playItem(p, v, depth);
        }},
    };
    return table;
}

bool isModifier(const std::string& key) { return key == "scaleByStrength" || key == "delay"; }

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
    if (!item.is_object()) return warnOnce("expected an effect name or an action, got " + item.dump());
    if (const float delay = item.value("delay", 0.0f); delay > 0.0f) {
        json now = item;
        now.erase("delay");
        return componentOf<EffectTriggers>(p.obj).queue(std::move(now), delay, p.strength, p.pos, p.direction);
    }
    for (const auto& [key, value] : item.items()) {
        if (isModifier(key)) continue;
        if (auto a = actions().find(key); a != actions().end()) a->second(p, item, value, depth);
        else warnOnce("unknown action '" + key + "', ignored");
    }
}

} // namespace

void emitMovementCue(Scene2D& scene, const GameObject& obj, std::string name, float strength, std::optional<glm::vec2> pos) {
    scene.events().emit(MovementCue{obj.id, std::move(name), strength, pos, obj.flipX ? -1 : 1});
}

void EffectLibrary::scan(const std::filesystem::path& dir) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        if (entry.path().string().ends_with(Suffix)) reload(entry.path());
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
        bool known = action.is_object();
        if (known)
            for (const auto& [key, value] : action.items())
                if (!isModifier(key) && !actions().count(key)) known = false;
        if (known) effect.actions.push_back(action);
        else std::cerr << "Effects: " << fileName << ": unknown action " << action.dump() << ", ignored\n";
    }
    effects_[name] = std::move(effect);
    warned_.erase(name);
}

void playEffect(Scene2D& scene, GameObject& obj, const json& item, float strength, glm::vec2 pos, int direction) {
    playItem({scene, obj, strength, pos, direction}, item, 0);
}

glm::vec2 effectFeet(const GameObject& obj) {
    const glm::vec2 p = obj.world().position;
    if (!obj.collider) return p;
    return p + obj.collider->offset - glm::vec2(0.0f, 0.5f * obj.collider->size.y);
}

void EffectTriggers::fire(GameObject& owner, Scene2D& scene, const std::string& key, float strength,
                          std::optional<glm::vec2> pos, int direction) {
    auto it = triggers.find(key);
    if (it == triggers.end()) return;
    const glm::vec2 at = pos ? *pos : effectFeet(owner);
    for (const json& item : it->second) playEffect(scene, owner, item, strength, at, direction);
}

void EffectTriggers::queue(json action, float delay, float strength, glm::vec2 pos, int direction) {
    delayed_.push_back({delay, std::move(action), strength, pos, direction});
}

void EffectTriggers::onAttach(GameObject& owner, Scene2D& scene) {
    const GameObjectId id = owner.id;
    Scene2D* s = &scene;
    // Handlers find the owner again: events are delivered later, at a flush.
    auto run = [this, id, s](const std::string& key, float strength, std::optional<glm::vec2> pos, std::optional<int> dir) {
        GameObject* o = s->findObject(id);
        if (o && enabled && !removing()) fire(*o, *s, key, strength, pos, dir ? *dir : (o->flipX ? -1 : 1));
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
    for (const Delayed& d : due) playEffect(scene, owner, d.action, d.strength, d.pos, d.direction);
}
