#include "engine/assets/AtlasLoader.hpp"
#include "engine/core/Paths.hpp"
#include "engine/render/Materials.hpp"
#include "engine/render/PixelScale.hpp"
#include "engine/render/SceneRenderer.hpp"
#include "engine/tools/LookFile.hpp"
#include "engine/world/Ldtk.hpp"
#include "engine/world/Scene2D.hpp"

#include <filesystem>
#include <fstream>

#include <cstdio>
#include <exception>
#include <functional>
#include <utility>
#include <vector>

namespace {
std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> r;
    return r;
}
int failures = 0;
struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().emplace_back(name, std::move(fn)); }
};
}

#define CHECK(expr) do { if (!(expr)) { std::printf("  CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); ++failures; } } while (0)
#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()

TEST(asset_path_is_next_to_executable) {
    auto p = assetPath("assets/levels/x.ldtk");
    CHECK(p.is_absolute());
    CHECK(p.filename() == "x.ldtk");
}

TEST(material_paths_found_when_present) {
    auto dir = std::filesystem::temp_directory_path() / "mush2d_mat";
    std::filesystem::create_directories(dir);
    for (auto n : {"a.png", "a_n.png", "a_e.png"}) std::ofstream(dir / n) << "x";
    auto m = resolveMaterialPaths(dir / "a.png");
    CHECK(m.normal && m.normal->filename() == "a_n.png");
    CHECK(m.emissive && m.emissive->filename() == "a_e.png");
    CHECK(!m.mask);
}
TEST(material_mismatched_size_falls_back) {
    CHECK(mapMatchesAlbedo(256, 128, 256, 128));
    CHECK(!mapMatchesAlbedo(256, 128, 128, 128));
}

TEST(pixel_scale_integer) {
    CHECK(computePixelScale(1280) == 3);
    CHECK(computePixelScale(1920) == 4);
    CHECK(computePixelScale(800) == 2);
    CHECK(computePixelScale(300) == 1);
}
TEST(light_list_clamps_to_64) {
    LightList l; for (int i = 0; i < 64; ++i) CHECK(l.add({}));
    CHECK(!l.add({})); CHECK(l.size() == 64);
}
TEST(sprite_instance_layout) { CHECK(sizeof(SpriteInstance) == 96); CHECK(sizeof(GpuLight) == 48); }
TEST(sprite_batch_additive_after_opaque) {
    SpriteBatch b;
    SpriteInstance a{}, o{};
    a.flags = SpriteFlagAdditive; a.rotation = 1; a.depth = 5; // far, but additive
    o.rotation = 2; o.depth = 0;
    b.add(a); b.add(o);
    const auto& s = b.sorted();
    CHECK(b.opaqueCount() == 1 && s[0].rotation == 2 && s[1].rotation == 1);
}
TEST(sprite_batch_sorts_back_to_front) {
    SpriteBatch b;
    auto inst = [](float depth, float tag) { SpriteInstance s{}; s.depth = depth; s.rotation = tag; return s; };
    b.add(inst(0, 1), 5);
    b.add(inst(3, 2), 9);
    b.add(inst(0, 3), 1);
    b.add(inst(0, 4), 1); // same key as tag 3: insertion order kept
    const auto& s = b.sorted();
    CHECK(s.size() == 4);
    CHECK(s[0].rotation == 2 && s[1].rotation == 3 && s[2].rotation == 4 && s[3].rotation == 1);
}

TEST(intgrid_merges_rects) {
    // 4x2: row 0 = 1 1 0 1, row 1 = 1 1 0 0
    auto r = mergeIntGrid({1,1,0,1, 1,1,0,0}, 4, 2, 1);
    CHECK(r.size() == 2);
    CHECK((r[0].x == 0 && r[0].y == 0 && r[0].w == 2 && r[0].h == 2));
    CHECK((r[1].x == 3 && r[1].y == 0 && r[1].w == 1 && r[1].h == 1));
}
TEST(layer_depth_from_suffix) {
    CHECK(parseLayerDepth("Mountains_d12", 0, 0) == 12.0f);
    CHECK(parseLayerDepth("Foreground_d-2", 0, 0) == -2.0f);
    CHECK(parseLayerDepth("Plain", 3, 1) == 2.0f);   // behind gameplay = far
    CHECK(parseLayerDepth("Plain", 0, 1) == -1.0f);  // in front
    CHECK(parseLayerDepth("Bad_data", 2, 2) == 0.0f);
}
TEST(ldtk_parallax_convention) {
    CHECK(ldtkParallaxToScroll(0.0f) == 1.0f);  // LDtk 0 = scrolls with the level
    CHECK(ldtkParallaxToScroll(1.0f) == 0.0f);  // LDtk 1 = fixed on screen
    CHECK(ldtkParallaxToScroll(0.25f) == 0.75f);
    CHECK(ldtkParallaxToScroll(-0.5f) == 1.5f); // negative = foreground, faster
}
TEST(ldtk_parses_mini) {
    auto lv = loadLdtkLevel(MUSH2D_TEST_DATA "/mini.ldtk", "Level_0");
    CHECK(lv.layers.size() == 3);                        // Entities, Collision, BG_d6
    CHECK(lv.layers[0].entities[0].identifier == "PlayerStart");
    CHECK(lv.layers[0].entities[0].px == glm::vec2(16, 16)); // pivot (0.5,1) removed
    CHECK(lv.layers[0].entities[0].fields["speed"] == 3);
    CHECK(lv.layers[1].intGrid.size() == 8);
    CHECK(lv.layers[2].depth == 6.0f);
    CHECK(lv.layers[2].parallaxX == 0.5f && lv.layers[2].parallaxY == 1.0f);
    CHECK(lv.layers[2].tiles.size() == 1 && lv.layers[2].tiles[0].flip == 1);
    CHECK(lv.layers[2].tilesetPx == glm::ivec2(64, 32));
    CHECK(lv.layers[0].depth == -1.0f);                  // in front of Collision
}
TEST(ldtk_tolerates_unknowns) {
    // mini2.ldtk: AutoLayer with 2 autoLayerTiles, Tiles layer without tileset, entity "Mystery"
    auto lv = loadLdtkLevel(MUSH2D_TEST_DATA "/mini2.ldtk", "Level_0");
    CHECK(lv.layers.size() == 2);                        // layer without tileset skipped
    CHECK(lv.layers[1].tiles.size() == 2);               // AutoLayer
    CHECK(lv.layers[0].entities[0].identifier == "Mystery");
}

TEST(ldtk_malformed_throws) {
    bool threw = false;
    try { loadLdtkLevel(MUSH2D_TEST_DATA "/bad.ldtk", "Level_0"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { loadLdtkLevel(MUSH2D_TEST_DATA "/bad_types.ldtk", "Level_0"); } catch (const std::exception&) { threw = true; }
    CHECK(threw); // valid JSON, wrong types: nlohmann type_error
}

TEST(intgrid_short_csv_is_ignored) {
    CHECK(mergeIntGrid({1, 1, 1}, 2, 2, 1).empty()); // 3 cells for a 2x2 grid
}
TEST(ldtk_parallax_anchor_is_level_centre) {
    const glm::vec2 centre{24.0f, 8.0f}, authored{10.0f, 3.0f}, scroll{0.25f, 0.5f};
    glm::vec2 p = ldtkParallaxAnchor(authored, scroll, centre);
    // Shader: screen = pos - camera * scroll. Camera at the level centre -> exactly where LDtk shows it.
    CHECK(glm::all(glm::lessThan(glm::abs((p - centre * scroll) - (authored - centre)), glm::vec2(1e-5f))));
    CHECK(ldtkParallaxAnchor(authored, {1.0f, 1.0f}, centre) == authored); // gameplay layer untouched
}
TEST(atlas_keeps_file_order_and_pivot) {
    // nlohmann::json would sort keys ("idle 10" < "idle 2" < "run 0"); frameTags index the file order.
    auto path = std::filesystem::temp_directory_path() / "mush2d_atlas.json";
    std::ofstream(path) << R"({"frames": {
        "run 0": {"frame": {"x": 0, "y": 0, "w": 4, "h": 6}, "pivot": {"x": 2, "y": 6}},
        "idle 0": {"frame": {"x": 4, "y": 0, "w": 4, "h": 6}},
        "idle 1": {"frame": {"x": 8, "y": 0, "w": 4, "h": 6}}},
        "meta": {"size": {"w": 12, "h": 6}, "frameTags": [
            {"name": "run", "from": 0, "to": 0}, {"name": "idle", "from": 1, "to": 2}]}})";
    TextureAtlas atlas = AtlasLoader::loadFromAsepriteJsonHash(path.string(), "");
    CHECK(atlas.getAnimation("run").frames.size() == 1 && atlas.getAnimation("run").frames[0].frameName == "run 0");
    CHECK(atlas.getAnimation("idle").frames.size() == 2 && atlas.getAnimation("idle").frames[1].frameName == "idle 1");
    CHECK(atlas.getFrame("run 0").pivotX == 2 && atlas.getFrame("run 0").pivotY == 6);
    CHECK(atlas.getFrame("idle 0").pivotX == 2 && atlas.getFrame("idle 0").pivotY == 3); // default: centre
}

TEST(look_roundtrip) {
    LookFile a; a.settings.fogDensity = 0.2f; a.lights["abc"] = {{1, 0.5f, 0}, 3, 6, 1.5f, 0, 0.1f};
    a.layers["Far"] = {{0.25f, 0.5f}, 3.0f};
    auto b = parseLook(serializeLook(a), FrameSettings{});
    CHECK(b && b->settings.fogDensity == 0.2f && b->lights.at("abc").radius == 6);
    CHECK(b && b->lights.at("abc").color.y == 0.5f && b->layers.at("Far").parallax.y == 0.5f && b->layers.at("Far").depth == 3.0f);
}
TEST(look_ignores_unknown_and_corrupt) {
    CHECK(!parseLook("{ not json", FrameSettings{}));
    CHECK(!parseLook("[1,2]", FrameSettings{}));
    auto l = parseLook(R"({"exposure":1.5,"futureKey":3})", FrameSettings{});
    CHECK(l && l->settings.exposure == 1.5f && l->settings.fogDensity == FrameSettings{}.fogDensity);
}
TEST(look_apply_skips_stale_ids) {
    LookFile look; look.lights["gone"] = {{1, 1, 1}, 9, 9, 9, 9, 9}; look.lights["here"] = {{1, 1, 1}, 4, 5, 6, 7, 8};
    look.layers["Nope"] = {{0, 0}, 1};
    std::vector<EditableLight> lights{{"here", {0, 0}, {{0, 0, 0}, 1, 1, 1, 1, 1}}};
    std::vector<EditableLayer> layers{{"Far", {{1, 1}, 0}}};
    applyLook(look, lights, layers);
    CHECK(lights.size() == 1 && lights[0].value.radius == 5 && layers[0].value.depth == 0);
    LookFile out; captureLook(out, lights, layers);
    CHECK(out.lights.size() == 1 && out.layers.at("Far").parallax.x == 1);
}

// --- Entity model (Scene2D) ---
namespace {
int g_frame = 0;

struct Counted : GameObject {
    int steps = 0, destroys = 0, firstStepFrame = -1, createdFrame = -1;
    std::vector<int>* log = nullptr; int tag = 0;
    void onCreate(Scene2D&) override { createdFrame = g_frame; }
    void onStep(Scene2D&, float) override {
        if (firstStepFrame < 0) firstStepFrame = g_frame;
        ++steps;
        if (log) log->push_back(tag);
    }
    void onDestroy(Scene2D&) override { ++destroys; }
};
struct Spawner : GameObject {
    std::vector<GameObjectId> spawned;
    void onStep(Scene2D& scene, float) override { spawned.push_back(scene.createObject<Counted>().id); }
};
struct ChainDestroy : Counted {
    GameObjectId other; GameObjectId* spawnedOut = nullptr;
    void onDestroy(Scene2D& scene) override {
        Counted::onDestroy(scene);
        scene.destroy(other); scene.destroy(id);
        if (spawnedOut) *spawnedOut = scene.createObject<Counted>().id;
    }
};
}

TEST(spawn_during_step_is_safe) {
    Scene2D scene; g_frame = 0;
    auto& spawner = scene.createObject<Spawner>();
    for (g_frame = 0; g_frame < 100; ++g_frame) { scene.step(0.016f); scene.lateStep(0.016f); }
    CHECK(spawner.spawned.size() == 100);
    CHECK(scene.aliveCount() == 101);
    bool ok = true;
    for (GameObjectId id : spawner.spawned) {
        auto* c = scene.findObjectAs<Counted>(id);
        if (!c) { ok = false; continue; }
        if (c->createdFrame < 99 && c->firstStepFrame != c->createdFrame + 1) ok = false;
        if (c->createdFrame == 99 && c->steps != 0) ok = false;
    }
    CHECK(ok);
}

TEST(destroy_during_step_is_deferred) {
    Scene2D scene;
    static int killerDestroys = 0, victimDestroys = 0, victimSteps = 0;
    killerDestroys = victimDestroys = victimSteps = 0;
    struct K : GameObject { GameObjectId other;
        void onStep(Scene2D& s, float) override { s.destroy(id); s.destroy(other); s.destroy(id); } // double destroy: no-op
        void onDestroy(Scene2D&) override { ++killerDestroys; } };
    struct V : GameObject {
        void onStep(Scene2D&, float) override { ++victimSteps; }
        void onDestroy(Scene2D& s) override { ++victimDestroys; s.destroy(id); } };
    struct Probe : GameObject { // runs last: both still resolve, flagged pending
        GameObjectId k, v; bool sawPending = false;
        void onStep(Scene2D& s, float) override {
            sawPending = s.find(k) && s.find(k)->pendingDestroy() && s.find(v) && s.find(v)->pendingDestroy();
        } };
    auto& killer = scene.createObject<K>();
    GameObjectId k = killer.id, v = scene.createObject<V>().id;
    killer.other = v;
    auto& probe = scene.createObject<Probe>(); probe.updateOrder = 10; probe.k = k; probe.v = v;
    scene.step(0.016f);
    CHECK(probe.sawPending);
    CHECK(scene.find(k) == nullptr);
    CHECK(scene.find(v) == nullptr);
    for (int i = 0; i < 3; ++i) { scene.step(0.016f); scene.lateStep(0.016f); }
    CHECK(killerDestroys == 1);
    CHECK(victimDestroys == 1);
    CHECK(victimSteps == 0); // marked earlier in the same frame, so no callback
    CHECK(scene.aliveCount() == 1);
}

TEST(stale_handle_returns_null) {
    Scene2D scene;
    GameObjectId a = scene.createObject<Counted>().id;
    CHECK(a && scene.find(a));
    scene.destroy(a); scene.flush();
    GameObjectId b = scene.createObject<Counted>().id;
    CHECK(b.index == a.index); // slot reused
    CHECK(!(a == b));
    CHECK(scene.find(a) == nullptr);
    CHECK(scene.find(b) != nullptr);
    CHECK(scene.find(InvalidGameObjectId) == nullptr);
    CHECK(!InvalidGameObjectId);
    CHECK(std::hash<GameObjectId>{}(a) != std::hash<GameObjectId>{}(b));
}

TEST(clear_invalidates_old_handles) {
    Scene2D scene;
    GameObjectId a = scene.createObject<Counted>().id;
    scene.createObject<Counted>();
    scene.step(0.016f);
    scene.clear();
    CHECK(scene.aliveCount() == 0);
    GameObjectId b = scene.createObject<Counted>().id;
    CHECK(scene.find(a) == nullptr);
    CHECK(scene.find(b) != nullptr);
    CHECK(!(a == b));
}

TEST(update_order_respected) {
    Scene2D scene;
    std::vector<int> log;
    int orders[] = {5, -1, 0, 0};
    std::vector<Counted*> objs;
    for (int i = 0; i < 4; ++i) {
        auto& c = scene.createObject<Counted>(); c.updateOrder = orders[i]; c.tag = i; c.log = &log; objs.push_back(&c);
    }
    scene.step(0.016f);
    CHECK((log == std::vector<int>{1, 2, 3, 0}));
    std::vector<int> visited;
    scene.forEach([&](GameObject& o) { visited.push_back(static_cast<Counted&>(o).tag); });
    CHECK((visited == std::vector<int>{1, 2, 3, 0}));
    log.clear();
    objs[0]->updateOrder = -5; // takes effect at the next flush (start of step)
    scene.step(0.016f);
    CHECK((log == std::vector<int>{0, 1, 2, 3}));
}

TEST(destroy_inside_onDestroy_is_safe) {
    Scene2D scene;
    auto& a = scene.createObject<ChainDestroy>();
    auto& b = scene.createObject<ChainDestroy>();
    GameObjectId spawned;
    a.other = b.id; b.other = a.id; a.spawnedOut = &spawned;
    GameObjectId ia = a.id, ib = b.id;
    scene.destroy(ia);
    scene.flush();
    CHECK(scene.find(ia) == nullptr);
    CHECK(scene.find(ib) == nullptr);
    CHECK(spawned && scene.find(spawned) != nullptr);
    CHECK(scene.aliveCount() == 1);
    // clear() with chained destroys must not crash either
    auto& c = scene.createObject<ChainDestroy>(); auto& d = scene.createObject<ChainDestroy>();
    c.other = d.id; d.other = c.id;
    scene.clear();
    CHECK(scene.aliveCount() == 0);
}

int main() {
    for (auto& [name, fn] : registry()) {
        std::printf("[ RUN ] %s\n", name);
        fn();
    }
    std::printf(failures ? "FAILED (%d)\n" : "OK\n", failures);
    return failures ? 1 : 0;
}
