#pragma once

#include <filesystem>
#include <string_view>

// <dir of the executable>/relative
std::filesystem::path assetPath(std::string_view relative);

// Debug: MUSH2D_SOURCE_DIR/relative (edit assets live). Release: same as assetPath.
std::filesystem::path sourceAssetPath(std::string_view relative);
