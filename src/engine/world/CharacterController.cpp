#include "CharacterController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <glm/gtc/constants.hpp>

namespace {

constexpr float kWalkCos = 0.6427876f;  // cos(50 deg): steeper than this is not ground
constexpr float kMaxStep = 0.1f;        // max displacement per sub-step
constexpr float kRadius = 0.1f;         // bottom corner rounding
constexpr float kSnap = 0.3f;           // ground snap distance when walking down
constexpr float kGroundTol = 0.02f;     // grounded probe, as the old stepPlatformer
constexpr float kDropTime = 0.25f;
constexpr float kOneWayTol = 0.01f;     // how far the base may sit under a one-way and still count as above
constexpr float kDepthEps = 1e-5f;      // smaller penetrations are contact, not overlap
constexpr float kSpanEps = 1e-4f;       // overlaps across the push axis thinner than this are ignored
constexpr int kArcSteps = 4;
constexpr int kIterations = 8;

using Poly = std::vector<glm::vec2>;

// Body outline around its centre, counter-clockwise, bottom corners rounded.
Poly bodyShape(glm::vec2 h) {
    const float r = std::min({kRadius, h.x, h.y});
    Poly p{{-h.x, h.y}};
    for (int corner = 0; corner < 2; ++corner) {
        const glm::vec2 c{corner ? h.x - r : -h.x + r, -h.y + r};
        for (int i = 0; i <= kArcSteps; ++i) {
            const float t = glm::pi<float>() * (1.0f + 0.5f * (float(corner) + float(i) / kArcSteps));
            p.push_back(c + r * glm::vec2(std::cos(t), std::sin(t)));
        }
    }
    p.push_back({h.x, h.y});
    return p;
}

// How far the body at `pos` must move along unit `dir` so that segment ab no longer cuts into it,
// measured only where the two overlap across `dir` (no vertex normals: the ghost vertex rule).
// Negative = free gap along -dir. nullopt when they do not overlap across `dir`.
std::optional<float> penetration(const Poly& P, glm::vec2 pos, glm::vec2 a, glm::vec2 b, glm::vec2 dir) {
    const glm::vec2 w{dir.y, -dir.x};
    float ua = glm::dot(a - pos, w), ub = glm::dot(b - pos, w), va = glm::dot(a - pos, dir), vb = glm::dot(b - pos, dir);
    if (std::abs(ub - ua) < 1e-6f) return std::nullopt; // segment parallel to dir
    if (ua > ub) { std::swap(ua, ub); std::swap(va, vb); }
    float pmin = std::numeric_limits<float>::max(), pmax = -pmin;
    for (glm::vec2 v : P) { const float u = glm::dot(v, w); pmin = std::min(pmin, u); pmax = std::max(pmax, u); }
    const float lo = std::max(ua, pmin), hi = std::min(ub, pmax);
    if (hi - lo <= kSpanEps) return std::nullopt;
    auto segV = [&](float u) { return va + (u - ua) / (ub - ua) * (vb - va); };
    auto low = [&](float u) { // lowest body point along dir at u
        float m = std::numeric_limits<float>::max();
        for (size_t i = 0; i < P.size(); ++i) {
            const glm::vec2 p = P[i], q = P[(i + 1) % P.size()];
            const float pu = glm::dot(p, w), qu = glm::dot(q, w), pv = glm::dot(p, dir), qv = glm::dot(q, dir);
            if (u < std::min(pu, qu) - 1e-6f || u > std::max(pu, qu) + 1e-6f) continue;
            m = std::min(m, std::abs(qu - pu) < 1e-9f ? std::min(pv, qv) : pv + (u - pu) / (qu - pu) * (qv - pv));
        }
        return m;
    };
    // segV - low is concave over the overlap: its max sits on an overlap end or a body vertex
    float best = std::max(segV(lo) - low(lo), segV(hi) - low(hi));
    for (glm::vec2 v : P) {
        const float u = glm::dot(v, w);
        if (u > lo && u < hi) best = std::max(best, segV(u) - glm::dot(v, dir));
    }
    return best;
}

struct Cand {
    const Segment* s;
    bool world;        // pointer stays valid after move() (CollisionWorld segment)
    bool startedAbove; // one-way rule 3: base above the segment at the start of the frame
};

glm::vec2 tangent(glm::vec2 n) { return {n.y, -n.x}; } // points right along a walkable surface

bool walkable(glm::vec2 n) { return n.y >= kWalkCos; }

bool isAbove(const Poly& P, glm::vec2 pos, const Segment& s) {
    const auto pen = penetration(P, pos, s.a, s.b, s.normal);
    return !pen || *pen <= kOneWayTol;
}

struct Support { const Cand* c; float gap; };

// Dropping through: every one-way is ignored, not only the one stood on (a Terrain branch is a chain of
// segments; standing on a vertex touches two).
bool dropping(const Body& body, const Segment& s) { return body.droppingThrough && s.oneWay; }

// Surfaces facing up under the body with a vertical gap in [-kGroundTol, maxGap].
std::vector<Support> supports(const std::vector<Cand>& cands, const Poly& P, glm::vec2 pos, float maxGap,
                              const Body& body) {
    std::vector<Support> out;
    for (const Cand& c : cands) {
        const Segment& s = *c.s;
        if (s.normal.y <= 1e-3f || dropping(body, s) || glm::dot(s.normal, pos - s.a) <= 0.0f) continue;
        if (s.oneWay && !isAbove(P, pos, s)) continue;
        const auto pen = penetration(P, pos, s.a, s.b, {0.0f, 1.0f});
        if (pen && -*pen >= -kGroundTol && -*pen <= maxGap) out.push_back({&c, -*pen});
    }
    return out;
}

} // namespace

MoveResult move(Body& body, glm::vec2 velocity, float dt, const CollisionQuery& query, bool wasGrounded,
                bool jumpedThisFrame) {
    if (body.droppingThrough && (body.dropThroughTimer -= dt) <= 0.0f) body.droppingThrough = nullptr;

    const Poly P = bodyShape(body.halfSize);
    glm::vec2 pos = body.position;

    // Broadphase over the whole frame: walking along a 50 deg slope is at most 1/cos(50) faster.
    const float reach = (std::abs(velocity.x) / kWalkCos + std::abs(velocity.y)) * dt + kSnap + 0.5f;
    const Collision2D::AABB box{pos - body.halfSize - reach, pos + body.halfSize + reach};
    std::vector<const Segment*> worldSegs;
    if (query.world) query.world->query(box, worldSegs);
    std::vector<Segment> boxSegs;
    boxSegs.reserve(query.solids.size() * 4);
    for (const Collision2D::AABB& s : query.solids) {
        if (s.max.x < box.min.x || s.min.x > box.max.x || s.max.y < box.min.y || s.min.y > box.max.y) continue;
        if (!(s.max.x > s.min.x && s.max.y > s.min.y)) continue; // degenerate: zero-length edges have no normal
        const glm::vec2 c[4] = {s.min, {s.max.x, s.min.y}, s.max, {s.min.x, s.max.y}}; // CCW: normals out
        for (int i = 0; i < 4; ++i) {
            const glm::vec2 d = c[(i + 1) % 4] - c[i];
            boxSegs.push_back({c[i], c[(i + 1) % 4], glm::normalize(glm::vec2(d.y, -d.x)), false, {}});
        }
    }
    std::vector<Cand> cands;
    for (const Segment* s : worldSegs) cands.push_back({s, true, !s->oneWay || isAbove(P, pos, *s)});
    for (const Segment& s : boxSegs) cands.push_back({&s, false, true});

    // Walking: move along the ground tangent at the flat horizontal speed, without gravity.
    const float inputX = velocity.x;
    bool groundedMode = false;
    glm::vec2 v = velocity;
    const auto sup = supports(cands, P, pos, kGroundTol, body);
    for (const Support& g : sup) // on a steep slope, input only pulls away
        if (!walkable(g.c->s->normal) && v.x * g.c->s->normal.x < 0.0f) v.x = 0.0f;
    if (wasGrounded && !jumpedThisFrame) {
        const Support* pick = nullptr;
        auto score = [&](const Support& g) { // prefer the surface rising most ahead: it never runs into the others
            const glm::vec2 n = g.c->s->normal;
            return inputX != 0.0f ? -n.x / n.y * (inputX > 0.0f ? 1.0f : -1.0f) : n.y;
        };
        for (const Support& g : sup)
            if (walkable(g.c->s->normal) && (!pick || score(g) > score(*pick))) pick = &g;
        if (pick) {
            const glm::vec2 n = pick->c->s->normal;
            v = tangent(n) * (inputX / n.y);
            groundedMode = true;
        }
    }

    MoveResult r;
    auto clip = [&](glm::vec2 n) {
        if (walkable(n)) {
            // v.x, not input: a wall that already stopped v.x must not be walked into again (wedge)
            // airborne landing keeps v.x (the walk then follows the slope), instead of deflecting sideways
            if (glm::dot(v, n) < 0.0f) v = groundedMode ? tangent(n) * (v.x / n.y) : glm::vec2(v.x, -v.x * n.x / n.y);
            return;
        }
        if (n.y >= 0.0f && v.x * n.x < 0.0f) v.x = 0.0f; // steep or wall: input only moves away from it
        if (glm::dot(v, n) < 0.0f) v -= glm::dot(v, n) * n;
        if (n.y < -0.5f) r.hitCeiling = true;
    };
    auto blocks = [&](const Cand& c) {
        const Segment& s = *c.s;
        if (dropping(body, s) || glm::dot(s.normal, pos - s.a) <= 0.0f) return false; // centre behind: not ours
        return !s.oneWay || (s.normal.y > 0.0f && glm::dot(v, s.normal) <= 0.0f && c.startedAbove);
    };

    const int steps = std::max(1, int(std::ceil(glm::length(v) * dt / kMaxStep)));
    const float h = dt / float(steps);
    for (int step = 0; step < steps; ++step) {
        pos += v * h;
        // Push out of the shallowest penetration first, along that segment's own normal, and repeat:
        // a wall touched by a few hundredths wins over the top of the block behind it.
        for (int it = 0; it < kIterations; ++it) {
            const Cand* best = nullptr;
            float depth = 0.0f;
            for (const Cand& c : cands) {
                if (!blocks(c)) continue;
                const auto pen = penetration(P, pos, c.s->a, c.s->b, c.s->normal);
                if (pen && *pen > kDepthEps && (!best || *pen < depth)) { best = &c; depth = *pen; }
            }
            if (!best) break;
            pos += best->s->normal * depth;
            clip(best->s->normal);
        }
    }

    // Snap down to ground when walking off a slope top or down a ramp.
    if (wasGrounded && !jumpedThisFrame) {
        const auto below = supports(cands, P, pos, kSnap, body);
        const Support* nearest = nullptr;
        for (const Support& g : below)
            if (!nearest || g.gap < nearest->gap) nearest = &g;
        if (nearest && walkable(nearest->c->s->normal) && nearest->gap > 0.0f) pos.y -= nearest->gap;
    }

    // Ground and steep contact at the end position.
    const Support *ground = nullptr, *steep = nullptr, *oneWay = nullptr;
    bool solidGround = false;
    const auto touching = supports(cands, P, pos, kGroundTol, body);
    for (const Support& g : touching) {
        const glm::vec2 n = g.c->s->normal;
        if (!walkable(n)) { if (!steep || n.y > steep->c->s->normal.y) steep = &g; continue; }
        if (!ground || n.y > ground->c->s->normal.y) ground = &g;
        if (g.c->s->oneWay) oneWay = &g; else solidGround = true;
    }
    for (const Cand& c : cands) // the base passed below the segment being dropped through
        if (c.s == body.droppingThrough) {
            const auto pen = penetration(P, pos, c.s->a, c.s->b, c.s->normal);
            if (pen && *pen > kOneWayTol) body.droppingThrough = nullptr;
        }

    r.grounded = ground != nullptr;
    r.onSteep = !ground && steep;
    if (ground) r.groundNormal = ground->c->s->normal;
    else if (steep) r.groundNormal = steep->c->s->normal;
    r.onOneWay = oneWay && !solidGround;
    if (r.onOneWay && oneWay->c->world) r.groundSegment = oneWay->c->s;
    if (r.grounded) v.y = 0.0f;
    body.position = r.position = pos;
    r.velocity = v;
    return r;
}

bool requestDropThrough(Body& body, const MoveResult& last) {
    if (!last.grounded || !last.onOneWay || !last.groundSegment) return false;
    body.droppingThrough = last.groundSegment;
    body.dropThroughTimer = kDropTime;
    return true;
}

namespace {
// Segment ab touches the closed box [lo, hi] (Liang-Barsky clip).
bool touchesBox(glm::vec2 a, glm::vec2 b, glm::vec2 lo, glm::vec2 hi) {
    float t0 = 0.0f, t1 = 1.0f;
    const glm::vec2 d = b - a;
    for (int axis = 0; axis < 2; ++axis) {
        if (std::abs(d[axis]) < 1e-9f) {
            if (a[axis] < lo[axis] || a[axis] > hi[axis]) return false;
            continue;
        }
        float ta = (lo[axis] - a[axis]) / d[axis], tb = (hi[axis] - a[axis]) / d[axis];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    return true;
}
} // namespace

bool depenetrate(Body& body, const CollisionQuery& query, float maxPush) {
    constexpr float kSkin = 0.035f, kStep = 0.05f; // resting contact is not overlap; skin covers the rounded corner a square box overlaps on a 45 deg ramp
    const glm::vec2 h = body.halfSize - kSkin;
    const glm::vec2 reach = body.halfSize + maxPush + 1.0f;
    std::vector<const Segment*> segs;
    if (query.world) query.world->query({body.position - reach, body.position + reach}, segs);
    std::erase_if(segs, [](const Segment* s) { return s->oneWay; }); // one-ways never trap a body

    auto isFree = [&](glm::vec2 p) {
        for (const Collision2D::AABB& s : query.solids)
            if (s.min.x < p.x + h.x && s.max.x > p.x - h.x && s.min.y < p.y + h.y && s.max.y > p.y - h.y) return false;
        const Segment* above = nullptr; // first surface straight above the centre
        float aboveY = std::numeric_limits<float>::max();
        for (const Segment* s : segs) {
            if (touchesBox(s->a, s->b, p - h, p + h)) return false;
            const float x0 = std::min(s->a.x, s->b.x), x1 = std::max(s->a.x, s->b.x);
            if (x1 - x0 < 1e-6f || p.x < x0 || p.x > x1) continue;
            const float y = s->a.y + (p.x - s->a.x) / (s->b.x - s->a.x) * (s->b.y - s->a.y);
            if (y > p.y && y < aboveY) { aboveY = y; above = s; }
        }
        return !above || above->normal.y < 0.0f; // a top facing up over the centre: buried inside a solid
    };

    const glm::vec2 dirs[] = {{0, 1}, {-0.7071f, 0.7071f}, {0.7071f, 0.7071f}, {-1, 0}, {1, 0},
                              {-0.7071f, -0.7071f}, {0.7071f, -0.7071f}, {0, -1}}; // up first
    if (isFree(body.position)) return true;
    for (float d = kStep; d <= maxPush + 1e-4f; d += kStep)
        for (glm::vec2 dir : dirs)
            if (isFree(body.position + dir * d)) {
                body.position += dir * d;
                return true;
            }
    return false;
}

namespace {
// Segments facing the body (accepted by `faces`) or dynamic solids touching the box [lo, hi].
SurfaceHit probeBox(const Body& body, glm::vec2 lo, glm::vec2 hi, const CollisionQuery& query, bool (*faces)(glm::vec2, int),
                    int side) {
    SurfaceHit r;
    for (const Collision2D::AABB& s : query.solids)
        if (s.min.x <= hi.x && s.max.x >= lo.x && s.min.y <= hi.y && s.max.y >= lo.y) r.hit = true;
    std::vector<const Segment*> segs;
    if (query.world) query.world->query({lo, hi}, segs);
    for (const Segment* s : segs) {
        if (s->oneWay || !faces(s->normal, side) || glm::dot(s->normal, body.position - s->a) <= 0.0f) continue;
        if (!touchesBox(s->a, s->b, lo, hi)) continue;
        r.hit = true;
        if (s->surface == Surface::IceClimb) r.surface = Surface::IceClimb;
    }
    return r;
}
constexpr float kProbeInset = 0.01f; // the box stops short of the body's corners: the floor or the wall end is not a hit
} // namespace

SurfaceHit probeWall(const Body& body, int side, const CollisionQuery& query, float reach) {
    const glm::vec2 h = body.halfSize;
    const float x = body.position.x + float(side) * h.x;
    const glm::vec2 lo{std::min(x, x + side * reach), body.position.y - h.y + kProbeInset};
    const glm::vec2 hi{std::max(x, x + side * reach), body.position.y + h.y - kProbeInset};
    return probeBox(body, lo, hi, query,
                    [](glm::vec2 n, int s) { return n.x * float(s) < 0.0f && n.y > -0.5f && !walkable(n); }, side);
}

SurfaceHit probeCeiling(const Body& body, const CollisionQuery& query, float reach) {
    const glm::vec2 h = body.halfSize;
    const glm::vec2 lo{body.position.x - h.x + kProbeInset, body.position.y + h.y};
    const glm::vec2 hi{body.position.x + h.x - kProbeInset, body.position.y + h.y + reach};
    return probeBox(body, lo, hi, query, [](glm::vec2 n, int) { return n.y < -0.5f; }, 0);
}
