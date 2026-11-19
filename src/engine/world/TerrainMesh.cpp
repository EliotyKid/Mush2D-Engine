#include "TerrainMesh.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float Column = 0.5f;    // fill column width, u
constexpr float Lift = 0.0625f;   // edge pieces stick out of the line by this much
constexpr float Overlap = 0.06f;  // extra length at both ends of a segment, to cover the joints
constexpr float Eps = 1e-4f;

SpriteInstance makeSprite(const Material& m, glm::vec2 pos, glm::vec2 size, glm::vec4 uv, float rotation) {
    SpriteInstance s{};
    s.position = pos; s.size = size; s.uvRect = uv; s.color = {1, 1, 1, 1};
    s.rotation = rotation; s.parallax = {1, 1};
    s.albedo = m.albedo; s.normal = m.normal; s.emissive = m.emissive; s.mask = m.mask;
    s.flags = SpriteFlagNearest;
    return s;
}

// y of every polygon edge crossing the vertical line x, ascending.
std::vector<float> crossings(const std::vector<glm::vec2>& p, float x) {
    std::vector<float> ys;
    for (size_t i = 0; i < p.size(); ++i) {
        const glm::vec2 a = p[i], b = p[(i + 1) % p.size()];
        if ((a.x <= x) != (b.x <= x)) ys.push_back(a.y + (x - a.x) / (b.x - a.x) * (b.y - a.y));
    }
    std::sort(ys.begin(), ys.end());
    return ys;
}

} // namespace

void buildTerrainMesh(const std::vector<Segment>& polyline, bool closed, float thickness, const TerrainStyle& st,
                      std::vector<SpriteInstance>& out) {
    if (polyline.empty()) return;
    std::vector<glm::vec2> p;
    for (const Segment& s : polyline) p.push_back(s.a);
    if (!closed) {
        p.push_back(polyline.back().b);
        if (p.front().x > p.back().x) std::reverse(p.begin(), p.end());
        const glm::vec2 first = p.front(), last = p.back();
        p.push_back({last.x, last.y - thickness});
        p.push_back({first.x, first.y - thickness});
    }
    if (p.size() < 3) return;
    float area = 0.0f; // shoelace: > 0 = counter-clockwise, so the right-hand perpendicular of a->b points out
    for (size_t i = 0; i < p.size(); ++i) {
        const glm::vec2 &a = p[i], &b = p[(i + 1) % p.size()];
        area += a.x * b.y - b.x * a.y;
    }
    if (area < 0.0f) std::reverse(p.begin(), p.end());

    float minX = p[0].x, maxX = p[0].x;
    for (const glm::vec2& v : p) { minX = std::min(minX, v.x); maxX = std::max(maxX, v.x); }

    // Fill. The interval layout comes from the column's middle; each interval shrinks to the narrowest one
    // over the column's width, so the fill never pokes out of a slanted surface (the edge covers the gap).
    const glm::vec2 tile = st.fillTile;
    for (int c = int(std::floor(minX / Column)); c * Column < maxX; ++c) {
        const float x0 = std::max(c * Column, minX), x1 = std::min((c + 1) * Column, maxX);
        if (x1 - x0 < Eps) continue;
        const std::vector<float> mid = crossings(p, 0.5f * (x0 + x1));
        const std::vector<float> l = crossings(p, x0 + Eps), r = crossings(p, x1 - Eps);
        const bool same = l.size() == mid.size() && r.size() == mid.size();
        const float u0 = x0 / tile.x - std::floor(x0 / tile.x), du = (x1 - x0) / tile.x;
        for (size_t k = 0; k + 1 < mid.size(); k += 2) {
            float lo = mid[k], hi = mid[k + 1];
            if (same) { lo = std::max(l[k], r[k]); hi = std::min(l[k + 1], r[k + 1]); }
            for (float top = hi; top - lo > 1e-3f;) { // cells of one tile row, top-down
                const float rowTop = std::ceil(top / tile.y - Eps) * tile.y;
                const float bottom = std::max(rowTop - tile.y, lo);
                out.push_back(makeSprite(st.fill, {0.5f * (x0 + x1), 0.5f * (top + bottom)}, {x1 - x0, top - bottom},
                                         {u0, (rowTop - top) / tile.y, du, (top - bottom) / tile.y}, 0.0f));
                top = bottom;
            }
        }
    }

    // Edge: along each segment, from b towards a, so the sprite's up (left of its direction) is the normal.
    // A thin open shape (a branch) gets strips no deeper than itself: the texture's top rows, not squashed.
    const float depth = closed ? st.edgeSize.y : std::min(st.edgeSize.y, thickness + Lift);
    const glm::vec4 uv{0.0f, 0.0f, 1.0f, depth / st.edgeSize.y};
    for (size_t i = 0; i < p.size(); ++i) {
        const glm::vec2 a = p[i], b = p[(i + 1) % p.size()];
        const float len = glm::length(b - a);
        if (len < Eps) continue;
        const glm::vec2 n = glm::vec2(b.y - a.y, a.x - b.x) / len, t = (a - b) / len;
        const int count = std::max(1, int(std::ceil(len / st.edgeSize.x - Eps)));
        const float piece = (len + 2.0f * Overlap) / count, rot = std::atan2(t.y, t.x);
        for (int k = 0; k < count; ++k) {
            const glm::vec2 centre = b + t * (-Overlap + (k + 0.5f) * piece) - n * (0.5f * depth - Lift);
            out.push_back(makeSprite(n.y > 0.3f ? st.edge : st.side, centre, {piece, depth}, uv, rot));
        }
    }
}
