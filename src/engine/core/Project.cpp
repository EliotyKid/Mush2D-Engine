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
