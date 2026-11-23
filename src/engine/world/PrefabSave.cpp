#include "engine/world/PrefabSave.hpp"

#include <format>
#include <fstream>

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

namespace {

// Floats as their shortest text (1.2f -> 1.2, not 1.2000000476837158), so files stay readable and a value read
// back from a file compares equal to the attribute it set.
json tidy(const json& v) {
    if (v.is_number_float()) return std::stod(std::format("{}", v.get<float>()));
    if (!v.is_array()) return v;
    json out = json::array();
    for (const json& e : v) out.push_back(tidy(e));
    return out;
}

// The edited attributes in prefab form: the differing ones with their value, the others null (= remove).
json editedDiff(const GameObject& o, const json& base, const std::set<std::string>& edited) {
    const json diff = diffAgainstBase(o, base);
    json doc = json::object();
    for (const std::string& name : edited) {
        if (o.ldtkOverridden.count(name)) continue;
        const auto [section, field] = prefabFieldOf(name);
        doc[section][field] = diff.contains(section) && diff[section].contains(field) ? diff[section][field] : json();
    }
    return doc;
}

// The resolved prefab `name` into out ({} = C++ defaults when name is empty).
bool resolvedOrDefaults(PrefabLibrary& library, const std::string& name, json& out, std::string& error) {
    out = json::object();
    if (name.empty()) return true;
    const json* doc = library.resolved(name, error);
    if (!doc) return false;
    out = *doc;
    error.clear(); // a "kept the last good version" note is not a failure
    return true;
}

} // namespace

json diffAgainstBase(const GameObject& object, const json& resolvedBase) {
    json diff = json::object();
    const void* self = static_cast<const GameObject*>(&object);
    for (const PropertyInfo* p : typeRegistry().allProps(typeOf(object).name)) {
        if (object.ldtkOverridden.count(p->name)) continue;
        const auto [section, field] = prefabFieldOf(p->name);
        const bool inBase = resolvedBase.contains(section) && resolvedBase[section].is_object() && resolvedBase[section].contains(field);
        json value = tidy(p->get(self));
        if (value != tidy(inBase ? resolvedBase[section][field] : p->defaultValue)) diff[section][field] = std::move(value);
    }
    return diff;
}

bool savePrefab(const std::filesystem::path& path, const json& doc, std::string& error) {
    static const std::set<std::string> Kept = {"version", "base", "type", "children", "components"};
    const std::string file = path.filename().string();
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    ordered_json out = ordered_json::object(); // ordered: the file keeps its key order
    if (exists) {
        std::ifstream in(path);
        out = ordered_json::parse(in, nullptr, false);
        if (out.is_discarded() || !out.is_object()) {
            error = file + ": not a valid prefab, left unchanged";
            return false;
        }
    } else {
        out["version"] = 1;
    }
    const ordered_json patch = ordered_json::parse(doc.dump());
    for (const char* key : {"base", "type"}) // a new file: its identity first
        if (!exists && patch.contains(key)) out[key] = patch[key];
    for (const auto& [key, value] : patch.items()) {
        if (Kept.count(key)) {
            if (!exists && !out.contains(key)) out[key] = value;
        } else if (value.is_object()) {
            ordered_json& section = out[key];
            if (!section.is_object()) section = ordered_json::object();
            section.merge_patch(value); // null removes a key
            if (section.empty()) out.erase(key);
        } else if (value.is_null()) {
            out.erase(key);
        } else {
            out[key] = value;
        }
    }
    std::ofstream os(path);
    os << out.dump(2, ' ', false, ordered_json::error_handler_t::replace) << '\n';
    if (!os) {
        error = file + ": could not write " + path.string();
        return false;
    }
    return true;
}

bool saveToSourcePrefab(PrefabLibrary& library, const GameObject& object, const std::set<std::string>& edited,
                        std::string& error) {
    if (!canSaveToPrefab(object)) {
        error = "'" + object.name + "' has no source prefab (made in code): use save as new prefab";
        return false;
    }
    const std::filesystem::path path = library.dir() / (object.prefab + ".prefab.json");
    std::ifstream in(path);
    const json own = json::parse(in, nullptr, false);
    if (own.is_discarded() || !own.is_object()) {
        error = path.filename().string() + ": not a valid prefab, left unchanged";
        return false;
    }
    const std::string baseName = own.contains("base") && own["base"].is_string() ? own["base"].get<std::string>() : "";
    json base;
    if (!resolvedOrDefaults(library, baseName, base, error) ||
        !savePrefab(path, editedDiff(object, base, edited), error))
        return false;
    library.clear();
    return true;
}

bool saveAsNewPrefab(PrefabLibrary& library, const GameObject& object, const std::string& name,
                     const std::set<std::string>& edited, std::string& error) {
    if (!validPrefabName(name)) {
        error = "invalid prefab name '" + name + "' (letters, digits, '_' and '-' only)";
        return false;
    }
    if (library.has(name)) {
        error = name + ".prefab.json already exists";
        return false;
    }
    json base;
    if (!resolvedOrDefaults(library, object.prefab, base, error)) return false;
    json doc = editedDiff(object, base, edited);
    if (!object.prefab.empty()) doc["base"] = object.prefab;
    doc["type"] = typeOf(object).name;
    if (!savePrefab(library.dir() / (name + ".prefab.json"), doc, error)) return false;
    library.clear();
    return true;
}
