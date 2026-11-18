#pragma once

#include "GameObject.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace Collision2D {

struct AABB {
    glm::vec2 min{0.0f, 0.0f};
    glm::vec2 max{0.0f, 0.0f};
};

enum class QueryKind { Any, Solid, Trigger };

}

// Uniform-grid broadphase. Stores one AABB per id (and whether it is a trigger) and answers
// "which stored boxes overlap this box" without scanning everything.
class SpatialGrid {
public:
    explicit SpatialGrid(float cell = 2.0f) : cell_(cell) {}

    void insert(GameObjectId id, const Collision2D::AABB& box, bool isTrigger = false) { update(id, box, isTrigger); }
    // Inserts when the id is not stored yet.
    void update(GameObjectId id, const Collision2D::AABB& box, bool isTrigger = false);
    void remove(GameObjectId id);
    void clear();

    // Replaces `out` with the ids whose stored box overlaps `box` (strict, like Collision2D::intersects).
    // No duplicates; sorted by (index, generation).
    void query(const Collision2D::AABB& box, std::vector<GameObjectId>& out,
               Collision2D::QueryKind kind = Collision2D::QueryKind::Any) const;

private:
    struct Entry {
        Collision2D::AABB box;
        bool isTrigger = false;
    };
    struct Range {
        int x0, y0, x1, y1;
    };
    Range cellsOf(const Collision2D::AABB& box) const;
    static std::int64_t key(int x, int y) { return (std::int64_t(x) << 32) ^ std::int64_t(std::uint32_t(y)); }
    void unlink(GameObjectId id, const Collision2D::AABB& box);

    float cell_;
    std::unordered_map<GameObjectId, Entry> entries_;
    std::unordered_map<std::int64_t, std::vector<GameObjectId>> cells_;
};
