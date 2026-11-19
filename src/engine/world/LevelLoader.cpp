#include "engine/world/LevelLoader.hpp"

#include <cstdio>

void LevelLoader::loadRoom(const LdtkLevel& level, EngineServices& services, MaterialLibrary& materials) {
    constexpr float U = LdtkPixelsPerUnit;
    const int n = int(level.layers.size());
    auto toWorld = [&](glm::vec2 px) { return ldtkToWorld(level.worldPx, px); };
    // Entities with parallax show at their authored place when the camera is at their room's centre, as in LDtk.
    const glm::vec2 roomCentre = toWorld(glm::vec2(level.pxSize) * 0.5f);
    std::vector<StaticSprite>& sprites = rooms_[level.iid];
    sprites.clear(); // reload must not duplicate
    struct SpawnRoom { // reset even if a factory throws
        Scene2D& scene;
        ~SpawnRoom() { scene.spawnRoomIid.clear(); }
    } spawnRoom{services.scene};
    services.scene.spawnRoomIid = level.iid;

    for (int li = 0; li < n; ++li) {
        const LdtkLayer& L = level.layers[li];
        const int order = n - li; // higher order draws later, so the front layer wins ties

        if (!L.tilesetPath.empty() && !L.tiles.empty()) {
            Material mat = materials.load(level.baseDir / L.tilesetPath);
            const float g = float(L.gridSize);
            int layerIndex = 0;
            while (layerIndex < int(layers_.size()) && layers_[layerIndex].identifier != L.identifier) ++layerIndex;
            if (layerIndex == int(layers_.size())) layers_.push_back({L.identifier, {L.parallaxX, L.parallaxY}, L.depth});
            const LayerState& state = layers_[layerIndex]; // panel edits also reach rooms streamed in later
            for (const LdtkTile& t : L.tiles) {
                SpriteInstance s{};
                // Parallax tiles are anchored at the world origin (spec decision 6): seamless across rooms.
                s.position = toWorld(glm::vec2(t.px) + g * 0.5f);
                s.size = {g / U, g / U};
                float u = t.src.x / float(L.tilesetPx.x), v = t.src.y / float(L.tilesetPx.y);
                float w = g / L.tilesetPx.x, h = g / L.tilesetPx.y;
                if (t.flip & 1) s.size.x = -s.size.x;
                if (t.flip & 2) { v += h; h = -h; }
                s.uvRect = {u, v, w, h};
                s.color = {1, 1, 1, 1};
                s.depth = state.depth;
                s.parallax = state.parallax;
                s.albedo = mat.albedo; s.normal = mat.normal; s.emissive = mat.emissive; s.mask = mat.mask;
                s.flags = SpriteFlagNearest;
                sprites.push_back({s, order, layerIndex});
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
            f->second(e, ldtkParallaxAnchor(toWorld(e.px + e.size * 0.5f), e.parallax, roomCentre), services);
        }
    }
}

void LevelLoader::unloadRoom(const std::string& roomIid, Scene2D& scene) {
    rooms_.erase(roomIid);
    scene.flush(); // forEach only visits flushed objects
    scene.forEach([&](GameObject& o) {
        if (o.roomIid == roomIid && !o.persistent) scene.destroy(o.id);
    });
    scene.flush(); // gone now: no object outlives its room's tiles for a frame
}

void LevelLoader::setLayer(const std::string& identifier, glm::vec2 parallax, float depth) {
    for (int i = 0; i < int(layers_.size()); ++i) {
        LayerState& l = layers_[i];
        if (l.identifier != identifier || (l.parallax == parallax && l.depth == depth)) continue;
        l.parallax = parallax;
        l.depth = depth;
        for (auto& [iid, sprites] : rooms_) {
            for (StaticSprite& s : sprites) {
                if (s.layer != i) continue;
                s.instance.parallax = parallax;
                s.instance.depth = depth;
            }
        }
    }
}

void LevelLoader::appendStatic(SpriteBatch& batch) const {
    for (const auto& [iid, sprites] : rooms_) {
        for (const StaticSprite& s : sprites) batch.add(s.instance, s.order);
    }
}
