#pragma once

#include "engine/core/GameRegistry.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The last game the person opened, kept in a small file: {"game": "<id>"}.
// Returns "" when the file is missing or cannot be read as that format.
std::string readLastGame(const std::filesystem::path& file);
// Writes {"game": "<id>"} to the file. A failed write is ignored: remembering is a convenience.
void writeLastGame(const std::filesystem::path& file, std::string_view id);

// A game folder that has a project.json but is not built into this program.
struct UncompiledGame {
    std::string id;
    std::string name;
};
// Looks for examples/*/project.json and games/*/project.json under root and returns those
// whose id is not in `compiled`.
std::vector<UncompiledGame> findUncompiledGames(const std::filesystem::path& root, const std::vector<GameEntry>& compiled);

// Runs ONE window frame. It calls drawUi between ImGui::NewFrame and ImGui::Render, then presents.
// Returns false when the window was closed. The Engine provides it, so the launcher shares its window.
using FrameRunner = std::function<bool(const std::function<void()>& drawUi)>;

// Chooses the game to open.
//  - cliGame names a compiled game: returns it at once, with no screen.
//  - otherwise shows a list (one button per game, the last choice marked; Enter opens the marked one)
//    using runFrame, and writes the choice to lastFile.
// Returns nullptr when the window closes before a choice (or when runFrame is empty and there is no cliGame).
const GameEntry* pickGame(const std::vector<GameEntry>& games, const std::filesystem::path& lastFile,
                          std::string_view cliGame, const FrameRunner& runFrame = {});
