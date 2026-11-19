#pragma once

#include "engine/render/Materials.hpp"
#include "engine/render/SpriteBatch.hpp"
#include "engine/world/CollisionWorld.hpp"

#include <vector>

// Textures and world sizes (units) of one terrain look. edge/side tile along a segment (edgeSize = one
// tile's length and its thickness); fill repeats every fillTile (columns are 0.5 u, so fillTile.x = 1 u).
struct TerrainStyle {
    Material edge, side, fill;
    glm::vec2 edgeSize{1.0f, 0.5f}, fillTile{1.0f, 1.0f};
};

// Terrain visual as plain sprites (no new pipeline). `polyline` uses only a/b of each segment, consecutive:
//  - open: the surface; it is filled down `thickness` below its end points (same shape as segmentsFromTerrain);
//  - closed: the polygon (last.b == first.a).
// Fill: 0.5 u wide columns split into 1 u cells with world-anchored uvRects (seamless repeat). Edge: rotated
// pieces along every segment, `edge` where the normal looks up (y > 0.3), `side` elsewhere. Depth, parallax
// and tint are left at gameplay defaults for the caller to set. Order: fill first, then edge pieces.
void buildTerrainMesh(const std::vector<Segment>& polyline, bool closed, float thickness, const TerrainStyle& style,
                      std::vector<SpriteInstance>& out);
