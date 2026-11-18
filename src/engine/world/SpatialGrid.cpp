#include "SpatialGrid.hpp"

#include "Collision.hpp"

#include <algorithm>
#include <cmath>

SpatialGrid::Range SpatialGrid::cellsOf(const Collision2D::AABB& box) const {
    auto c = [&](float v) { return static_cast<int>(std::floor(v / cell_)); };
    return {c(box.min.x), c(box.min.y), c(box.max.x), c(box.max.y)};
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
    Range r = cellsOf(box);
    for (int y = r.y0; y <= r.y1; ++y) {
        for (int x = r.x0; x <= r.x1; ++x) {
            auto cell = cells_.find(key(x, y));
            if (cell == cells_.end()) continue;
            for (GameObjectId id : cell->second) {
                const Entry& e = entries_.at(id);
                if (kind == Collision2D::QueryKind::Solid && e.isTrigger) continue;
                if (kind == Collision2D::QueryKind::Trigger && !e.isTrigger) continue;
                if (Collision2D::intersects(box, e.box)) out.push_back(id);
            }
        }
    }
    // an object spanning several visited cells is listed once per cell
    std::sort(out.begin(), out.end(), [](GameObjectId a, GameObjectId b) {
        return a.index != b.index ? a.index < b.index : a.generation < b.generation;
    });
    out.erase(std::unique(out.begin(), out.end()), out.end());
}
