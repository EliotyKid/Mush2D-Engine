#include "engine/world/LevelLoader.hpp"

#include <cstdio>

void LevelLoader::load(const LdtkLevel& level, EngineServices& services, MaterialLibrary& materials) {
    constexpr float U = LdtkPixelsPerUnit;
    const float H = float(level.pxSize.y);
    const int n = int(level.layers.size());
    auto toWorld = [&](glm::vec2 px) { return glm::vec2(px.x / U, (H - px.y) / U); };
    const glm::vec2 centre = glm::vec2(level.pxSize) / (2.0f * U);
    sprites_.clear(); // reload must not duplicate
    layers_.clear();
    levelCentre_ = centre;

    for (int li = 0; li < n; ++li) {
        const LdtkLayer& L = level.layers[li];
        const int order = n - li; // higher order draws later, so the front layer wins ties

        if (!L.tilesetPath.empty() && !L.tiles.empty()) {
            Material mat = materials.load(level.baseDir / L.tilesetPath);
            const float g = float(L.gridSize);
            const glm::vec2 scroll{L.parallaxX, L.parallaxY};
            const int layerIndex = int(layers_.size());
            layers_.push_back({L.identifier, scroll, L.depth});
            for (const LdtkTile& t : L.tiles) {
                SpriteInstance s{};
                const glm::vec2 authored = toWorld(glm::vec2(t.px) + g * 0.5f);
                s.position = ldtkParallaxAnchor(authored, scroll, centre); // centre-based
                s.size = {g / U, g / U};
                float u = t.src.x / float(L.tilesetPx.x), v = t.src.y / float(L.tilesetPx.y);
                float w = g / L.tilesetPx.x, h = g / L.tilesetPx.y;
                if (t.flip & 1) s.size.x = -s.size.x;
                if (t.flip & 2) { v += h; h = -h; }
                s.uvRect = {u, v, w, h};
                s.color = {1, 1, 1, 1};
                s.depth = L.depth;
                s.parallax = {L.parallaxX, L.parallaxY};
                s.albedo = mat.albedo; s.normal = mat.normal; s.emissive = mat.emissive; s.mask = mat.mask;
                s.flags = SpriteFlagNearest;
                sprites_.push_back({s, order, layerIndex, authored});
            }
        }

        if (L.identifier == "Collision" && L.type == "IntGrid") {
            for (const IntRect& r : mergeIntGrid(L.intGrid, L.cells.x, L.cells.y, 1)) {
                GameObject& o = services.scene.createObject<GameObject>();
                o.name = "Collision";
                o.addTag("solid");
                const float g = float(L.gridSize);
                o.transform.position = toWorld({(r.x + r.w * 0.5f) * g, (r.y + r.h * 0.5f) * g});
                ColliderData c;
                c.size = {r.w * g / U, r.h * g / U};
                o.collider = c;
            }
        }

        for (const LdtkEntity& e : L.entities) {
            auto f = factories_.find(e.identifier);
            if (f == factories_.end()) {
                std::fprintf(stderr, "LDtk: no factory for entity '%s', ignored\n", e.identifier.c_str());
                continue;
            }
            f->second(e, ldtkParallaxAnchor(toWorld(e.px + e.size * 0.5f), e.parallax, centre), services);
        }
    }
}

void LevelLoader::setLayer(const std::string& identifier, glm::vec2 parallax, float depth) {
    for (int i = 0; i < int(layers_.size()); ++i) {
        LayerState& l = layers_[i];
        if (l.identifier != identifier || (l.parallax == parallax && l.depth == depth)) continue;
        l.parallax = parallax;
        l.depth = depth;
        for (StaticSprite& s : sprites_) {
            if (s.layer != i) continue;
            s.instance.position = ldtkParallaxAnchor(s.authoredPos, parallax, levelCentre_);
            s.instance.parallax = parallax;
            s.instance.depth = depth;
        }
    }
}

void LevelLoader::appendStatic(SpriteBatch& batch) const {
    for (const StaticSprite& s : sprites_) batch.add(s.instance, s.order);
}
