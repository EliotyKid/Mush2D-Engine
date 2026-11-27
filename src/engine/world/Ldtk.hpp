#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

constexpr float LdtkPixelsPerUnit = 32.0f;

struct IntRect { int x, y, w, h; }; // in cells

// Greedy row-by-row merge of cells equal to solidValue. Order = scan order of each rect's first cell.
std::vector<IntRect> mergeIntGrid(const std::vector<int>& csv, int cw, int ch, int solidValue);

struct LdtkTile { glm::ivec2 px, src; int flip; }; // px: top-left in level pixels (layer offset applied); flip bit0 = X, bit1 = Y
struct LdtkEntity {
    std::string identifier, iid;
    glm::vec2 px;    // top-left in level pixels (pivot already removed)
    glm::vec2 size;  // pixels
    nlohmann::json fields; // {identifier: value}; F_Point = {cx,cy} in layer cells, arrays of it are JSON lists
    int layerGridPx = 16;            // grid size of the owning layer
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
    std::map<int, std::string> intGridNames; // IntGrid value -> its identifier in the LDtk layer definition
    std::vector<int> intGrid; std::vector<LdtkTile> tiles; std::vector<LdtkEntity> entities;
};
struct LdtkLevel {
    std::string identifier, iid; glm::ivec2 pxSize{0, 0};
    glm::ivec2 worldPx{0, 0};      // LDtk worldX/worldY, pixels, Y down
    std::string region = "forest"; // level field "region"; "forest" when absent
    std::filesystem::path baseDir; // directory of the .ldtk file
    std::vector<LdtkLayer> layers; // front -> back, as in LDtk
};

struct LdtkProject { std::filesystem::path baseDir; std::vector<LdtkLevel> levels; };
// Parses every level. Throws std::runtime_error on missing file or bad JSON.
LdtkProject loadLdtkProject(const std::filesystem::path& ldtk);
// World units (32 px) with one Y-up axis: ((worldX + px.x) / 32, -(worldY + px.y) / 32).
glm::vec2 ldtkToWorld(glm::ivec2 levelWorldPx, glm::vec2 px);
// Per level, indices (ascending) of levels whose rects touch or overlap within tolerancePx.
std::vector<std::vector<int>> computeNeighbours(const std::vector<LdtkLevel>& levels, int tolerancePx = 1);
// Throws std::runtime_error on missing file, bad JSON or unknown level.
LdtkLevel loadLdtkLevel(const std::filesystem::path& ldtk, const std::string& levelIdentifier);
// Identifier suffix "_d<number>" wins; else index-based: layers behind the gameplay layer get positive (far) depth.
float parseLayerDepth(const std::string& identifier, int orderFromFront, int frontIndexOfGameplay);
// LDtk parallaxFactor: 0 = scrolls with the level, 1 = fixed on screen. Engine: 1 = scrolls with camera, 0 = fixed.
float ldtkParallaxToScroll(float ldtkFactor);
// LDtk (parallaxScaling off) shows a parallax layer at its authored place when the camera is at the level centre.
// The engine shows it there when the camera is at 0, so shift by (1 - scroll) * levelCentre (world units).
glm::vec2 ldtkParallaxAnchor(glm::vec2 worldPos, glm::vec2 scroll, glm::vec2 levelCentre);

// Entity field readers: the fallback when the field is absent or of another type.
float numberField(const LdtkEntity& e, const char* key, float fallback);
std::string stringField(const LdtkEntity& e, const char* key, const std::string& fallback);
// "#RRGGBB" -> linear-ish 0..1 (lights are HDR intensities, so plain /255 is fine).
glm::vec3 colorField(const LdtkEntity& e, const char* key, glm::vec3 fallback);
// Entities use pivot (0.5, 1): the bottom-centre of the box (centre in world units) is where they stand.
glm::vec2 bottomOf(const LdtkEntity& e, glm::vec2 centre);
