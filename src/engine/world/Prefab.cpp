#include "engine/world/Prefab.hpp"

#include "engine/assets/AssetRegistry.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/SpriteAnimator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <set>

using json = nlohmann::json;

namespace {

constexpr int MaxDepth = 16; // children of children...: also stops a child that instantiates its own parent

const std::set<std::string> KnownKeys = {"version", "base", "type", "props", "extra", "sprite", "collider",
                                         "animations", "components", "children", "name", "position", "prefab", "remove"};
const std::set<std::string> KeyedSections = {"props", "extra", "sprite", "collider", "animations"};

// Prefab section field -> GameObject attribute.
const std::map<std::string, std::string> SpriteFields = {{"atlas", "spriteAtlas"}, {"frame", "spriteFrame"},
    {"layer", "spriteLayer"}, {"color", "spriteColor"}, {"depth", "depth"}, {"additive", "additive"}};
const std::map<std::string, std::string> ColliderFields = {
    {"size", "colliderSize"}, {"offset", "colliderOffset"}, {"isTrigger", "isTrigger"}};

bool removed(const json& item) { return item.contains("remove") && item["remove"] == true; }

json mergePrefab(const json& base, const json& own);

// Lists of objects matched by idKey ("type" or "name"): merged in place, "remove" drops, new ones appended.
json mergeList(const json& base, const json& own, const char* idKey) {
    json out = base.is_array() ? base : json::array();
    if (!own.is_array()) return out;
    for (const json& item : own) {
        if (!item.is_object()) continue;
        const json id = item.contains(idKey) ? item[idKey] : json();
        auto it = std::find_if(out.begin(), out.end(), [&](const json& b) {
            return !id.is_null() && b.is_object() && b.contains(idKey) && b[idKey] == id;
        });
        if (removed(item)) {
            if (it != out.end()) out.erase(it);
        } else if (it != out.end()) {
            *it = mergePrefab(*it, item);
        } else {
            out.push_back(item);
        }
    }
    return out;
}

json mergePrefab(const json& base, const json& own) {
    json out = base;
    for (const auto& [key, value] : own.items()) {
        if (KeyedSections.count(key) && value.is_object() && out.contains(key) && out[key].is_object()) out[key].update(value);
        else if (key == "components") out[key] = mergeList(out.contains(key) ? out[key] : json(), value, "type");
        else if (key == "children") out[key] = mergeList(out.contains(key) ? out[key] : json(), value, "name");
        else out[key] = value;
    }
    return out;
}

// LDtk field value -> the attribute's JSON form.
json fromLdtk(const PropertyInfo& p, const json& v) {
    if (p.type == PropType::EntityRef && v.is_object() && v.contains("entityIid")) return v["entityIid"];
    if (p.type == PropType::Color && v.is_string()) { // "#RRGGBB"
        const std::string s = v.get<std::string>();
        unsigned rgb = 0;
        if (s.size() == 7 && s[0] == '#' && std::all_of(s.begin() + 1, s.end(), [](unsigned char c) { return std::isxdigit(c); }) &&
            std::sscanf(s.c_str() + 1, "%6x", &rgb) == 1)
            return {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, 1.0f};
    }
    return v;
}

const PropertyInfo* findProp(const std::string& typeName, const std::string& name) {
    for (const PropertyInfo* p : typeRegistry().allProps(typeName))
        if (p->name == name) return p;
    return nullptr;
}

} // namespace

std::vector<std::string> applyProps(void* obj, const std::string& typeName, const json& props) {
    std::vector<std::string> warnings;
    if (props.is_null()) return warnings;
    if (!props.is_object()) return {"expected an object of attributes"};
    for (const auto& [key, value] : props.items()) {
        const PropertyInfo* p = findProp(typeName, key);
        std::string error;
        if (!p) warnings.push_back(key + ": unknown attribute of " + typeName);
        else if (!p->set(obj, value, error)) warnings.push_back(key + ": " + error);
    }
    return warnings;
}

glm::vec2 ldtkBoxBottom(const GameObject& o) {
    glm::vec2 size{0.0f};
    if (!o.extra().contains("ldtkSize") || !reflect::vecFromJson(o.extra()["ldtkSize"], size)) return o.transform.position;
    return o.transform.position - glm::vec2(0.0f, 0.5f * size.y);
}

bool validPrefabName(const std::string& name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}

std::pair<std::string, std::string> prefabFieldOf(const std::string& attribute) {
    for (const auto& [section, fields] : {std::pair{"sprite", &SpriteFields}, std::pair{"collider", &ColliderFields}})
        for (const auto& [field, attr] : *fields)
            if (attr == attribute) return {section, field};
    return {"props", attribute};
}

std::string snakeCase(const std::string& identifier) {
    std::string out;
    for (std::size_t i = 0; i < identifier.size(); ++i) {
        const unsigned char c = identifier[i];
        if (std::isupper(c) && i > 0 && (std::islower((unsigned char)identifier[i - 1]) || std::isdigit((unsigned char)identifier[i - 1])))
            out += '_';
        out += char(std::tolower(c));
    }
    return out;
}

bool PrefabLibrary::has(const std::string& name) const {
    std::error_code ec;
    return validPrefabName(name) && std::filesystem::is_regular_file(dir_ / (name + ".prefab.json"), ec);
}

void PrefabLibrary::clear() {
    for (auto& [name, doc] : cache_) lastGood_[name] = std::move(doc);
    cache_.clear();
}

void PrefabLibrary::warn(std::string message) {
    std::fprintf(stderr, "Prefab: %s\n", message.c_str());
    warnings_.push_back(std::move(message));
}

const json* PrefabLibrary::resolved(const std::string& name, std::string& error) {
    std::vector<std::string> chain;
    if (const json* doc = resolveChain(name, error, chain)) return doc;
    if (auto it = lastGood_.find(name); it != lastGood_.end()) {
        error += " (kept the last good version)";
        return &it->second;
    }
    return nullptr;
}

const json* PrefabLibrary::resolveChain(const std::string& name, std::string& error, std::vector<std::string>& chain) {
    if (auto it = cache_.find(name); it != cache_.end()) return &it->second;
    if (!validPrefabName(name)) {
        error = "invalid prefab name '" + name + "' (letters, digits, '_' and '-' only)";
        return nullptr;
    }
    if (std::find(chain.begin(), chain.end(), name) != chain.end()) {
        error = "prefab cycle:";
        for (const std::string& n : chain) error += " " + n + " ->";
        error += " " + name;
        return nullptr;
    }
    const std::string file = name + ".prefab.json";
    std::ifstream in(dir_ / file);
    if (!in) {
        error = file + ": not found in " + dir_.string();
        return nullptr;
    }
    json own;
    try {
        own = json::parse(in);
    } catch (const json::exception& e) {
        error = file + ": invalid JSON: " + e.what();
        return nullptr;
    }
    if (!own.is_object()) {
        error = file + ": expected a JSON object";
        return nullptr;
    }
    if (own.contains("version") && own["version"] != 1) {
        error = file + ": unsupported version " + own["version"].dump();
        return nullptr;
    }
    json doc = own;
    if (own.contains("base")) {
        if (!own["base"].is_string()) {
            error = file + ": base: expected a prefab name";
            return nullptr;
        }
        chain.push_back(name);
        const json* base = resolveChain(own["base"].get<std::string>(), error, chain);
        chain.pop_back();
        if (!base) {
            error = file + ": base: " + error;
            return nullptr;
        }
        doc = mergePrefab(*base, own);
    }
    doc.erase("base");
    return &(cache_[name] = std::move(doc));
}

GameObject* PrefabLibrary::instantiate(Scene2D& scene, const std::string& name, const json& overrides,
                                       const std::string& entityIid) {
    warnings_.clear();
    std::string error;
    const json* doc = resolved(name, error);
    if (!error.empty()) warn(error);
    if (!doc) return nullptr;
    std::vector<std::string> lineage;
    return spawn(scene, *doc, name, name + ".prefab.json", 0, InvalidGameObjectId, overrides, entityIid, lineage);
}

GameObject* PrefabLibrary::spawn(Scene2D& scene, const json& doc, const std::string& prefab, const std::string& where,
                                 int depth, GameObjectId parent, const json& overrides, const std::string& entityIid,
                                 std::vector<std::string>& lineage) {
    if (depth > MaxDepth) {
        warn(where + ": children nested deeper than " + std::to_string(MaxDepth) + " (a prefab containing itself?)");
        return nullptr;
    }
    const std::string typeName = doc.contains("type") && doc["type"].is_string() ? doc["type"].get<std::string>() : "GameObject";
    const TypeInfo* type = typeRegistry().find(typeName);
    if (!type || !type->createObject) {
        warn(where + ": type '" + typeName + "' is not a registered object class with a no-argument constructor");
        return nullptr;
    }
    for (const auto& [key, value] : doc.items())
        if (!KnownKeys.count(key)) warn(where + ": unknown key '" + key + "'");
    for (const char* key : {"components", "children"})
        if (doc.contains(key) && !doc[key].is_array()) warn(where + ": " + key + ": expected a list");
    if (doc.contains("animations") && !doc["animations"].is_object()) warn(where + ": animations: expected an object");

    GameObject& o = scene.addObject(type->createObject()); // construct + onCreate
    o.prefab = prefab;
    o.entityIid = entityIid;
    void* self = static_cast<GameObject*>(&o);
    auto apply = [&](const std::string& section, const json& props) {
        for (const std::string& w : applyProps(self, typeName, props)) warn(where + ": " + section + w);
    };
    // Prefab section {field: value} -> {attribute: value}, only the fields present.
    auto section = [&](const char* key, const std::map<std::string, std::string>& fields) {
        if (!doc.contains(key)) return;
        if (!doc[key].is_object()) return warn(where + ": " + key + ": expected an object");
        json props = json::object();
        for (const auto& [field, value] : doc[key].items()) {
            if (auto f = fields.find(field); f != fields.end()) props[f->second] = value;
            else if (!(std::string(key) == "sprite" && field == "animation")) warn(where + ": " + key + "." + field + ": unknown field");
        }
        apply(std::string(key) + ".", props);
    };

    if (doc.contains("props")) apply("props.", doc["props"]);
    json top = json::object(); // child entries: their own name and local position (beat the child prefab's props)
    for (const char* key : {"name", "position"})
        if (doc.contains(key)) top[key] = doc[key];
    apply("", top);
    section("sprite", SpriteFields);
    section("collider", ColliderFields);
    if (doc.contains("extra")) {
        if (doc["extra"].is_object()) o.extra().update(doc["extra"]);
        else warn(where + ": extra: expected an object");
    }

    if (doc.contains("components") && doc["components"].is_array()) {
        for (const json& c : doc["components"]) {
            if (!c.is_object() || removed(c)) continue;
            const std::string cType = c.contains("type") && c["type"].is_string() ? c["type"].get<std::string>() : "";
            const TypeInfo* ct = typeRegistry().find(cType);
            if (!ct || !ct->isComponent) {
                warn(where + ": components: '" + cType + "' is not a registered component");
                continue;
            }
            Component* target = nullptr; // one onCreate already added is configured, not duplicated
            for (const auto& existing : o.components())
                if (!existing->removing() && typeOf(*existing).name == cType) target = existing.get();
            if (!target && !ct->createComponent) {
                warn(where + ": components: '" + cType + "' has no no-argument constructor");
                continue;
            }
            if (!target) target = &o.addComponent(ct->createComponent());
            if (c.contains("props"))
                for (const std::string& w : applyProps(static_cast<Component*>(target), cType, c["props"]))
                    warn(where + ": components." + cType + ".props." + w);
        }
    }

    std::map<std::string, std::string> animations; // state -> animation; sprite.animation is the "idle" one
    if (doc.contains("sprite") && doc["sprite"].is_object() && doc["sprite"].contains("animation")) {
        if (doc["sprite"]["animation"].is_string()) animations["idle"] = doc["sprite"]["animation"].get<std::string>();
        else warn(where + ": sprite.animation: expected an animation name");
    }
    if (doc.contains("animations") && doc["animations"].is_object()) {
        for (const auto& [state, anim] : doc["animations"].items()) {
            if (anim.is_string()) animations[state] = anim.get<std::string>();
            else warn(where + ": animations." + state + ": expected an animation name");
        }
    }
    if (!animations.empty()) {
        SpriteAnimator* animator = o.getComponent<SpriteAnimator>();
        if (!animator) animator = &o.addComponent<SpriteAnimator>();
        for (auto& [state, anim] : animations) animator->animations[state] = anim;
    }

    if (!prefab.empty()) lineage.push_back(prefab); // the prefabs being spawned, root first
    if (doc.contains("children") && doc["children"].is_array()) {
        for (const json& child : doc["children"]) {
            if (!child.is_object() || removed(child)) continue;
            const std::string childName = child.contains("name") && child["name"].is_string() ? child["name"].get<std::string>() : "?";
            const std::string childWhere = where + ": children." + childName;
            json childDoc = child;
            std::string childPrefab;
            if (child.contains("prefab")) {
                childPrefab = child["prefab"].is_string() ? child["prefab"].get<std::string>() : "";
                if (std::find(lineage.begin(), lineage.end(), childPrefab) != lineage.end()) {
                    std::string path;
                    for (const std::string& n : lineage) path += n + " -> ";
                    warn(childWhere + ": prefab '" + childPrefab + "' contains itself (" + path + childPrefab + "), child skipped");
                    continue;
                }
                std::string error;
                const json* base = depth < MaxDepth ? resolved(childPrefab, error) : nullptr;
                if (!error.empty()) warn(childWhere + ": " + error);
                if (!base) {
                    if (depth >= MaxDepth) warn(childWhere + ": children nested too deep");
                    continue;
                }
                childDoc = mergePrefab(*base, child);
                childDoc.erase("prefab");
            }
            spawn(scene, childDoc, childPrefab, childWhere, depth + 1, o.id, json(), {}, lineage);
        }
    }
    if (!prefab.empty()) lineage.pop_back();

    if (parent) scene.setParent(o.id, parent, false); // before the hook, so the child sees its parent

    if (overrides.is_object()) {
        for (const auto& [key, value] : overrides.items()) {
            if (value.is_null()) continue; // an unset LDtk field
            const PropertyInfo* p = findProp(typeName, key);
            std::string error;
            if (!p) o.extra()[key] = value;
            else if (p->set(self, fromLdtk(*p, value), error)) o.ldtkOverridden.insert(key);
            else warn(where + ": LDtk field " + key + ": " + error);
        }
    }

    if (o.sprite && !o.sprite->atlasName.empty()) { // spriteAtlas name -> the index the renderer uses
        const AssetRegistry* assets = scene.service<AssetRegistry>();
        const auto index = assets ? assets->atlasIndex(o.sprite->atlasName) : std::nullopt;
        if (index) o.sprite->atlasTextureIndex = *index;
        else warn(where + ": atlas '" + o.sprite->atlasName + "' not found" + (assets ? "" : " (no AssetRegistry service)"));
    }

    o.onPrefabApplied(scene);
    return &o;
}
