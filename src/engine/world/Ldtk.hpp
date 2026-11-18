#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

struct IntRect { int x, y, w, h; }; // in cells

// Greedy row-by-row merge of cells equal to solidValue. Order = scan order of each rect's first cell.
std::vector<IntRect> mergeIntGrid(const std::vector<int>& csv, int cw, int ch, int solidValue);

struct LdtkTile { glm::ivec2 px, src; int flip; }; // px: top-left in level pixels (layer offset applied); flip bit0 = X, bit1 = Y
struct LdtkEntity {
    std::string identifier, iid;
    glm::vec2 px;    // top-left in level pixels (pivot already removed)
    glm::vec2 size;  // pixels
    nlohmann::json fields; // {identifier: value}
    float depth = 0.0f;              // of the owning layer
    glm::vec2 parallax{1.0f, 1.0f};  // of the owning layer, engine convention
};
struct LdtkLayer {
    std::string identifier, type;
    int gridSize = 16; glm::ivec2 cells{0, 0};
    float parallaxX = 1.0f, parallaxY = 1.0f; // engine convention: 1 = moves with gameplay layer
    float depth = 0.0f;
    std::string tilesetPath;     // relative to the .ldtk; empty if none
    glm::ivec2 tilesetPx{0, 0};  // tileset image size in pixels
    std::vector<int> intGrid; std::vector<LdtkTile> tiles; std::vector<LdtkEntity> entities;
};
struct LdtkLevel {
    std::string identifier; glm::ivec2 pxSize{0, 0};
    std::filesystem::path baseDir; // directory of the .ldtk file
    std::vector<LdtkLayer> layers; // front -> back, as in LDtk
};

// Throws std::runtime_error on missing file, bad JSON or unknown level.
LdtkLevel loadLdtkLevel(const std::filesystem::path& ldtk, const std::string& levelIdentifier);
// Identifier suffix "_d<number>" wins; else index-based: layers behind the gameplay layer get positive (far) depth.
float parseLayerDepth(const std::string& identifier, int orderFromFront, int frontIndexOfGameplay);
// LDtk parallaxFactor: 0 = scrolls with the level, 1 = fixed on screen. Engine: 1 = scrolls with camera, 0 = fixed.
float ldtkParallaxToScroll(float ldtkFactor);
// LDtk (parallaxScaling off) shows a parallax layer at its authored place when the camera is at the level centre.
// The engine shows it there when the camera is at 0, so shift by (1 - scroll) * levelCentre (world units).
glm::vec2 ldtkParallaxAnchor(glm::vec2 worldPos, glm::vec2 scroll, glm::vec2 levelCentre);
