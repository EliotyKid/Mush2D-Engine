#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// One named resource bar (for example a stamina or mana bar). `element` links it to a combat element name
// (empty = none). The bar refills `regenPerSecond` per second once `regenDelay` seconds passed since it was last spent.
struct ResourceBarDef {
    std::string name, element;
    float max = 100.0f, regenPerSecond = 10.0f, regenDelay = 1.0f;
};

// What a game declares in its project.json. The engine reads it; it never knows the game's own content.
// Later phases add more fields here (surfaces, input actions).
struct Project {
    std::string id, name;
    std::vector<std::string> elements; // the combat element names (for example the ones a hit can carry)

    std::vector<ResourceBarDef> resources; // the named bars every Resources component starts with

    bool hasElement(std::string_view name) const;
    // The name of the bar linked to `element`, or "" when no bar is.
    std::string_view barForElement(std::string_view element) const;
};

// Reads and checks a project.json. On any error it prints "Project: <file>: <field>: <reason>" to std::cerr
// and returns nullopt.
std::optional<Project> loadProject(const std::filesystem::path& file);

// The project of the running game (empty until Engine sets it).
const Project& activeProject();
void setActiveProject(Project project);

// Debug hot reload: loads `file` and replaces the active project only if that worked. True when replaced.
bool reloadActiveProject(const std::filesystem::path& file);
