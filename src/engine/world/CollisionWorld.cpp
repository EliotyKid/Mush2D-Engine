#include "CollisionWorld.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace {

// IntGrid cells are built in half-cell integer coordinates (x right, y up) so merging is exact.
struct Raw { glm::ivec2 a, b; bool oneWay; };

// Top-edge heights (half cells) at the left and right side of a solid cell; {-1,-1} = not solid.
glm::ivec2 profile(int v) {
    switch (v) {
        case 1: return {2, 2};
        case 3: return {0, 2};
        case 4: return {2, 0};
        case 5: return {0, 1};
        case 6: return {1, 0};
        case 7: return {1, 2};
        case 8: return {2, 1};
        default: return {-1, -1};
    }
}

bool sameLine(const Raw& p, const Raw& q) {
    const glm::ivec2 d = p.b - p.a, e = q.b - q.a;
    return p.oneWay == q.oneWay && d.x * e.y - d.y * e.x == 0 && d.x * e.x + d.y * e.y > 0;
}

// Joins end-to-start collinear segments of the same kind into one.
std::vector<Raw> mergeCollinear(const std::vector<Raw>& in) {
    using Key = std::pair<int, int>;
    std::map<Key, std::vector<int>> starts, ends;
    for (int i = 0; i < int(in.size()); ++i) {
        starts[{in[i].a.x, in[i].a.y}].push_back(i);
        ends[{in[i].b.x, in[i].b.y}].push_back(i);
    }
    auto find = [&](std::map<Key, std::vector<int>>& m, glm::ivec2 p, const Raw& ref, const std::vector<char>& used) {
        auto it = m.find({p.x, p.y});
        if (it != m.end())
            for (int j : it->second) if (!used[j] && sameLine(ref, in[j])) return j;
        return -1;
    };
    std::vector<char> used(in.size(), 0);
    std::vector<Raw> out;
    for (int i = 0; i < int(in.size()); ++i) {
        if (used[i]) continue;
        int head = i;
        for (int j; (j = find(ends, in[head].a, in[head], used)) >= 0;) head = j;
        Raw r = in[head];
        used[head] = 1;
        for (int j; (j = find(starts, r.b, r, used)) >= 0;) { r.b = in[j].b; used[j] = 1; }
        out.push_back(r);
    }
    return out;
}

Segment makeSegment(glm::vec2 a, glm::vec2 b, bool oneWay, const std::string& roomIid) {
    const glm::vec2 d = b - a;
    return {a, b, glm::normalize(glm::vec2(d.y, -d.x)), oneWay, roomIid};
}

} // namespace

std::vector<Segment> segmentsFromIntGrid(const LdtkLayer& L, glm::ivec2 levelWorldPx, glm::ivec2 levelPxSize,
                                         const std::string& roomIid) {
    const int cw = L.cells.x, ch = L.cells.y, g = L.gridSize;
    auto at = [&](int x, int y) { return x < 0 || y < 0 || x >= cw || y >= ch ? 0 : L.intGrid[size_t(y) * cw + x]; };
    auto solid = [&](int x, int y) { return profile(at(x, y)).x >= 0; };

    std::vector<Raw> raw;
    for (int cy = 0; cy < ch; ++cy)
        for (int cx = 0; cx < cw; ++cx) {
            const int v = at(cx, cy);
            const glm::ivec2 o{2 * cx, -2 * cy - 2}; // lower-left corner
            auto add = [&](glm::ivec2 a, glm::ivec2 b, bool oneWay = false) { raw.push_back({o + a, o + b, oneWay}); };
            if (v == 2) { add({2, 2}, {0, 2}, true); continue; }
            const glm::ivec2 p = profile(v);
            if (p.x < 0) continue;
            if (at(cx, cy + 1) != 1) add({0, 0}, {2, 0});                         // bottom
            const glm::ivec2 pr = profile(at(cx + 1, cy)), pl = profile(at(cx - 1, cy));
            if (p.y > std::max(pr.x, 0) && (cx + 1) * g < levelPxSize.x) add({2, std::max(pr.x, 0)}, {2, p.y}); // right
            if (p.x > std::max(pl.y, 0) && cx > 0) add({0, p.x}, {0, std::max(pl.y, 0)});                      // left
            if (!(p.x == 2 && p.y == 2 && solid(cx, cy - 1))) add({2, p.y}, {0, p.x}); // top / diagonal
        }

    std::vector<Segment> out;
    auto toWorld = [&](glm::ivec2 h) { return ldtkToWorld(levelWorldPx, glm::vec2(h.x * g * 0.5f, -h.y * g * 0.5f)); };
    for (const Raw& r : mergeCollinear(raw)) out.push_back(makeSegment(toWorld(r.a), toWorld(r.b), r.oneWay, roomIid));
    return out;
}

std::vector<Segment> segmentsFromTerrain(const LdtkEntity& e, glm::ivec2 levelWorldPx, int layerGridPx,
                                         const std::string& roomIid) {
    auto flag = [&](const char* key, bool def) {
        auto it = e.fields.find(key);
        return it != e.fields.end() && it->is_boolean() ? it->get<bool>() : def;
    };
    const bool closed = flag("closed", false), oneWay = flag("oneWay", false);
    float thickness = 3.0f;
    if (auto it = e.fields.find("thickness"); it != e.fields.end() && it->is_number()) thickness = it->get<float>();

    std::vector<glm::vec2> pts{ldtkToWorld(levelWorldPx, e.px)};
    if (auto it = e.fields.find("points"); it != e.fields.end() && it->is_array()) {
        bool first = true;
        for (auto& p : *it) {
            if (!p.is_object() || !p.contains("cx") || !p["cx"].is_number() || !p.contains("cy") || !p["cy"].is_number()) continue;
            const int cx = p["cx"].get<int>(), cy = p["cy"].get<int>();
            // the entity sits on points[0]: drop the duplicate
            const bool dup = first && int(std::floor(e.px.x / layerGridPx)) == cx && int(std::floor(e.px.y / layerGridPx)) == cy;
            first = false;
            if (!dup) pts.push_back(ldtkToWorld(levelWorldPx, glm::vec2(cx * layerGridPx, cy * layerGridPx)));
        }
    }
    // shortcut: an open terrain whose surface drops more than `thickness` self-intersects (the bottom
    // runs straight between the end points); upgrade by clipping the thickness offset under the surface.
    if (!closed && pts.size() >= 2) { // fill down by thickness
        const glm::vec2 last = pts.back(), first = pts.front();
        pts.push_back({last.x, last.y - thickness});
        pts.push_back({first.x, first.y - thickness});
    }
    if (pts.size() < 3) return {};

    float area = 0.0f; // shoelace; > 0 = counter-clockwise
    for (size_t i = 0; i < pts.size(); ++i) {
        const glm::vec2 &p = pts[i], &q = pts[(i + 1) % pts.size()];
        area += p.x * q.y - q.x * p.y;
    }
    if (area < 0.0f) std::reverse(pts.begin(), pts.end());

    std::vector<Segment> out;
    for (size_t i = 0; i < pts.size(); ++i) {
        const glm::vec2 a = pts[i], b = pts[(i + 1) % pts.size()];
        if (a == b) continue;
        Segment s = makeSegment(a, b, oneWay, roomIid);
        if (!oneWay || s.normal.y > 0.7f) out.push_back(std::move(s));
    }
    return out;
}

void CollisionWorld::addRoom(const std::string& roomIid, std::vector<Segment> segments) {
    removeRoom(roomIid);
    Room& room = rooms_[roomIid];
    room.segments = std::move(segments);
    constexpr float pad = 1e-3f; // zero-thickness boxes still overlap a query that crosses them
    for (const Segment& s : room.segments) {
        const std::uint32_t id = next_++;
        grid_.insert({id, 1}, {glm::min(s.a, s.b) - pad, glm::max(s.a, s.b) + pad});
        byId_[id] = &s;
        room.ids.push_back(id);
    }
}

void CollisionWorld::removeRoom(const std::string& roomIid) {
    auto it = rooms_.find(roomIid);
    if (it == rooms_.end()) return;
    for (std::uint32_t id : it->second.ids) { grid_.remove({id, 1}); byId_.erase(id); }
    rooms_.erase(it);
}

void CollisionWorld::query(const Collision2D::AABB& box, std::vector<const Segment*>& out) const {
    std::vector<GameObjectId> ids;
    grid_.query(box, ids);
    out.clear();
    for (GameObjectId id : ids) out.push_back(byId_.at(id.index));
}
