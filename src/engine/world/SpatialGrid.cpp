#include "SpatialGrid.hpp"

#include "Collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
bool finite(const Collision2D::AABB& b) {
    return std::isfinite(b.min.x) && std::isfinite(b.min.y) && std::isfinite(b.max.x) && std::isfinite(b.max.y);
}
}

SpatialGrid::Range SpatialGrid::cellsOf(const Collision2D::AABB& box) const {
    // Clamp before the int cast: a huge float would be undefined behaviour.
    auto c = [&](float v) { return static_cast<int>(std::clamp(std::floor(double(v) / cell_), -1.0e9, 1.0e9)); };
    Range r{c(box.min.x), c(box.min.y), c(box.max.x), c(box.max.y)};
    if (r.x1 - r.x0 >= kMaxCells) { r.x1 = r.x0 + kMaxCells - 1; r.clamped = true; }
    if (r.y1 - r.y0 >= kMaxCells) { r.y1 = r.y0 + kMaxCells - 1; r.clamped = true; }
    if (r.clamped) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "SpatialGrid: a box spans more than %d cells on an axis; indexing only its first %d\n", kMaxCells, kMaxCells);
        }
    }
    return r;
}

void SpatialGrid::unlink(GameObjectId id, const Collision2D::AABB& box) {
    Range r = cellsOf(box);
    for (int y = r.y0; y <= r.y1; ++y) {
        for (int x = r.x0; x <= r.x1; ++x) {
            auto it = cells_.find(key(x, y));
            if (it == cells_.end()) continue;
            std::erase(it->second, id);
            if (it->second.empty()) cells_.erase(it);
        }
    }
}

void SpatialGrid::update(GameObjectId id, const Collision2D::AABB& box, bool isTrigger) {
    if (!finite(box)) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "SpatialGrid: box with a NaN or infinite coordinate skipped; the object is left out of queries\n");
        }
        remove(id);
        return;
    }
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        Range a = cellsOf(it->second.box), b = cellsOf(box);
        if (a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1) { // same cells: nothing to relink
            it->second = {box, isTrigger};
            return;
        }
        unlink(id, it->second.box);
        it->second = {box, isTrigger};
    } else {
        entries_.emplace(id, Entry{box, isTrigger});
    }
    Range r = cellsOf(box);
    for (int y = r.y0; y <= r.y1; ++y) {
        for (int x = r.x0; x <= r.x1; ++x) cells_[key(x, y)].push_back(id);
    }
}

void SpatialGrid::remove(GameObjectId id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) return;
    unlink(id, it->second.box);
    entries_.erase(it);
}

void SpatialGrid::clear() {
    entries_.clear();
    cells_.clear();
}

void SpatialGrid::query(const Collision2D::AABB& box, std::vector<GameObjectId>& out, Collision2D::QueryKind kind) const {
    out.clear();
    if (!finite(box)) return;
    auto consider = [&](GameObjectId id, const Entry& e) {
        if (kind == Collision2D::QueryKind::Solid && e.isTrigger) return;
        if (kind == Collision2D::QueryKind::Trigger && !e.isTrigger) return;
        if (Collision2D::intersects(box, e.box)) out.push_back(id);
    };
    Range r = cellsOf(box);
    if (r.clamped) { // too wide for the cell index (and a clamped entry is not fully indexed): scan everything
        for (const auto& [id, e] : entries_) consider(id, e);
    } else {
        for (int y = r.y0; y <= r.y1; ++y) {
            for (int x = r.x0; x <= r.x1; ++x) {
                auto cell = cells_.find(key(x, y));
                if (cell == cells_.end()) continue;
                for (GameObjectId id : cell->second) consider(id, entries_.at(id));
            }
        }
    }
    // an object spanning several visited cells is listed once per cell
    std::sort(out.begin(), out.end(), [](GameObjectId a, GameObjectId b) {
        return a.index != b.index ? a.index < b.index : a.generation < b.generation;
    });
    out.erase(std::unique(out.begin(), out.end()), out.end());
}
