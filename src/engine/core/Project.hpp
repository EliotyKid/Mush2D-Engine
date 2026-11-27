#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// What a game declares in its project.json. The engine reads it; it never knows the game's own content.
// Later phases add more fields here (resource bars, surfaces, input actions).
struct Project {
    std::string id, name;
    std::vector<std::string> elements; // the combat element names (for example the ones a hit can carry)

    bool hasElement(std::string_view name) const;
};

// Reads and checks a project.json. On any error it prints "Project: <file>: <field>: <reason>" to std::cerr
// and returns nullopt.
std::optional<Project> loadProject(const std::filesystem::path& file);

// The project of the running game (empty until Engine sets it).
const Project& activeProject();
void setActiveProject(Project project);

// Debug hot reload: loads `file` and replaces the active project only if that worked. True when replaced.
bool reloadActiveProject(const std::filesystem::path& file);
