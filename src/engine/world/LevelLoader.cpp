#include "engine/world/LevelLoader.hpp"

#include "engine/world/PersistentComponent.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/world/TerrainMesh.hpp"
#include "engine/world/World.hpp"

#include <cstdio>
#include <set>

namespace {
struct SpawnRoom { // reset even if a factory throws
    Scene2D& scene;
    SpawnRoom(Scene2D& s, const std::string& iid) : scene(s) { scene.spawnRoomIid = iid; }
    ~SpawnRoom() { scene.spawnRoomIid.clear(); }
};
}

std::filesystem::path terrainStyleDir(const std::filesystem::path& terrainRoot, const std::string& style) {
    if (std::filesystem::exists(terrainRoot / style / "edge.png")) return terrainRoot / style;
    static std::set<std::string> warned;
    if (warned.insert(style).second)
        std::fprintf(stderr, "Terrain: style '%s' has no textures in %s, using 'forest'\n", style.c_str(),
                     terrainRoot.string().c_str());
    return terrainRoot / "forest";
}

// The terrain's look: assets/terrain/<style>/{edge,side,fill}.png next to the levels folder; style = entity
// field "style" (default "forest"). Texture pixels / 32 = world size.
void LevelLoader::appendTerrainSprites(const LdtkEntity& e, const LdtkLevel& level, int layerGridPx, int order,
                                       MaterialLibrary& materials, std::vector<StaticSprite>& sprites) {
    const TerrainShape shape = terrainShape(e, level.worldPx, layerGridPx);
    std::string name = "forest";
    if (auto it = e.fields.find("style"); it != e.fields.end() && it->is_string()) name = it->get<std::string>();
    const std::filesystem::path dir = terrainStyleDir(level.baseDir.parent_path() / "terrain", name);
    TerrainStyle style;
    style.edge = materials.load(dir / "edge.png");
    style.side = materials.load(dir / "side.png");
    style.fill = materials.load(dir / "fill.png");
    style.edgeSize = {style.edge.width / LdtkPixelsPerUnit, style.edge.height / LdtkPixelsPerUnit};
    style.fillTile = {style.fill.width / LdtkPixelsPerUnit, style.fill.height / LdtkPixelsPerUnit};

    std::vector<Segment> line;
    const std::vector<glm::vec2>& p = shape.points;
    for (size_t i = 0; i + (shape.closed ? 0 : 1) < p.size(); ++i) {
        Segment seg;
        seg.a = p[i]; seg.b = p[(i + 1) % p.size()];
        line.push_back(seg);
    }
    std::vector<SpriteInstance> mesh;
    buildTerrainMesh(line, shape.closed, shape.thickness, style, mesh);
    for (SpriteInstance& s : mesh) {
        s.depth = e.depth;
        s.parallax = e.parallax;
        sprites.push_back({s, order, -1}); // -1: not an editable tile layer
    }
}

void LevelLoader::loadRoom(const LdtkLevel& level, EngineServices& services, MaterialLibrary& materials) {
    constexpr float U = LdtkPixelsPerUnit;
    const int n = int(level.layers.size());
    auto toWorld = [&](glm::vec2 px) { return ldtkToWorld(level.worldPx, px); };
    std::vector<StaticSprite>& sprites = rooms_[level.iid];
    sprites.clear(); // reload must not duplicate
    SpawnRoom spawnRoom{services.scene, level.iid};
    std::vector<Segment> segments;

    for (int li = 0; li < n; ++li) {
        const LdtkLayer& L = level.layers[li];
        const int order = n - li; // higher order draws later, so the front layer wins ties

        if (!L.tilesetPath.empty() && !L.tiles.empty()) {
            Material mat = materials.load(level.baseDir / L.tilesetPath);
            const float g = float(L.gridSize);
            int layerIndex = 0;
            while (layerIndex < int(layers_.size()) &&
                   (layers_[layerIndex].region != level.region || layers_[layerIndex].identifier != L.identifier))
                ++layerIndex;
            if (layerIndex == int(layers_.size()))
                layers_.push_back({level.region, L.identifier, {L.parallaxX, L.parallaxY}, L.depth});
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
            std::vector<Segment> s = segmentsFromIntGrid(L, level.worldPx, level.pxSize, level.iid);
            segments.insert(segments.end(), s.begin(), s.end());
        }
        for (const LdtkEntity& e : L.entities) {
            if (e.identifier != "Terrain") continue;
            std::vector<Segment> s = segmentsFromTerrain(e, level.worldPx, L.gridSize, level.iid);
            segments.insert(segments.end(), s.begin(), s.end());
            appendTerrainSprites(e, level, L.gridSize, order, materials, sprites);
        }
    }
    services.scene.collision.addRoom(level.iid, std::move(segments));
}

void LevelLoader::spawnEntity(const LdtkLevel& level, const LdtkEntity& e, EngineServices& services) {
    if (e.identifier == "Terrain") return; // collision only, built by loadRoom
    // Not game objects: always their code factory, even if a prefab file of that name exists.
    const bool codeOnly = e.identifier == "Light" || e.identifier == "PlayerStart";
    PrefabLibrary* prefabs = services.scene.service<PrefabLibrary>();
    const std::string prefabName = stringField(e, "prefab", snakeCase(e.identifier));
    const bool usePrefab = !codeOnly && prefabs && prefabs->has(prefabName);
    auto f = factories_.find(e.identifier);
    if (!codeOnly && prefabs && e.fields.contains("prefab") && !prefabs->has(prefabName) &&
        warnedEntities_.insert("prefab:" + prefabName).second) {
        warnings_.push_back("LDtk: entity '" + e.identifier + "' names prefab '" + prefabName + "', but " + prefabName +
                            ".prefab.json does not exist");
        std::fprintf(stderr, "%s\n", warnings_.back().c_str());
    }
    if (!usePrefab && f == factories_.end()) {
        if (warnedEntities_.insert(e.identifier).second) {
            warnings_.push_back("LDtk: no prefab (" + prefabName + ".prefab.json) and no factory for entity '" +
                                e.identifier + "', ignored");
            std::fprintf(stderr, "%s\n", warnings_.back().c_str());
        }
        return;
    }
    SpawnRoom spawnRoom{services.scene, level.iid};
    auto toWorld = [&](glm::vec2 px) { return ldtkToWorld(level.worldPx, px); };
    // Entities with parallax show at their authored place when the camera is at their room's centre, as in LDtk.
    const glm::vec2 roomCentre = toWorld(glm::vec2(level.pxSize) * 0.5f);
    const glm::vec2 pos = ldtkParallaxAnchor(toWorld(e.px + e.size * 0.5f), e.parallax, roomCentre);
    if (!usePrefab) {
        f->second(e, pos, services);
        return;
    }
    // position = the entity box centre (what factories get); ldtkSize = the box in world units (to extra()
    // unless the class exports it). Both count as LDtk values.
    nlohmann::json fields = e.fields.is_object() ? e.fields : nlohmann::json::object();
    fields.erase("prefab");
    fields["position"] = {pos.x, pos.y};
    fields["ldtkSize"] = {e.size.x / LdtkPixelsPerUnit, e.size.y / LdtkPixelsPerUnit};
    GameObject* o = prefabs->instantiate(services.scene, prefabName, fields, e.iid);
    if (!o) return;
    if (o->sprite) o->sprite->parallax = e.parallax; // drawn with its LDtk layer
    // As the old factories: persistence from the entity's fields, recorded where the object now stands.
    if (parsePersistence(e) != Persistence::None || o->getComponent<PersistentComponent>())
        trackEntity(*o, e, o->transform.position);
    // A policy set only by the prefab: World must know it too, or a Respawn object never comes back.
    // None (no component): forgets a policy an older version of the prefab gave this entity (hot reload).
    if (World* world = services.scene.service<World>()) {
        const auto* p = o->getComponent<PersistentComponent>();
        world->track(e.iid, world->roomOf(level.iid), p ? p->policy : Persistence::None, p ? p->respawnDistance : 0.0f);
    }
}

void LevelLoader::unloadRoom(const std::string& roomIid, Scene2D& scene) {
    rooms_.erase(roomIid);
    scene.collision.removeRoom(roomIid);
    scene.flush(); // forEach only visits flushed objects
    scene.forEach([&](GameObject& o) {
        if (o.roomIid == roomIid && !o.persistent) scene.destroy(o.id);
    });
    scene.flush(); // gone now: no object outlives its room's tiles for a frame
}

void LevelLoader::setLayer(const std::string& region, const std::string& identifier, glm::vec2 parallax, float depth) {
    for (int i = 0; i < int(layers_.size()); ++i) {
        LayerState& l = layers_[i];
        if (l.region != region || l.identifier != identifier || (l.parallax == parallax && l.depth == depth)) continue;
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
