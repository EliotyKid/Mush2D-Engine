#include "engine/core/Project.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>

namespace {
// The active project lives in a function-local static, so it exists before any other static code uses it.
Project& activeStorage() {
    static Project project;
    return project;
}

// One warning line in the format "Project: <file>: <field>: <reason>".
void warn(const std::filesystem::path& file, const std::string& field, const std::string& reason) {
    std::cerr << "Project: " << file.generic_string() << ": " << field << ": " << reason << '\n';
}
}

bool Project::hasElement(std::string_view name) const {
    return std::find(elements.begin(), elements.end(), name) != elements.end();
}

std::string_view Project::barForElement(std::string_view element) const {
    if (element.empty()) return {};
    for (const ResourceBarDef& bar : resources)
        if (bar.element == element) return bar.name;
    return {};
}

SurfaceId Project::surface(std::string_view name) const {
    for (size_t i = 0; i < surfaces.size(); ++i)
        if (surfaces[i].name == name) return SurfaceId(i + 1); // ids start at 1: 0 is the default
    return DefaultSurface;
}

bool Project::surfaceHas(SurfaceId id, std::string_view tag) const {
    if (id == DefaultSurface || id > surfaces.size()) return false;
    const std::vector<std::string>& tags = surfaces[id - 1].tags;
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

std::optional<Project> loadProject(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        warn(file, "file", "cannot open it");
        return std::nullopt;
    }
    nlohmann::json doc = nlohmann::json::parse(in, nullptr, false); // false: no exception on bad JSON
    if (doc.is_discarded() || !doc.is_object()) {
        warn(file, "file", "not a valid JSON object");
        return std::nullopt;
    }

    Project project;
    for (const char* field : {"id", "name"}) { // required text fields
        if (!doc.contains(field) || !doc[field].is_string() || doc[field].get<std::string>().empty()) {
            warn(file, field, "expected a non-empty string");
            return std::nullopt;
        }
    }
    project.id = doc["id"].get<std::string>();
    project.name = doc["name"].get<std::string>();

    if (doc.contains("elements")) { // optional list of unique, non-empty names
        if (!doc["elements"].is_array()) {
            warn(file, "elements", "expected a list of names");
            return std::nullopt;
        }
        std::set<std::string> seen;
        for (const nlohmann::json& item : doc["elements"]) {
            if (!item.is_string() || item.get<std::string>().empty()) {
                warn(file, "elements", "every name must be a non-empty string");
                return std::nullopt;
            }
            if (!seen.insert(item.get<std::string>()).second) {
                warn(file, "elements", "duplicate name '" + item.get<std::string>() + "'");
                return std::nullopt;
            }
            project.elements.push_back(item.get<std::string>());
        }
    }

    if (doc.contains("resources")) { // optional list of bars: {name, element?, max?, regenPerSecond?, regenDelay?}
        if (!doc["resources"].is_array()) {
            warn(file, "resources", "expected a list of bars");
            return std::nullopt;
        }
        std::set<std::string> names;
        for (const nlohmann::json& item : doc["resources"]) {
            if (!item.is_object() || !item.contains("name") || !item["name"].is_string() || item["name"].get<std::string>().empty()) {
                warn(file, "resources", "every bar needs a non-empty \"name\"");
                return std::nullopt;
            }
            ResourceBarDef bar;
            bar.name = item["name"].get<std::string>();
            const std::string where = "resources." + bar.name;
            if (!names.insert(bar.name).second) {
                warn(file, "resources", "duplicate bar name '" + bar.name + "'");
                return std::nullopt;
            }
            if (item.contains("element")) {
                if (!item["element"].is_string()) {
                    warn(file, where + ".element", "expected an element name");
                    return std::nullopt;
                }
                bar.element = item["element"].get<std::string>();
                if (!bar.element.empty() && !project.hasElement(bar.element)) {
                    warn(file, where + ".element", "unknown element '" + bar.element + "' (not in \"elements\")");
                    return std::nullopt;
                }
            }
            // The three numbers: max must be above 0, the regen values 0 or more.
            struct Number { const char* key; float* target; bool positive; };
            for (const Number& n : {Number{"max", &bar.max, true}, {"regenPerSecond", &bar.regenPerSecond, false}, {"regenDelay", &bar.regenDelay, false}}) {
                if (!item.contains(n.key)) continue; // missing: the default stays
                if (!item[n.key].is_number() || item[n.key].get<float>() < 0.0f || (n.positive && item[n.key].get<float>() <= 0.0f)) {
                    warn(file, where + "." + n.key, n.positive ? "expected a number above 0" : "expected a number of 0 or more");
                    return std::nullopt;
                }
                *n.target = item[n.key].get<float>();
            }
            project.resources.push_back(bar);
        }
    }

    if (doc.contains("surfaces")) { // optional list of surfaces: {name, intGrid?, terrainFlag?, tags?}
        if (!doc["surfaces"].is_array()) {
            warn(file, "surfaces", "expected a list of surfaces");
            return std::nullopt;
        }
        if (doc["surfaces"].size() > 255) { // a SurfaceId is one byte and 0 is the default
            warn(file, "surfaces", "at most 255 surfaces");
            return std::nullopt;
        }
        std::set<std::string> names;
        for (const nlohmann::json& item : doc["surfaces"]) {
            if (!item.is_object() || !item.contains("name") || !item["name"].is_string() || item["name"].get<std::string>().empty()) {
                warn(file, "surfaces", "every surface needs a non-empty \"name\"");
                return std::nullopt;
            }
            SurfaceDef def;
            def.name = item["name"].get<std::string>();
            const std::string where = "surfaces." + def.name;
            if (!names.insert(def.name).second) {
                warn(file, "surfaces", "duplicate surface name '" + def.name + "'");
                return std::nullopt;
            }
            // The two optional text links to the level editor.
            struct Text { const char* key; std::string* target; };
            for (const Text& t : {Text{"intGrid", &def.intGrid}, {"terrainFlag", &def.terrainFlag}}) {
                if (!item.contains(t.key)) continue;
                if (!item[t.key].is_string()) {
                    warn(file, where + "." + t.key, "expected a text");
                    return std::nullopt;
                }
                *t.target = item[t.key].get<std::string>();
            }
            if (item.contains("tags")) {
                if (!item["tags"].is_array()) {
                    warn(file, where + ".tags", "expected a list of texts");
                    return std::nullopt;
                }
                for (const nlohmann::json& tag : item["tags"]) {
                    if (!tag.is_string()) {
                        warn(file, where + ".tags", "every tag must be a text");
                        return std::nullopt;
                    }
                    def.tags.push_back(tag.get<std::string>());
                }
            }
            project.surfaces.push_back(std::move(def));
        }
    }
    return project;
}

const Project& activeProject() { return activeStorage(); }

void setActiveProject(Project project) { activeStorage() = std::move(project); }

bool reloadActiveProject(const std::filesystem::path& file) {
    std::optional<Project> project = loadProject(file);
    if (!project) return false; // the old project keeps running
    setActiveProject(std::move(*project));
    return true;
}
