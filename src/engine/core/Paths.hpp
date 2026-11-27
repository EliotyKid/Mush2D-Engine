#pragma once

#include <filesystem>
#include <string_view>

// The folder of the game that is running, relative to the project root (for example "examples/my_game").
// Engine::run sets it before the game starts. Empty means "no game": paths then point at the exe dir itself.
void setGameRoot(const std::filesystem::path& dir);
const std::filesystem::path& gameRoot();

// <dir of the executable>/<gameRoot>/relative. The build copies each game's assets/ there.
std::filesystem::path assetPath(std::string_view relative);

// Debug: MUSH2D_SOURCE_DIR/<gameRoot>/relative (edit assets live). Release: same as assetPath.
std::filesystem::path sourceAssetPath(std::string_view relative);

// <dir of the executable>/relative, whatever game runs. For the engine's own files (compiled shaders).
std::filesystem::path enginePath(std::string_view relative);
