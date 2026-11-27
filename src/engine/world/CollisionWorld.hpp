#pragma once

#include "CollisionTypes.hpp"
#include "engine/core/Project.hpp"
#include "Ldtk.hpp"
#include "SpatialGrid.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

// A collision line in world units (Y up). The normal points out of the solid; it is the right-hand
// perpendicular of a->b, so a polygon walked counter-clockwise has outward normals.
struct Segment {
    glm::vec2 a{0.0f}, b{0.0f}, normal{0.0f, 1.0f};
    bool oneWay = false;
    std::string roomIid;
    SurfaceId surface = DefaultSurface; // what the segment is made of, for gameplay (see Project::surfaces)
};

// IntGrid values: 1 solid, 2 one-way top, 3/4 ramp 45 deg up-right/left, 5/6 22.5 deg low half,
// 7/8 22.5 deg high half. Any other value listed in `valueSurface` (IntGrid value -> surface) is a full solid
// cell whose faces get that surface; values 1-8 listed there keep their shape and also get the surface.
// Outer edges only, collinear neighbours of the same surface merged. Vertical faces lying on the room
// border (x = 0 or x = levelPxSize.x) are not generated.
std::vector<Segment> segmentsFromIntGrid(const LdtkLayer& collision, glm::ivec2 levelWorldPx, glm::ivec2 levelPxSize,
                                         const std::string& roomIid, const std::map<int, SurfaceId>& valueSurface);
// Terrain entity as authored: points = entity position, then fields["points"] (dup of the first dropped),
// in world units; closed / oneWay / thickness (default 3 u) from the fields. `surface` comes from the first
// project surface whose terrainFlag is a true boolean field of the entity (DefaultSurface when none is).
struct TerrainShape { std::vector<glm::vec2> points; bool closed = false, oneWay = false; SurfaceId surface = DefaultSurface; float thickness = 3.0f; };
TerrainShape terrainShape(const LdtkEntity& e, glm::ivec2 levelWorldPx, int layerGridPx);
// Terrain entity: points = entity position, then fields["points"] (F_Point array, cells of layerGridPx).
// fields: closed (polygon; else a surface filled down by thickness, default 3 u), oneWay (keeps only
// segments with normal.y > 0.7), plus the project's terrainFlag fields (every segment gets that surface). Winding is corrected
// so normals point to the air.
std::vector<Segment> segmentsFromTerrain(const LdtkEntity& e, glm::ivec2 levelWorldPx, int layerGridPx,
                                         const std::string& roomIid);

// Segments per room, indexed in a private SpatialGrid (ids are segment slots with generation 1).
// query() is a broadphase: it returns every segment whose AABB (padded by a hair) overlaps the box.
class CollisionWorld {
public:
    void addRoom(const std::string& roomIid, std::vector<Segment> segments); // replaces an existing room
    void removeRoom(const std::string& roomIid);
    void query(const Collision2D::AABB& box, std::vector<const Segment*>& out) const; // replaces `out`

private:
    struct Room { std::vector<Segment> segments; std::vector<std::uint32_t> ids; };
    std::unordered_map<std::string, Room> rooms_; // node-stable: Segment pointers survive other rooms changing
    std::unordered_map<std::uint32_t, const Segment*> byId_;
    SpatialGrid grid_{2.0f};
    std::uint32_t next_ = 1;
};
