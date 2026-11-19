#include "engine/assets/AtlasLoader.hpp"
#include "engine/core/Paths.hpp"
#include "engine/render/LightComponent.hpp"
#include "engine/render/Materials.hpp"
#include "engine/render/PixelScale.hpp"
#include "engine/render/SceneRenderer.hpp"
#include "engine/tools/LookFile.hpp"
#include "engine/world/CharacterController.hpp"
#include "engine/world/Collision.hpp"
#include "engine/world/CollisionWorld.hpp"
#include "engine/world/Ldtk.hpp"
#include "engine/world/LevelLoader.hpp"
#include "engine/world/Region.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/TerrainMesh.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldState.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/world/PersistentComponent.hpp"
#include "game/objects/CheckpointObject.hpp"
#include "game/objects/DoorObject.hpp"
#include "game/objects/ItemObject.hpp"
#include "game/objects/LeverObject.hpp"
#include "game/objects/PlayerObject.hpp"
#include "game/objects/SlimeEnemy.hpp"

#include <map>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <glm/gtc/constants.hpp>
#include <fstream>
#include <limits>

#include <cstdio>
#include <exception>
#include <functional>
#include <unordered_map>
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
    CHECK(parseLayerDepth("Foreground_d_2", 0, 0) == -2.0f); // LDtk identifiers cannot hold '-': it saves _d_2
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

TEST(ldtk_world_coordinates) {
    // level at worldPx (512,-256): both corners, single Y-up axis
    CHECK(ldtkToWorld({512, -256}, {0, 0}) == glm::vec2(16.0f, 8.0f));
    CHECK(ldtkToWorld({512, -256}, {320, 160}) == glm::vec2(26.0f, 3.0f));
    CHECK(ldtkToWorld({0, 0}, {64, 96}) == glm::vec2(2.0f, -3.0f));
}
TEST(ldtk_project_reads_all_levels) {
    auto p = loadLdtkProject(MUSH2D_TEST_DATA "/world3.ldtk");
    CHECK(p.levels.size() == 3);
    CHECK(p.levels[2].identifier == "Room_C" && p.levels[2].iid == "iid-c");
    CHECK(p.levels[2].worldPx == glm::ivec2(512, -256) && p.levels[2].pxSize == glm::ivec2(320, 160));
    CHECK(p.levels[0].region == "forest" && p.levels[2].region == "cave");
    CHECK(p.levels[1].region == "forest"); // missing field -> default
    bool threw = false;
    try { loadLdtkProject(MUSH2D_TEST_DATA "/bad.ldtk"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
}
TEST(world_neighbours_by_touching_rects) {
    auto room = [](int x, int y) { LdtkLevel l; l.worldPx = {x, y}; l.pxSize = {100, 100}; return l; };
    // 0 anchor; 1 touches right edge; 2 overlaps 1; 3 is 2 px left of 0; 4 is 1 px below 0 (tolerance)
    auto n = computeNeighbours({room(0, 0), room(100, 0), room(150, 50), room(-102, 0), room(0, 101)});
    CHECK((n[0] == std::vector<int>{1, 4}));
    CHECK((n[1] == std::vector<int>{0, 2, 4}));
    CHECK((n[2] == std::vector<int>{1}));
    CHECK(n[3].empty());
}
namespace {
// Rooms in a row along +X, all 320 px tall at worldY = 0; widths in px.
LdtkProject roomsInRow(std::vector<int> widths) {
    LdtkProject p;
    int x = 0;
    for (std::size_t i = 0; i < widths.size(); ++i) {
        LdtkLevel l;
        l.iid = "room" + std::to_string(i);
        l.worldPx = {x, 0};
        l.pxSize = {widths[i], 320};
        p.levels.push_back(l);
        x += widths[i];
    }
    return p;
}
struct StreamLog { int loads = 0, unloads = 0; };
void hookWorld(World& w, StreamLog& log) {
    w.onLoad = [&log](int) { ++log.loads; };
    w.onUnload = [&log](int) { ++log.unloads; };
}
}

TEST(world_streaming_loads_neighbours_and_unloads_with_hysteresis) {
    // Units: room0 x 0..10, room1 x 10..12 (narrow), room2 x 12..22; all y -10..0.
    LdtkProject p = roomsInRow({320, 64, 320});
    World w; StreamLog log; hookWorld(w, log);
    LevelLoader loader;
    w.open(p, loader, nullptr);
    CHECK(w.worldBounds() == glm::vec4(0.0f, -10.0f, 22.0f, 0.0f));
    using V = std::vector<int>;
    auto at = [&](float x) { w.update({x, -5.0f}); };

    at(1.0f);  CHECK(w.currentRoom() == 0); CHECK((w.loadedRooms() == V{0, 1}));
    at(11.0f); CHECK(w.currentRoom() == 1); CHECK((w.loadedRooms() == V{0, 1, 2}));
    at(12.5f); CHECK(w.currentRoom() == 2); CHECK((w.loadedRooms() == V{0, 1, 2})); // room0 2.5 away: kept
    at(14.0f); CHECK((w.loadedRooms() == V{0, 1, 2}));                              // exactly 4: kept
    at(14.5f); CHECK((w.loadedRooms() == V{1, 2}));                                 // 4.5 away: unloaded
    at(21.0f); CHECK((w.loadedRooms() == V{1, 2}));
    CHECK(log.loads == 3 && log.unloads == 1);
    // Walk back: room0 is a neighbour of room1 again.
    at(11.0f); CHECK(w.currentRoom() == 1); CHECK((w.loadedRooms() == V{0, 1, 2}));
    at(9.0f);  CHECK(w.currentRoom() == 0); CHECK((w.loadedRooms() == V{0, 1, 2})); // room2 3 away: kept
    at(1.0f);  CHECK((w.loadedRooms() == V{0, 1}));                                 // room2 11 away: unloaded
    CHECK(log.loads == 4 && log.unloads == 2);
}

TEST(streaming_no_thrash_at_border) {
    LdtkProject p = roomsInRow({320, 320, 320});
    World w; StreamLog log; hookWorld(w, log);
    LevelLoader loader;
    w.open(p, loader, nullptr);
    w.update({10.0f, -5.0f}); // on the shared edge of room0 and room1
    const int firstLoads = log.loads, firstUnloads = log.unloads;
    CHECK(firstLoads > 0);
    for (int i = 0; i < 200; ++i) w.update({10.0f, -5.0f});
    CHECK(log.loads - firstLoads <= 1);
    CHECK(log.unloads - firstUnloads == 0);
}

TEST(world_room_at_keeps_current_on_overlap_and_gap) {
    // Spec decision 4. room0 x 0..10, room1 x 8..18 (overlap 8..10), room2 x 30..40 (gap 18..30).
    LdtkProject p = roomsInRow({320, 320, 320});
    p.levels[1].worldPx.x = 256;
    p.levels[2].worldPx.x = 960;
    World w; StreamLog log; hookWorld(w, log);
    LevelLoader loader;
    w.open(p, loader, nullptr);
    w.update({1.0f, -5.0f});  CHECK(w.currentRoom() == 0);
    w.update({9.0f, -5.0f});  CHECK(w.currentRoom() == 0); // both contain it: current wins
    w.update({12.0f, -5.0f}); CHECK(w.currentRoom() == 1);
    w.update({9.0f, -5.0f});  CHECK(w.currentRoom() == 1);
    w.update({24.0f, -5.0f}); CHECK(w.currentRoom() == 1); // gap: keep the last
    w.update({31.0f, -5.0f}); CHECK(w.currentRoom() == 2);
}

TEST(world_failed_load_is_not_marked_loaded) {
    LdtkProject p = roomsInRow({320, 320});
    World w;
    w.onLoad = [](int room) { if (room == 1) throw std::runtime_error("bad room"); };
    LevelLoader loader;
    w.open(p, loader, nullptr);
    bool threw = false;
    try { w.update({1.0f, -5.0f}); } catch (const std::exception&) { threw = true; }
    CHECK(threw && (w.loadedRooms() == std::vector<int>{0}));
}

TEST(world_unload_keeps_persistent_objects) {
    Scene2D scene;
    scene.spawnRoomIid = "A"; // what LevelLoader::loadRoom sets while a room loads
    GameObjectId a = scene.createObject<GameObject>().id;
    GameObject& kept = scene.createObject<GameObject>();
    kept.persistent = true;
    scene.spawnRoomIid = "B";
    GameObjectId b = scene.createObject<GameObject>().id;
    scene.spawnRoomIid.clear();
    GameObjectId free = scene.createObject<GameObject>().id;
    CHECK(scene.find(a)->roomIid == "A" && scene.find(free)->roomIid.empty());
    GameObjectId keptId = kept.id;

    LevelLoader loader;
    loader.unloadRoom("A", scene);
    scene.flush();
    CHECK(scene.find(a) == nullptr);
    CHECK(scene.find(keptId) != nullptr && scene.find(b) != nullptr && scene.find(free) != nullptr);
}

// --- WorldState, respawn rules, interactive objects ---
namespace {
LdtkEntity entity(std::string identifier, std::string iid, glm::vec2 px, nlohmann::json fields) {
    LdtkEntity e;
    e.identifier = std::move(identifier);
    e.iid = std::move(iid);
    e.px = px;
    e.size = {32, 32};
    e.fields = std::move(fields);
    return e;
}
// Three rooms in a row (x 0..10, 10..20, 20..30, y -10..0); `entities` go into room 0.
LdtkProject roomsWith(std::vector<LdtkEntity> entities) {
    LdtkProject p = roomsInRow({320, 320, 320});
    p.levels[0].layers.push_back(LdtkLayer{});
    p.levels[0].layers[0].entities = std::move(entities);
    return p;
}
// Counts factory runs by iid (no GPU: onSpawn replaces LevelLoader::spawnEntity).
struct SpawnCount {
    std::map<std::string, int> n;
    void hook(World& w) {
        w.onLoad = [](int) {};
        w.onUnload = [](int) {};
        w.onSpawn = [this](int, const LdtkEntity& e) { ++n[e.iid]; };
    }
};
// Room 0 entity at px (32, 288) -> world centre (1.5, -9.5).
constexpr glm::vec2 SpawnAt{1.5f, -9.5f};
const glm::vec4 FarView{100.0f, 100.0f, 110.0f, 110.0f}; // a camera rect that shows none of the rooms
}

TEST(worldstate_permanent_never_respawns) {
    CHECK(parsePersistence(entity("Slime", "a", {}, {{"respawn", false}})) == Persistence::Permanent);
    CHECK(parsePersistence(entity("Slime", "a", {}, {{"respawn", true}})) == Persistence::Respawn);
    CHECK(parsePersistence(entity("Item", "a", {}, {{"persistence", "permanent"}})) == Persistence::Permanent);
    CHECK(parsePersistence(entity("Item", "a", {}, {{"persistence", "respawn"}})) == Persistence::Respawn);
    CHECK(parsePersistence(entity("Item", "a", {}, {{"persistence", "none"}})) == Persistence::None);
    CHECK(parsePersistence(entity("Light", "a", {}, nlohmann::json::object())) == Persistence::None);
    CHECK(parseRespawnDistance(entity("Item", "a", {}, nlohmann::json::object())) == 12.0f);

    World w; SpawnCount c; c.hook(w);
    LevelLoader loader;
    w.open(roomsWith({entity("Slime", "s1", {32, 288}, {{"respawn", false}})}), loader, nullptr);
    w.update({1.0f, -5.0f});
    CHECK(c.n["s1"] == 1);
    w.state.markGone("s1", SpawnAt, 1.0);
    w.update({25.0f, -5.0f}); // room 0 unloads
    CHECK((w.loadedRooms() == std::vector<int>{1, 2}));
    w.update({11.0f, -5.0f}); // and loads again
    CHECK(w.loadedRooms().front() == 0);
    for (double t = 2.0; t < 10.0; t += 0.5) w.checkRespawns({28.0f, -5.0f}, FarView, t);
    CHECK(c.n["s1"] == 1 && w.state.isGone("s1"));
}

TEST(worldstate_respawn_by_distance_offscreen) {
    World w; SpawnCount c; c.hook(w);
    LevelLoader loader;
    w.open(roomsWith({entity("Item", "i1", {32, 288}, {{"persistence", "respawn"}, {"respawnDistance", 5}})}), loader,
           nullptr);
    w.update({1.0f, -5.0f});
    w.state.markGone("i1", SpawnAt, 1.0);
    w.checkRespawns(SpawnAt + glm::vec2(4.0f, 0.0f), FarView, 1.0); // 4 < 5: too close
    CHECK(c.n["i1"] == 1);
    w.checkRespawns(SpawnAt + glm::vec2(6.0f, 0.0f), FarView, 1.2); // far, but within the 0.5 s interval
    CHECK(c.n["i1"] == 1);
    w.checkRespawns(SpawnAt + glm::vec2(6.0f, 0.0f), FarView, 1.5);
    CHECK(c.n["i1"] == 2 && !w.state.isGone("i1"));

    // Room not loaded: no respawn; reloading the room skips it (still gone) until the check brings it back.
    w.state.markGone("i1", SpawnAt, 2.0);
    w.update({25.0f, -5.0f});
    w.checkRespawns({25.0f, -5.0f}, FarView, 3.0);
    CHECK(c.n["i1"] == 2);
    w.update({11.0f, -5.0f});
    CHECK(c.n["i1"] == 2);
    w.checkRespawns({11.0f, -5.0f}, FarView, 4.0);
    CHECK(c.n["i1"] == 3);
}

TEST(respawn_waits_until_offscreen) {
    World w; SpawnCount c; c.hook(w);
    LevelLoader loader;
    w.open(roomsWith({entity("Slime", "s1", {32, 288}, {{"respawn", true}})}), loader, nullptr);
    w.update({1.0f, -5.0f});
    w.state.markGone("s1", SpawnAt, 0.0);
    const glm::vec2 far{SpawnAt.x + 13.0f, SpawnAt.y}; // beyond the default 12
    const glm::vec4 showsSpawn{0.0f, -10.0f, 20.0f, 0.0f};
    for (double t = 1.0; t < 4.0; t += 0.5) w.checkRespawns(far, showsSpawn, t);
    CHECK(c.n["s1"] == 1 && w.state.isGone("s1"));
    w.checkRespawns(far, {2.0f, -10.0f, 22.0f, 0.0f}, 4.0); // 0.5 outside the view: inside the 1-unit margin
    CHECK(c.n["s1"] == 1);
    w.checkRespawns(far, {5.0f, -10.0f, 25.0f, 0.0f}, 4.5); // camera moved right: spawn point off screen
    CHECK(c.n["s1"] == 2);
}

TEST(interactive_state_survives_unload) {
    Scene2D scene;
    World w;
    LevelLoader loader;
    std::map<std::string, GameObjectId> live;
    w.onLoad = [](int) {};
    w.onUnload = [&](int room) { loader.unloadRoom(w.project().levels[room].iid, scene); };
    w.onSpawn = [&](int room, const LdtkEntity& e) {
        scene.spawnRoomIid = w.project().levels[room].iid;
        GameObject* o = nullptr;
        if (e.identifier == "Door") o = &scene.createObject<DoorObject>(w.state, e.iid, glm::vec2(1.0f, 2.0f));
        else o = &scene.createObject<LeverObject>(w.state, e.iid, e.fields.at("door").at("entityIid").get<std::string>());
        scene.spawnRoomIid.clear();
        live[e.iid] = o->id;
    };
    w.open(roomsWith({entity("Door", "d1", {32, 288}, nlohmann::json::object()),
                      entity("Lever", "l1", {96, 288}, {{"door", {{"entityIid", "d1"}}}})}),
           loader, nullptr);
    w.update({1.0f, -5.0f});
    scene.flush();
    auto* door = scene.findObjectAs<DoorObject>(live["d1"]);
    CHECK(door && !door->isOpen() && door->collider->enabled);

    scene.findObjectAs<LeverObject>(live["l1"])->toggle();
    w.update({25.0f, -5.0f}); // unload room 0
    CHECK(scene.findObject(live["d1"]) == nullptr);
    w.update({11.0f, -5.0f}); // reload
    scene.flush();
    door = scene.findObjectAs<DoorObject>(live["d1"]);
    auto* lever = scene.findObjectAs<LeverObject>(live["l1"]);
    CHECK(door && door->isOpen() && !door->collider->enabled && !door->visible);
    CHECK(lever && lever->isOn());
    lever->toggle(); // closes it again, live
    scene.step(0.016f);
    CHECK(!door->isOpen() && door->collider->enabled);
}

TEST(lever_toggles_on_down_and_item_is_collected) {
    Scene2D scene;
    WorldState state;
    PlayerObject::Config pc;
    pc.movementMode = PlayerObject::Config::MovementMode::Platformer;
    pc.gravity = 0.0f;
    PlayerObject& player = scene.createObject<PlayerObject>(pc);
    LeverObject& lever = scene.createObject<LeverObject>(state, "l1", "d1");
    InputState down;
    down.moveDown.pressed = down.moveDown.down = true;
    player.applyInput(down);
    scene.step(0.016f); // not standing on it yet (the game's trigger scan reports overlaps)
    CHECK(!lever.isOn());
    lever.onTriggerEnter(scene, player);
    player.applyInput(down);
    scene.step(0.016f);
    CHECK(lever.isOn() && state.get("d1", "open", 0.0) == 1.0);
    player.applyInput(InputState{}); // not pressed this frame: no toggle
    scene.step(0.016f);
    CHECK(lever.isOn());

    ItemObject& item = scene.createObject<ItemObject>(state, "coin");
    trackEntity(item, entity("Item", "i1", {}, {{"persistence", "permanent"}}), {2.0f, 3.0f});
    const GameObjectId itemId = item.id;
    item.onTriggerEnter(scene, player);
    scene.flush();
    CHECK(scene.findObject(itemId) == nullptr && state.isGone("i1") && state.find("i1")->spawnPos == glm::vec2(2.0f, 3.0f));
}

TEST(reload_keeps_world_state) {
    World w; SpawnCount c; c.hook(w);
    LevelLoader loader;
    LdtkProject p = roomsWith({entity("Slime", "s1", {32, 288}, {{"respawn", false}}),
                               entity("Door", "d1", {96, 288}, nlohmann::json::object())});
    w.open(p, loader, nullptr);
    w.update({1.0f, -5.0f});
    w.state.markGone("s1", SpawnAt, 1.0);
    w.state.set("d1", "open", 1.0);
    w.open(p, loader, nullptr); // F5: same project parsed again
    w.update({1.0f, -5.0f});
    CHECK(c.n["s1"] == 1 && c.n["d1"] == 2);
    CHECK(w.state.isGone("s1") && w.state.get("d1", "open", 0.0) == 1.0);

    // Snapshot of a room's iids: restore rolls changes back and erases entries made after it.
    auto snap = w.state.snapshot({"s1", "d1", "x"});
    CHECK(snap.size() == 2);
    w.state.clearGone("s1");
    w.state.set("d1", "open", 0.0);
    w.state.set("x", "open", 1.0);
    w.state.set("other", "open", 1.0);
    w.state.restore({"s1", "d1", "x"}, snap);
    CHECK(w.state.isGone("s1") && w.state.get("d1", "open", 0.0) == 1.0);
    CHECK(w.state.find("x") == nullptr && w.state.get("other", "open", 0.0) == 1.0);
}

TEST(death_restores_room_snapshot) {
    // Room 0: permanent slime, door, checkpoint. Room 1: a door of its own.
    LdtkProject p = roomsWith({entity("Slime", "s1", {32, 288}, {{"respawn", false}}),
                               entity("Door", "d1", {96, 288}, nlohmann::json::object()),
                               entity("Checkpoint", "c1", {160, 288}, nlohmann::json::object())});
    p.levels[1].layers.push_back(LdtkLayer{});
    p.levels[1].layers[0].entities = {entity("Door", "d2", {32, 288}, nlohmann::json::object())};
    World w; SpawnCount c; c.hook(w);
    std::vector<int> unloads;
    w.onUnload = [&](int room) { unloads.push_back(room); };
    LevelLoader loader;
    w.open(p, loader, nullptr);
    w.update({11.0f, -5.0f}); // start in room 1
    w.state.set("d1", "open", 1.0); // opened from afar before entering room 0
    w.state.set("d2", "open", 1.0);
    w.update({1.0f, -5.0f}); // enter room 0: snapshot
    CHECK(w.currentRoom() == 0);
    w.state.markGone("s1", SpawnAt, 2.0);
    w.state.set("d1", "open", 0.0);
    w.state.set("c1", "activated", 3.0);
    w.state.set("d2", "open", 0.0); // another room's state, changed while in room 0

    glm::vec2 teleported{-1.0f};
    unloads.clear(); // room 2 left when room 0 became current
    w.onPlayerDeath({5.0f, -9.0f}, [&](glm::vec2 at) { teleported = at; });
    CHECK(!w.state.isGone("s1") && w.state.get("d1", "open", 0.0) == 1.0); // room 0 as it was on entry
    CHECK(w.state.get("c1", "activated", -1.0) == 3.0);                   // checkpoints are never rolled back
    CHECK(w.state.get("d2", "open", 1.0) == 0.0);                          // other rooms keep their state
    CHECK((unloads == std::vector<int>{0}) && c.n["s1"] == 2 && c.n["d1"] == 2); // room 0 reloaded
    CHECK(teleported == glm::vec2(5.0f, -9.0f) && w.currentRoom() == 0);
}

TEST(death_fade_freezes_world) {
    LdtkProject p = roomsWith({entity("Door", "d1", {96, 288}, nlohmann::json::object())});
    World w; SpawnCount c; c.hook(w);
    LevelLoader loader;
    w.open(p, loader, nullptr);
    w.update({1.0f, -5.0f});
    w.state.set("d1", "open", 1.0); // changed after entry: death must undo it
    glm::vec2 teleported{-1.0f};
    w.killPlayer({1.0f, -5.0f}, [&](glm::vec2 at) { teleported = at; });
    CHECK(w.dying());
    w.stepFade(0.1f);
    w.update({15.0f, -5.0f}); // focus drifts into room 1 mid-fade: ignored
    CHECK(w.currentRoom() == 0);
    w.stepFade(0.1f); // black: dies
    CHECK(w.currentRoom() == 0 && w.state.get("d1", "open", 0.0) == 0.0 && teleported == glm::vec2(1.0f, -5.0f));
}

TEST(death_snapshot_covers_cross_room_door) {
    LdtkProject p = roomsWith({entity("Lever", "l1", {32, 288}, {{"door", {{"entityIid", "d2"}}}})});
    p.levels[1].layers.push_back(LdtkLayer{});
    p.levels[1].layers[0].entities = {entity("Door", "d2", {32, 288}, nlohmann::json::object())};
    World w; SpawnCount c; c.hook(w);
    LevelLoader loader;
    w.open(p, loader, nullptr);
    w.update({1.0f, -5.0f}); // enter room 0
    w.state.set("l1", "on", 1.0);
    w.state.set("d2", "open", 1.0); // the lever opens a door in room 1
    w.onPlayerDeath({1.0f, -5.0f}, [](glm::vec2) {});
    CHECK(w.state.get("l1", "on", 0.0) == 0.0 && w.state.get("d2", "open", 0.0) == 0.0);
}

TEST(teleport_streams_target_synchronously) {
    LdtkProject p = roomsInRow({320, 320, 320, 320, 320}); // x 0..50
    World w; StreamLog log; hookWorld(w, log);
    LevelLoader loader;
    w.open(p, loader, nullptr);
    w.update({1.0f, -5.0f});
    glm::vec2 teleported{-1.0f};
    w.onPlayerDeath({45.0f, -5.0f}, [&](glm::vec2 at) { teleported = at; });
    const std::vector<int>& L = w.loadedRooms();
    CHECK(w.currentRoom() == 4 && teleported == glm::vec2(45.0f, -5.0f));
    CHECK(std::count(L.begin(), L.end(), 3) == 1 && std::count(L.begin(), L.end(), 4) == 1);
    CHECK(std::count(L.begin(), L.end(), 0) == 0); // the far death room is not reloaded
    w.update({45.0f, -5.0f});
    CHECK((w.loadedRooms() == std::vector<int>{3, 4}));

    // killPlayer: fades out over FadeSeconds, dies at black, fades back in.
    teleported = glm::vec2(-1.0f);
    w.killPlayer({1.0f, -5.0f}, [&](glm::vec2 at) { teleported = at; });
    const float h = World::FadeSeconds * 0.5f;
    CHECK(std::abs(w.stepFade(h) - 0.5f) < 1e-4f && teleported.x < 0.0f);
    w.killPlayer({45.0f, -5.0f}, [&](glm::vec2 at) { teleported = at; }); // ignored while dying
    CHECK(w.stepFade(h) < 1e-4f && teleported == glm::vec2(1.0f, -5.0f) && w.currentRoom() == 0);
    CHECK(std::abs(w.stepFade(h) - 0.5f) < 1e-4f);
    CHECK(w.stepFade(h) == 1.0f && w.stepFade(h) == 1.0f);
}

TEST(checkpoint_records_activation_time_only_with_iid) {
    Scene2D scene;
    WorldState state;
    state.clock = 7.0;
    PlayerObject& player = scene.createObject<PlayerObject>(PlayerObject::Config{});
    CheckpointObject::Config cc;
    cc.worldState = &state;
    scene.createObject<CheckpointObject>(cc).onTriggerEnter(scene, player); // no iid
    CHECK(state.find("") == nullptr);
    cc.iid = "c1";
    scene.createObject<CheckpointObject>(cc).onTriggerEnter(scene, player);
    CHECK(state.get("c1", "activated", -1.0) == 7.0); // activation time: the latest one is the respawn point
}

TEST(slime_stomp_kills_and_bounces) {
    Scene2D scene;
    WorldState state;
    SlimeEnemy::Config sc;
    sc.worldState = &state;
    sc.moveSpeed = 0.0f;
    SlimeEnemy& slime = scene.createObject<SlimeEnemy>(sc); // collider 0.8 x 0.8 at (0, 0)
    GameObject& ground = scene.createObject<GameObject>(); // the slime falls now: it stands on this
    ground.transform.position = {0.0f, -0.5f};
    ground.collider = ColliderData{.size = {2.0f, 0.2f}};
    trackEntity(slime, entity("Slime", "s1", {}, {{"respawn", false}}), {0.0f, 0.0f});
    PlayerObject::Config pc;
    pc.movementMode = PlayerObject::Config::MovementMode::Platformer;
    pc.colliderSize = {0.6f, 1.6f};
    pc.jumpSpeed = 10.0f;
    PlayerObject& player = scene.createObject<PlayerObject>(pc);
    player.transform.position = {0.0f, 1.5f}; // feet at 0.7, slime top at 0.4
    const GameObjectId slimeId = slime.id;
    bool bounced = false;
    for (int i = 0; i < 60 && !bounced; ++i) {
        scene.step(1.0f / 60.0f);
        bounced = player.velocity.y > 0.0f;
    }
    scene.flush();
    CHECK(bounced && std::abs(player.velocity.y - 6.0f) < 0.6f); // jumpSpeed * 0.6, minus up to a frame of gravity
    CHECK(scene.findObject(slimeId) == nullptr && state.isGone("s1"));

    // From the side (feet below the slime's centre) nothing happens.
    SlimeEnemy& other = scene.createObject<SlimeEnemy>(sc);
    other.transform.position = {5.0f, 0.0f};
    GameObject& floor = scene.createObject<GameObject>();
    floor.transform.position = {5.0f, -0.5f};
    floor.collider = ColliderData{.size = {20.0f, 0.2f}};
    player.transform.position = {5.65f, 0.401f}; // overlaps the slime's right edge, feet just above the floor
    player.velocity = {0.0f, 0.0f};
    for (int i = 0; i < 10; ++i) scene.step(1.0f / 60.0f);
    CHECK(scene.findObject(other.id) != nullptr && player.velocity.y <= 0.0f);
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
TEST(look_pixel_mode) {
    auto mode = [](const char* text) { auto l = parseLook(text, FrameSettings{}); return l ? l->settings.pixelMode : -1; };
    CHECK(mode(R"({})") == PixelSmooth); // FrameSettings default
    CHECK(mode(R"({"pixelArt":{"enabled":true}})") == PixelSharp);
    CHECK(mode(R"({"pixelArt":{"enabled":false}})") == PixelHD);
    CHECK(mode(R"({"pixelArt":{"enabled":true,"mode":"smooth"}})") == PixelSmooth);
    CHECK(mode(R"({"pixelArt":{"mode":"bogus"}})") == PixelSmooth); // unknown string keeps the default
    for (int m : {PixelSharp, PixelSmooth, PixelHD}) {
        LookFile a; a.settings.pixelMode = m;
        auto b = parseLook(serializeLook(a), FrameSettings{});
        CHECK(b && b->settings.pixelMode == m);
    }
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

TEST(region_roundtrip) {
    Region a;
    a.look.settings.fogDensity = 0.2f; a.look.lights["abc"] = {{1, 0.5f, 0}, 3, 6, 1.5f, 0, 0.1f};
    a.layers.push_back({"assets/kaa/parallax/bg1.png", 18.0f, {0.22f, 0.55f}, {-18.72f, 1.375f}, true});
    a.layers.push_back({"assets/x.png", 2.0f, {0.85f, 0.92f}, {0.0f, -3.5f}, false});
    auto b = parseRegion(serializeRegion(a), FrameSettings{});
    CHECK(b && b->look.settings.fogDensity == 0.2f && b->look.lights.at("abc").radius == 6);
    CHECK(b && b->layers.size() == 2);
    if (b && b->layers.size() == 2) {
        const RegionLayer& l = b->layers[0];
        CHECK(l.image == "assets/kaa/parallax/bg1.png" && l.depth == 18.0f && l.repeatX);
        CHECK(l.parallax == glm::vec2(0.22f, 0.55f) && l.offset == glm::vec2(-18.72f, 1.375f));
        CHECK(!b->layers[1].repeatX && b->layers[1].image == "assets/x.png");
    }
    CHECK(!parseRegion("{ nope", FrameSettings{}));
}
TEST(missing_region_falls_back) {
    FrameSettings d; d.exposure = 2.5f;
    Region r = loadRegionOrDefault(std::filesystem::temp_directory_path() / "mush2d_nowhere.region.json", d);
    CHECK(r.name == "mush2d_nowhere" && r.layers.empty() && r.look.settings.exposure == 2.5f);
}
TEST(region_look_blend) {
    FrameSettings a, b;
    a.fogDensity = 0.0f; b.fogDensity = 0.2f;
    a.ambientColor = {0, 0, 0}; b.ambientColor = {1, 0.5f, 0.25f};
    a.pixelMode = PixelSharp; b.pixelMode = PixelHD;
    a.lightHalfRes = false; b.lightHalfRes = true;
    auto near = [](float x, float y) { return std::abs(x - y) < 1e-6f; };
    const float duration = 1.5f;
    FrameSettings s0 = lerpSettings(a, b, 0.0f / duration);
    CHECK(s0.fogDensity == 0.0f && s0.ambientColor == a.ambientColor && s0.pixelMode == PixelSharp && !s0.lightHalfRes);
    FrameSettings s1 = lerpSettings(a, b, duration / duration);
    CHECK(s1.fogDensity == 0.2f && s1.ambientColor == b.ambientColor && s1.pixelMode == PixelHD && s1.lightHalfRes);
    FrameSettings mid = lerpSettings(a, b, 0.75f / duration);
    CHECK(near(mid.fogDensity, 0.1f) && near(mid.ambientColor.y, 0.25f) && mid.pixelMode == PixelHD && mid.lightHalfRes);
    FrameSettings early = lerpSettings(a, b, 0.7f / duration); // ints and bools switch at t >= 0.5
    CHECK(early.pixelMode == PixelSharp && !early.lightHalfRes && near(early.exposure, 1.0f));
}
TEST(region_repeat_copies_cover_only_the_view) {
    // 18-unit image, copy 0 left edge at x = 2, parallax 0.5. View 10 units wide around camera x = 40 -> layer x 20 +- 5.
    RegionLayer l{"x.png", 4.0f, {0.5f, 1.0f}, {2.0f, 0.0f}, true};
    glm::ivec2 r = regionCopyRange(l, 18.0f, {40.0f, 0.0f}, 5.0f); // copies span [2+18k, 20+18k]
    CHECK(r == glm::ivec2(0, 1));                                  // 15..25 touches copy 0 (2..20) and copy 1 (20..38)
    CHECK(regionCopyRange(l, 18.0f, {0.0f, 0.0f}, 1.0f) == glm::ivec2(-1, -1)); // -1..1: only copy -1 (-16..2)
    l.repeatX = false;
    CHECK(regionCopyRange(l, 18.0f, {400.0f, 0.0f}, 5.0f) == glm::ivec2(0, 0)); // single copy, never culled here
}
TEST(region_bad_image_is_skipped) {
    Region r;
    r.layers.push_back({"assets/nowhere/missing.png", 1.0f, {1.0f, 1.0f}, {0.0f, 0.0f}, true});
    MaterialLibrary lib; // no GPU: an unreadable image fails before any upload
    resolveRegionMaterials(r, lib); // resolved once, at region load
    CHECK(r.layers.size() == 1 && !r.layers[0].material); // kept (saved back), not drawn
    Camera2D cam;
    SpriteBatch b;
    appendRegionLayers(r, cam, VkExtent2D{1280, 720}, b);
    CHECK(b.sorted().empty());
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
    static inline std::unordered_map<GameObjectId, int> destroyLog; // survives the object being freed
    static inline std::vector<GameObjectId> destroyOrder;
    void onDestroy(Scene2D&) override { ++destroys; ++destroyLog[id]; destroyOrder.push_back(id); }
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
    Counted::destroyLog.clear();
    scene.destroy(ia);
    scene.flush();
    CHECK(Counted::destroyLog[ia] == 1);
    CHECK(Counted::destroyLog[ib] == 1);
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


TEST(child_world_transform) {
    Scene2D scene;
    auto& parent = scene.createObject<GameObject>();
    auto& child = scene.createObject<GameObject>();
    parent.transform.position = {2, 0}; parent.transform.rotation = glm::half_pi<float>(); parent.transform.scale = {2, 2};
    child.transform.position = {1, 0};
    CHECK(scene.setParent(child.id, parent.id, false));
    CHECK(child.parent() == parent.id && parent.children().size() == 1);
    scene.flush();
    auto w = child.world();
    CHECK(std::abs(w.position.x - 2) < 1e-4f && std::abs(w.position.y - 2) < 1e-4f);
    CHECK(std::abs(w.scale.x - 2) < 1e-4f && std::abs(w.rotation - glm::half_pi<float>()) < 1e-4f);
    parent.transform.position = {5, 1};
    scene.step(0.016f);
    CHECK(std::abs(scene.worldTransform(child.id).position.x - 5) < 1e-4f);
    CHECK(std::abs(scene.worldTransform(child.id).position.y - 3) < 1e-4f);
    // keepWorld: reparenting preserves world, detaching too
    auto& other = scene.createObject<GameObject>();
    other.transform.position = {-3, 4};
    CHECK(scene.setParent(other.id, parent.id, true));
    auto ow = other.world();
    CHECK(std::abs(ow.position.x + 3) < 1e-4f && std::abs(ow.position.y - 4) < 1e-4f);
    CHECK(std::abs(other.transform.scale.x - 0.5f) < 1e-4f);
    CHECK(scene.setParent(other.id, InvalidGameObjectId, true));
    CHECK(!other.parent() && std::abs(other.transform.position.x + 3) < 1e-4f && parent.children().size() == 1);
}

TEST(destroy_parent_destroys_children) {
    Scene2D scene;
    auto& p = scene.createObject<Counted>();
    auto& c = scene.createObject<Counted>();
    auto& g = scene.createObject<Counted>();
    GameObjectId ip = p.id, ic = c.id, ig = g.id;
    CHECK(scene.setParent(ic, ip) && scene.setParent(ig, ic));
    Counted::destroyLog.clear(); Counted::destroyOrder.clear();
    scene.destroy(ip);
    scene.flush();
    CHECK(!scene.find(ip) && !scene.find(ic) && !scene.find(ig));
    CHECK(Counted::destroyLog[ip] == 1 && Counted::destroyLog[ic] == 1 && Counted::destroyLog[ig] == 1);
    CHECK((Counted::destroyOrder == std::vector<GameObjectId>{ig, ic, ip})); // children before parents
    CHECK(scene.aliveCount() == 0);
    // destroying only a child unlinks it from the parent
    auto& p2 = scene.createObject<Counted>(); auto& c2 = scene.createObject<Counted>();
    scene.setParent(c2.id, p2.id);
    scene.destroy(c2.id); scene.flush();
    CHECK(p2.children().empty());
}

TEST(reparent_cycle_rejected) {
    Scene2D scene;
    auto& a = scene.createObject<GameObject>(); auto& b = scene.createObject<GameObject>(); auto& c = scene.createObject<GameObject>();
    CHECK(scene.setParent(b.id, a.id) && scene.setParent(c.id, b.id));
    CHECK(!scene.setParent(a.id, c.id));
    CHECK(!scene.setParent(a.id, a.id));
    CHECK(!a.parent());
    CHECK(scene.setParent(c.id, a.id)); // legal reparent
    CHECK(b.children().empty() && a.children().size() == 2);
}

TEST(setParent_to_dying_parent_is_consistent) {
    Scene2D scene;
    auto& p = scene.createObject<Counted>(); auto& c = scene.createObject<Counted>();
    GameObjectId ip = p.id, ic = c.id;
    scene.destroy(ip);
    CHECK(!scene.setParent(ic, ip)); // would leave a dangling parent after the flush
    scene.flush();
    CHECK(!scene.find(ip) && scene.find(ic) && !scene.find(ic)->parent());
    // a pending-destroy child cannot be attached either
    auto& p2 = scene.createObject<Counted>(); auto& c2 = scene.createObject<Counted>();
    scene.destroy(c2.id);
    CHECK(!scene.setParent(c2.id, p2.id));
    scene.flush();
    CHECK(p2.children().empty());
}

TEST(find_helpers_skip_pending_destroy) {
    Scene2D scene;
    auto& a = scene.createObject<Counted>(); a.addTag("x");
    GameObjectId ia = a.id;
    scene.destroy(ia);
    CHECK(scene.find(ia) != nullptr);
    CHECK(scene.findFirstObjectOfType<Counted>() == nullptr);
    CHECK(scene.findAllObjectsOfType<Counted>().empty());
    CHECK(scene.findObjectsByTag("x").empty());
    CHECK(scene.findObjectAs<Counted>(ia) == nullptr);
    CHECK(scene.findObject(ia) == nullptr && std::as_const(scene).findObject(ia) == nullptr);
}

TEST(iteration_guard_survives_throw) {
    Scene2D scene;
    scene.createObject<GameObject>();
    scene.flush();
    try { scene.forEach([](GameObject&) { throw 1; }); } catch (int) {}
    auto& b = scene.createObject<Counted>();
    scene.destroy(b.id);
    scene.flush();
    CHECK(scene.aliveCount() == 1); // flush ran, so iterating_ was released
}

namespace {
struct Probe2 : Component {
    std::vector<std::string>* log = nullptr; std::string name;
    void onAttach(GameObject&, Scene2D&) override { log->push_back(name + ":attach"); }
    void onStep(GameObject&, Scene2D&, float) override { log->push_back(name + ":step"); }
    void onDetach(GameObject&, Scene2D&) override { log->push_back(name + ":detach"); }
};
struct CompA : Probe2 { CompA(std::vector<std::string>* l) { log = l; name = "A"; } };
struct CompB : Probe2 { CompB(std::vector<std::string>* l) { log = l; name = "B"; } };
struct LoggedOwner : GameObject {
    std::vector<std::string>* log = nullptr;
    void onStep(Scene2D&, float) override { log->push_back("owner:step"); }
    void onDestroy(Scene2D&) override { log->push_back("owner:destroy"); }
};
}

TEST(component_lifecycle_and_step_order) {
    Scene2D scene;
    std::vector<std::string> log;
    auto& o = scene.createObject<LoggedOwner>(); o.log = &log;
    o.addComponent<CompA>(&log); auto& b = o.addComponent<CompB>(&log);
    CHECK(o.getComponent<CompA>() && o.getComponent<CompB>() == &b);
    CHECK(o.getComponent<LightComponent>() == nullptr);
    CHECK((log == std::vector<std::string>{"A:attach", "B:attach"}));
    log.clear();
    scene.step(0.016f);
    CHECK((log == std::vector<std::string>{"owner:step", "A:step", "B:step"}));
    b.enabled = false; log.clear();
    scene.step(0.016f);
    CHECK((log == std::vector<std::string>{"owner:step", "A:step"}));
    // removal is deferred to flush
    o.removeComponent<CompA>(); log.clear();
    CHECK(o.getComponent<CompA>() != nullptr && log.empty());
    scene.flush();
    CHECK((log == std::vector<std::string>{"A:detach"}));
    CHECK(o.getComponent<CompA>() == nullptr && o.getComponent<CompB>() != nullptr);
    // destroy: components detach before the owner's onDestroy
    log.clear();
    scene.destroy(o.id); scene.flush();
    CHECK((log == std::vector<std::string>{"B:detach", "owner:destroy"}));
}

TEST(event_bus_deferred_dispatch_and_unsubscribe) {
    struct Ping { int n; };
    EventBus bus;
    std::vector<int> got;
    auto sub = bus.subscribe<Ping>([&](const Ping& p) { got.push_back(p.n); });
    bus.emit(Ping{1}); bus.emit(Ping{2});
    CHECK(got.empty()); // deferred
    bus.dispatch();
    CHECK((got == std::vector<int>{1, 2}));
    // event emitted from a handler lands in the next round, same dispatch()
    EventBus::Subscription chain;
    chain = bus.subscribe<Ping>([&](const Ping& p) { if (p.n == 10) bus.emit(Ping{11}); });
    got.clear(); bus.emit(Ping{10}); bus.dispatch();
    CHECK((got == std::vector<int>{10, 11}));
    // handler unsubscribing itself and another during dispatch is safe
    EventBus::Subscription self, other;
    int selfHits = 0, otherHits = 0;
    other = bus.subscribe<Ping>([&](const Ping&) { ++otherHits; });
    self = bus.subscribe<Ping>([&](const Ping&) { ++selfHits; self = {}; other = {}; });
    bus.emit(Ping{0}); bus.dispatch(); bus.emit(Ping{0}); bus.dispatch();
    CHECK(selfHits == 1 && otherHits == 1);
    sub = {}; got.clear();
    bus.emit(Ping{3}); bus.dispatch();
    CHECK(got.empty());
    // runaway re-emit is capped (8 rounds) instead of looping forever
    int rounds = 0;
    auto loop = bus.subscribe<Ping>([&](const Ping&) { ++rounds; bus.emit(Ping{0}); });
    bus.emit(Ping{0}); bus.dispatch();
    CHECK(rounds == 8);
}

TEST(subscription_dies_with_owner) {
    struct Hit { int n; };
    static int hits = 0; hits = 0;
    struct Listener : GameObject {
        EventBus::Subscription sub;
        void onCreate(Scene2D& s) override { sub = s.events().subscribe<Hit>([this](const Hit&) { ++hits; }); }
    };
    Scene2D scene;
    GameObjectId l = scene.createObject<Listener>().id;
    scene.events().emit(Hit{1}); scene.events().dispatch();
    CHECK(hits == 1);
    scene.destroy(l); scene.flush();
    scene.events().emit(Hit{1}); scene.events().dispatch();
    CHECK(hits == 1); // dead listener must not be called
    // Subscription outliving its bus is harmless
    EventBus::Subscription late;
    int lateHits = 0;
    { EventBus tmp; late = tmp.subscribe<Hit>([&](const Hit&) { ++lateHits; }); tmp.emit(Hit{1}); }
    late = {};
    CHECK(lateHits == 0); // bus died undispatched: handler never ran
}

TEST(object_destroyed_event_emitted_on_flush) {
    Scene2D scene;
    std::vector<GameObjectId> seen;
    auto sub = scene.events().subscribe<ObjectDestroyed>([&](const ObjectDestroyed& e) { seen.push_back(e.id); });
    GameObjectId a = scene.createObject<GameObject>().id, b = scene.createObject<GameObject>().id;
    scene.destroy(a); scene.destroy(b);
    CHECK(seen.empty());
    scene.flush();
    CHECK(seen.size() == 2 && seen[0] == a && seen[1] == b);
}

TEST(light_component_follows_owner) {
    Scene2D scene;
    auto& o = scene.createObject<GameObject>();
    o.transform.position = {10, 5};
    auto& lc = o.addComponent<LightComponent>();
    lc.light = GpuLight{{1, 0, 0.5f, 4}, {1, 1, 1, 2}, {0, 0, 0, 0}};
    scene.flush(); // forEach only visits flushed objects
    LightList lights;
    collectComponentLights(scene, lights);
    CHECK(lights.size() == 1);
    CHECK(std::abs(lights.data()[0].posHeightRadius.x - 11) < 1e-4f && std::abs(lights.data()[0].posHeightRadius.y - 5) < 1e-4f);
    CHECK(lights.data()[0].posHeightRadius.z == 0.5f && lights.data()[0].posHeightRadius.w == 4);
    // moves with owner; rotation and scale apply to the local offset
    o.transform.position = {0, 0}; o.transform.rotation = glm::half_pi<float>(); o.transform.scale = {2, 2};
    lights.clear(); collectComponentLights(scene, lights);
    CHECK(std::abs(lights.data()[0].posHeightRadius.x) < 1e-4f && std::abs(lights.data()[0].posHeightRadius.y - 2) < 1e-4f);
    // child owner uses the parent's world transform
    auto& child = scene.createObject<GameObject>();
    child.addComponent<LightComponent>().light = lc.light;
    scene.setParent(child.id, o.id, false); scene.flush();
    lights.clear(); collectComponentLights(scene, lights);
    CHECK(lights.size() == 2);
    // disabled component and inactive owner are skipped
    lc.enabled = false; lights.clear(); collectComponentLights(scene, lights);
    CHECK(lights.size() == 1);
    child.active = false; lights.clear(); collectComponentLights(scene, lights);
    CHECK(lights.size() == 0);
}

namespace {
using Collision2D::AABB;
bool sameId(const GameObjectId& a, const GameObjectId& b) { return a.index < b.index; }
}

TEST(spatial_grid_matches_bruteforce) {
    std::uint32_t seed = 12345;
    auto rnd = [&](float lo, float hi) {
        seed = seed * 1664525u + 1013904223u;
        return lo + (hi - lo) * float(seed >> 8) / float(1u << 24);
    };
    auto randomBox = [&](float maxSize) {
        glm::vec2 c{rnd(-30, 30), rnd(-30, 30)}, h{rnd(0.05f, maxSize) * 0.5f, rnd(0.05f, maxSize) * 0.5f};
        return AABB{c - h, c + h};
    };
    SpatialGrid grid;
    std::vector<AABB> boxes;
    for (std::uint32_t i = 0; i < 500; ++i) {
        boxes.push_back(randomBox(5.0f));
        grid.insert(GameObjectId{i, 1}, boxes.back());
    }
    // move a few and remove a few so update/remove are covered too
    for (std::uint32_t i = 0; i < 500; i += 7) { boxes[i] = randomBox(5.0f); grid.update(GameObjectId{i, 1}, boxes[i]); }
    std::vector<bool> removed(500, false);
    for (std::uint32_t i = 3; i < 500; i += 11) { removed[i] = true; grid.remove(GameObjectId{i, 1}); }
    for (int q = 0; q < 200; ++q) {
        AABB box = randomBox(12.0f);
        std::vector<GameObjectId> got, expected;
        grid.query(box, got);
        for (std::uint32_t i = 0; i < 500; ++i) {
            if (!removed[i] && Collision2D::intersects(box, boxes[i])) expected.push_back(GameObjectId{i, 1});
        }
        std::sort(got.begin(), got.end(), sameId);
        CHECK(got == expected); // also proves no duplicates
    }
}

TEST(grid_tracks_collider_changes) {
    Scene2D scene;
    auto& o = scene.createObject<GameObject>();
    o.collider = ColliderData{};
    o.transform.position = {0, 0};
    scene.flush();
    const AABB far{{5.5f, -0.5f}, {6.5f, 0.5f}};
    const AABB near{{-0.2f, -0.2f}, {0.2f, 0.2f}};
    CHECK(Collision2D::queryAABB(scene, near).size() == 1);
    CHECK(Collision2D::queryAABB(scene, far).empty());
    o.collider->size = {20, 1}; scene.step(0.016f);        // size change
    CHECK(Collision2D::queryAABB(scene, far).size() == 1);
    o.transform.position = {0, 10}; scene.step(0.016f);   // move
    CHECK(Collision2D::queryAABB(scene, near).empty());
    o.collider->isTrigger = true; scene.step(0.016f);     // trigger flag
    AABB top{{-0.2f, 9.8f}, {0.2f, 10.2f}};
    CHECK(Collision2D::queryAABB(scene, top, Collision2D::QueryKind::Solid).empty());
    CHECK(Collision2D::queryAABB(scene, top, Collision2D::QueryKind::Trigger).size() == 1);
    o.collider->enabled = false; scene.step(0.016f);      // disabled
    CHECK(Collision2D::queryAABB(scene, top).empty());
    o.collider->enabled = true; scene.step(0.016f);
    CHECK(Collision2D::queryAABB(scene, top).size() == 1);
    scene.destroy(o.id); scene.flush();                   // destroyed
    CHECK(Collision2D::queryAABB(scene, top).empty());
}

TEST(parented_mover_collides_in_world_space) {
    Scene2D scene;
    auto& wall = scene.createObject<GameObject>();
    wall.transform.position = {5, 0};
    wall.collider = ColliderData{};
    auto& parent = scene.createObject<GameObject>();
    parent.transform.position = {4, 0};
    auto& mover = scene.createObject<GameObject>();
    mover.collider = ColliderData{};
    scene.setParent(mover.id, parent.id, false);
    scene.flush();
    CHECK(Collision2D::collidesAtPosition(scene, mover.id, {1.5f, 0}));   // local 1.5 = world 5.5, inside the wall
    CHECK(!Collision2D::collidesAtPosition(scene, mover.id, {-3.0f, 0})); // world 1: free
    CHECK(!Collision2D::collidesAtPosition(scene, mover.id, {5.0f, 0}));  // world 9: free (local 5 would hit)
}

TEST(clear_with_destroying_handler_is_safe) {
    Scene2D scene;
    scene.createObject<Counted>(); scene.createObject<Counted>();
    scene.flush();
    bool fired = false;
    auto sub = scene.events().subscribe<ObjectDestroyed>([&](const ObjectDestroyed&) {
        if (fired) return;
        fired = true;
        GameObjectId late = scene.createObject<Counted>().id; // reuses a freed slot
        scene.destroy(late);                                  // queued while clear() dispatches
        scene.destroy(late);
    });
    scene.clear();
    CHECK(fired && scene.aliveCount() == 0);
    Counted::destroyLog.clear();
    GameObjectId fresh = scene.createObject<Counted>().id; // may land in the slot the stale destroy named
    scene.flush();
    scene.step(0.016f);
    CHECK(scene.find(fresh) != nullptr && !scene.find(fresh)->pendingDestroy());
    CHECK(Counted::destroyLog[fresh] == 0);
    CHECK(scene.aliveCount() == 1);
}

TEST(setParent_refreshes_only_what_it_needs) {
    Scene2D scene;
    auto& root = scene.createObject<GameObject>();
    auto& mid = scene.createObject<GameObject>();
    auto& leaf = scene.createObject<GameObject>();
    root.transform.position = {10, 0};
    mid.transform.position = {1, 0}; leaf.transform.position = {1, 0};
    CHECK(scene.setParent(mid.id, root.id, false) && scene.setParent(leaf.id, mid.id, false));
    scene.flush();
    root.transform.position = {20, 0}; // moved after the last flush: the leaf's cache is stale
    CHECK(scene.setParent(leaf.id, InvalidGameObjectId, true));
    CHECK(std::abs(leaf.transform.position.x - 22) < 1e-4f);
    // keepWorld into another tree whose root also moved since the flush
    auto& other = scene.createObject<GameObject>();
    other.transform.position = {0, 0}; scene.flush();
    other.transform.position = {0, 5};
    CHECK(scene.setParent(leaf.id, other.id, true));
    CHECK(std::abs(leaf.transform.position.x - 22) < 1e-4f && std::abs(leaf.transform.position.y + 5) < 1e-4f);
    CHECK(std::abs(leaf.world().position.x - 22) < 1e-4f && std::abs(leaf.world().position.y) < 1e-4f);
}

TEST(grid_ignores_nonfinite_and_survives_huge_boxes) {
    const float inf = std::numeric_limits<float>::infinity(), nan = std::numeric_limits<float>::quiet_NaN();
    SpatialGrid grid;
    auto id = [](std::uint32_t i) { return GameObjectId{i, 1}; };
    grid.insert(id(1), AABB{{0, 0}, {1, 1}});
    grid.insert(id(2), AABB{{0, 0}, {inf, 1}});
    grid.insert(id(3), AABB{{nan, 0}, {1, 1}});
    grid.insert(id(4), AABB{{-1e30f, -1e30f}, {1e30f, 1e30f}});
    grid.insert(id(5), AABB{{0, 0}, {2000, 1}}); // wider than kMaxCells cells
    std::vector<GameObjectId> got;
    grid.query(AABB{{0.2f, 0.2f}, {0.4f, 0.4f}}, got);
    CHECK(std::find(got.begin(), got.end(), id(1)) != got.end());
    CHECK(std::find(got.begin(), got.end(), id(2)) == got.end() && std::find(got.begin(), got.end(), id(3)) == got.end());
    grid.query(AABB{{-1e30f, -1e30f}, {1e30f, 1e30f}}, got); // too wide to index: scans entries
    CHECK((got == std::vector<GameObjectId>{id(1), id(4), id(5)}));
    grid.query(AABB{{0, 0}, {inf, 1}}, got);
    CHECK(got.empty());
    grid.update(id(1), AABB{{0, 0}, {nan, 1}}); // a stored box that turns non-finite is dropped
    grid.query(AABB{{-1e30f, -1e30f}, {1e30f, 1e30f}}, got);
    CHECK((got == std::vector<GameObjectId>{id(4), id(5)}));

    Scene2D scene;
    auto& bad = scene.createObject<GameObject>();
    bad.collider = ColliderData{}; bad.collider->size = {inf, 1};
    scene.step(0.016f);
    CHECK(Collision2D::queryAABB(scene, AABB{{-1, -1}, {1, 1}}).empty());
}

TEST(spatial_grid_wide_entry_found_by_narrow_query) {
    SpatialGrid grid;
    GameObjectId wide{7, 1};
    grid.insert(wide, AABB{{0, 0}, {2000, 1}}); // beyond kMaxCells cells: only partly indexed
    std::vector<GameObjectId> got;
    grid.query(AABB{{1000, 0.2f}, {1001, 0.4f}}, got);
    CHECK((got == std::vector<GameObjectId>{wide}));
    grid.remove(wide);
    grid.query(AABB{{1000, 0.2f}, {1001, 0.4f}}, got);
    CHECK(got.empty());
}

TEST(dispatch_handler_can_destroy_create_and_reparent) {
    Scene2D scene;
    GameObjectId trigger = scene.createObject<Counted>().id, victim = scene.createObject<Counted>().id;
    GameObjectId parent = scene.createObject<Counted>().id, kid = scene.createObject<Counted>().id;
    GameObjectId made;
    bool reparented = false;
    auto sub = scene.events().subscribe<ObjectDestroyed>([&](const ObjectDestroyed& e) {
        if (e.id != trigger) return;
        scene.destroy(victim);
        made = scene.createObject<Counted>().id;
        reparented = scene.setParent(kid, parent);
    });
    scene.destroy(trigger);
    scene.flush(); // the handler runs inside this flush's dispatch
    CHECK(reparented && scene.find(kid)->parent() == parent && scene.find(parent)->children().size() == 1);
    CHECK(scene.find(victim) && scene.find(victim)->pendingDestroy());
    CHECK(scene.find(made) != nullptr);
    scene.flush();
    CHECK(!scene.find(victim) && !scene.find(trigger));
    CHECK(scene.find(made) != nullptr && scene.aliveCount() == 3);
}

namespace {
int reparentOk = 0, reparentIntoSelf = 1;
struct Reparenter : GameObject {
    GameObjectId mover, newParent;
    void onDestroy(Scene2D& s) override {
        reparentOk = s.setParent(mover, newParent);   // allowed: both survive
        reparentIntoSelf = s.setParent(mover, id);    // refused: this object is pending destroy
    }
};
}

TEST(setParent_inside_onDestroy) {
    Scene2D scene;
    GameObjectId oldParent = scene.createObject<GameObject>().id, mover = scene.createObject<GameObject>().id;
    GameObjectId newParent = scene.createObject<GameObject>().id;
    scene.setParent(mover, oldParent);
    auto& r = scene.createObject<Reparenter>();
    r.mover = mover; r.newParent = newParent;
    scene.destroy(r.id);
    scene.flush();
    CHECK(reparentOk == 1 && reparentIntoSelf == 0);
    CHECK(scene.find(mover)->parent() == newParent);
    CHECK(scene.find(oldParent)->children().empty() && scene.find(newParent)->children().size() == 1);
    CHECK(scene.aliveCount() == 3);
}

namespace {
LdtkLayer gridLayer(int cw, int ch, std::initializer_list<std::pair<glm::ivec2, int>> cells) {
    LdtkLayer L; L.identifier = "Collision"; L.type = "IntGrid"; L.gridSize = 16; L.cells = {cw, ch};
    L.intGrid.assign(size_t(cw) * ch, 0);
    for (auto& [c, v] : cells) L.intGrid[size_t(c.y) * cw + c.x] = v;
    return L;
}
std::vector<Segment> gridSegs(const LdtkLayer& L) {
    return segmentsFromIntGrid(L, {0, 0}, L.cells * L.gridSize, "r");
}
bool hasNormal(const Segment& s, glm::vec2 n) { return glm::length(s.normal - n) < 1e-4f; }
template <class P> int countIf(const std::vector<Segment>& v, P pred) { int n = 0; for (auto& s : v) n += pred(s) ? 1 : 0; return n; }
bool isDiag(const Segment& s) { return std::abs(s.normal.x) > 0.01f && std::abs(s.normal.y) > 0.01f; }

LdtkEntity terrainEntity(glm::vec2 px, std::initializer_list<glm::ivec2> pts, bool closed, float thickness, bool oneWay) {
    LdtkEntity e; e.identifier = "Terrain"; e.px = px; e.layerGridPx = 4;
    e.fields = nlohmann::json::object();
    auto arr = nlohmann::json::array();
    for (auto p : pts) arr.push_back({{"cx", p.x}, {"cy", p.y}});
    e.fields["points"] = arr; e.fields["closed"] = closed; e.fields["thickness"] = thickness; e.fields["oneWay"] = oneWay;
    return e;
}
}

TEST(intgrid_segments_outer_edges_only) {
    auto segs = gridSegs(gridLayer(6, 4, {{{1,1},1},{{2,1},1},{{3,1},1},{{1,2},1},{{2,2},1},{{3,2},1}}));
    CHECK(segs.size() == 4);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {0, 1}); }) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {0, -1}); }) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {1, 0}); }) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {-1, 0}); }) == 1);
    for (auto& s : segs) {
        const float len = glm::length(s.b - s.a);
        CHECK(std::abs(len - (std::abs(s.normal.y) > 0.5f ? 1.5f : 1.0f)) < 1e-4f);
        CHECK(s.roomIid == "r" && !s.oneWay);
    }
}

TEST(intgrid_slopes_and_oneway) {
    // two 45 ramps up-right, diagonal neighbours, over a block -> one diagonal of 1 x 1 u
    auto a = gridSegs(gridLayer(5, 4, {{{1,2},3},{{2,1},3},{{2,2},1}}));
    CHECK(countIf(a, isDiag) == 1);
    for (auto& s : a) if (isDiag(s)) {
        CHECK(std::abs(glm::length(s.b - s.a) - std::sqrt(2.0f)) < 1e-4f);
        CHECK(s.normal.x < 0 && s.normal.y > 0); // faces up-left, towards the air
    }
    // 5 + 7: one continuous 22.5 deg ramp over 2 cells (1.0 x 0.5 u)
    auto b = gridSegs(gridLayer(5, 4, {{{1,2},5},{{2,2},7}}));
    CHECK(countIf(b, isDiag) == 1);
    for (auto& s : b) if (isDiag(s)) CHECK(std::abs(std::abs(s.b.x - s.a.x) - 1.0f) < 1e-4f && std::abs(std::abs(s.b.y - s.a.y) - 0.5f) < 1e-4f);
    // mirrored 8 + 6 also merge; 5 next to 6 does not
    CHECK(countIf(gridSegs(gridLayer(5, 4, {{{1,2},8},{{2,2},6}})), isDiag) == 1);
    CHECK(countIf(gridSegs(gridLayer(5, 4, {{{1,2},5},{{2,2},6}})), isDiag) == 2);
    // 2 next to 1: one-way top only, and the 1's own top stays solid
    auto c = gridSegs(gridLayer(5, 4, {{{1,1},2},{{2,1},1}}));
    CHECK(countIf(c, [](auto& s) { return s.oneWay; }) == 1);
    for (auto& s : c) if (s.oneWay) CHECK(hasNormal(s, {0, 1}) && std::abs(glm::length(s.b - s.a) - 0.5f) < 1e-4f);
    CHECK(countIf(c, [](auto& s) { return !s.oneWay && hasNormal(s, {0, 1}); }) == 1);
}

TEST(intgrid_no_faces_on_room_border) {
    auto segs = gridSegs(gridLayer(2, 2, {{{0,0},1},{{1,0},1},{{0,1},1},{{1,1},1}}));
    CHECK(segs.empty()); // no faces on the room border: sides, top and bottom
}

TEST(terrain_points_to_world) {
    // entity at px (64,32) == cell (16,8): the duplicate first point is dropped
    auto e = terrainEntity({64, 32}, {{16, 8}, {32, 8}}, false, 3.0f, false);
    auto segs = segmentsFromTerrain(e, {320, -160}, 4, "r");
    CHECK(segs.size() == 4);
    // surface (12,4)->(14,4) with normal up; bottom 3 u below
    CHECK(countIf(segs, [](auto& s) {
        return hasNormal(s, {0, 1}) && std::min(s.a.x, s.b.x) == 12.0f && std::max(s.a.x, s.b.x) == 14.0f && s.a.y == 4.0f && s.b.y == 4.0f;
    }) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {0, -1}) && s.a.y == 1.0f; }) == 1);
}

TEST(terrain_orientation_both_windings) {
    // 32 px square (cells 0..8) in both windings; normals must point away from the centre
    const glm::vec2 centre = ldtkToWorld({0, 0}, {16, 16});
    auto cw = terrainEntity({0, 0}, {{8, 0}, {8, 8}, {0, 8}}, true, 3.0f, false);
    auto ccw = terrainEntity({0, 0}, {{0, 8}, {8, 8}, {8, 0}}, true, 3.0f, false);
    for (auto* e : {&cw, &ccw}) {
        auto segs = segmentsFromTerrain(*e, {0, 0}, 4, "r");
        CHECK(segs.size() == 4);
        for (auto& s : segs) CHECK(glm::dot(s.normal, (s.a + s.b) * 0.5f - centre) > 0.0f);
    }
}

TEST(terrain_open_surface_thickness) {
    // drawn right to left: still solid below the line, normal up on top
    auto e = terrainEntity({128, 0}, {{0, 0}}, false, 2.0f, false);
    auto segs = segmentsFromTerrain(e, {0, 0}, 4, "r");
    CHECK(segs.size() == 4);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {0, 1}) && s.a.y == 0.0f; }) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {0, -1}) && s.a.y == -2.0f; }) == 1);
    // one-way keeps only the upward-facing run
    auto o = segmentsFromTerrain(terrainEntity({0, 0}, {{16, 0}, {32, 8}}, false, 2.0f, true), {0, 0}, 4, "r");
    CHECK(!o.empty());
    for (auto& s : o) CHECK(s.oneWay && s.normal.y > 0.7f);
}

namespace {
// edge = albedo 1, side = 2, fill = 3
TerrainStyle meshStyle() {
    TerrainStyle s;
    s.edge.albedo = 1; s.side.albedo = 2; s.fill.albedo = 3;
    return s;
}
std::vector<Segment> polyline(std::initializer_list<glm::vec2> pts, bool closed) {
    std::vector<glm::vec2> v(pts);
    std::vector<Segment> out;
    for (size_t i = 0; i + (closed ? 0 : 1) < v.size(); ++i) {
        Segment s; s.a = v[i]; s.b = v[(i + 1) % v.size()];
        out.push_back(s);
    }
    return out;
}
std::vector<SpriteInstance> only(const std::vector<SpriteInstance>& all, uint32_t albedo) {
    std::vector<SpriteInstance> r;
    for (auto& s : all) if (s.albedo == albedo) r.push_back(s);
    return r;
}
}

TEST(terrain_mesh_covers_columns) {
    // flat open surface 4 u wide, 2.5 u thick: fill area = 10, down to y = -2.5, columns without holes
    std::vector<SpriteInstance> out;
    buildTerrainMesh(polyline({{0, 0}, {4, 0}}, false), false, 2.5f, meshStyle(), out);
    auto fill = only(out, 3);
    float area = 0.0f, bottom = 0.0f, top = -1e9f;
    for (auto& s : fill) {
        area += s.size.x * s.size.y;
        bottom = std::min(bottom, s.position.y - s.size.y * 0.5f);
        top = std::max(top, s.position.y + s.size.y * 0.5f);
        CHECK(s.size.y <= 1.0f + 1e-4f && s.uvRect.w <= 1.0f + 1e-4f && s.uvRect.y >= 0.0f);
        CHECK(s.uvRect.x >= 0.0f && s.uvRect.x + s.uvRect.z <= 1.0f + 1e-4f); // never leaves its tile
    }
    CHECK(std::abs(area - 10.0f) < 1e-3f && std::abs(bottom + 2.5f) < 1e-4f && std::abs(top) < 1e-4f);
    // closed hill: per column, cells touch (no gaps) and stay under the surface
    out.clear();
    buildTerrainMesh(polyline({{0, 0}, {4, 0}, {2, 2}}, true), true, 0.0f, meshStyle(), out);
    std::map<int, std::vector<SpriteInstance>> cols;
    for (auto& s : only(out, 3)) cols[int(std::floor(s.position.x * 100))].push_back(s);
    CHECK(cols.size() == 6); // the two base-corner columns shrink to nothing: the edge pieces cover those corners
    for (auto& [x, v] : cols) {
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.position.y > b.position.y; });
        for (size_t i = 0; i + 1 < v.size(); ++i)
            CHECK(std::abs((v[i].position.y - v[i].size.y * 0.5f) - (v[i + 1].position.y + v[i + 1].size.y * 0.5f)) < 1e-4f);
        const float roof = 2.0f - std::abs(v[0].position.x - 2.0f); // triangle surface at this column
        CHECK(v[0].position.y + v[0].size.y * 0.5f <= roof + 1e-3f);
        CHECK(std::abs(v.back().position.y - v.back().size.y * 0.5f) < 1e-4f); // down to the base line
    }
}

TEST(terrain_edge_follows_surface) {
    // 2 u flat, then a 45 deg ramp 2.83 u long: pieces = ceil(length), rotated to the angle
    std::vector<SpriteInstance> out;
    buildTerrainMesh(polyline({{0, 0}, {2, 0}, {4, 2}}, false), false, 3.0f, meshStyle(), out);
    auto edge = only(out, 1);
    int flat = 0, ramp = 0;
    float flatLen = 0.0f, rampLen = 0.0f;
    for (auto& s : edge) {
        if (s.rotation == 0.0f) { ++flat; flatLen += s.size.x; }
        else if (std::abs(s.rotation - glm::quarter_pi<float>()) < 1e-4f) { ++ramp; rampLen += s.size.x; }
        else CHECK(false);
        CHECK(s.uvRect == glm::vec4(0, 0, 1, 1));
    }
    CHECK(flat == 2 && ramp == 3);
    CHECK(flatLen >= 2.0f && flatLen < 2.2f && rampLen >= 2.828f && rampLen < 3.0f);
    CHECK(!only(out, 2).empty()); // sides and bottom use the second edge
    for (auto& s : edge) if (s.rotation != 0.0f) { // ramp pieces hang under the line, along its normal
        const glm::vec2 on = s.position + glm::vec2(-0.70710678f, 0.70710678f) * (0.25f - 0.0625f);
        CHECK(std::abs(on.y - (on.x - 2.0f)) < 1e-4f);
    }
    // a thin open shape (a branch, 0.25 u) gets strips no deeper than itself, cropped in uv, not stretched
    out.clear();
    buildTerrainMesh(polyline({{0, 0}, {2, 0}}, false), false, 0.25f, meshStyle(), out);
    for (int material : {1, 2})
        for (auto& s : only(out, material))
            CHECK(s.size.y <= 0.25f + 0.0625f + 1e-4f && std::abs(s.uvRect.w - s.size.y / 0.5f) < 1e-4f);
}

TEST(collision_world_add_query_remove) {
    CollisionWorld w;
    w.addRoom("a", gridSegs(gridLayer(4, 4, {{{1,2},1}})));
    std::vector<const Segment*> out;
    w.query({{0.0f, -2.0f}, {4.0f, 0.0f}}, out);
    CHECK(out.size() == 4);
    w.query({{50.0f, 50.0f}, {51.0f, 51.0f}}, out);
    CHECK(out.empty());
    w.removeRoom("a");
    w.query({{0.0f, -2.0f}, {4.0f, 0.0f}}, out);
    CHECK(out.empty());
}

TEST(intgrid_22_slope_world_coords) {
    // 5 + 7 at the level origin: one diagonal from (0,-0.5) up to (1,0), facing up-left
    auto a = gridSegs(gridLayer(4, 1, {{{0,0},5},{{1,0},7}}));
    CHECK(countIf(a, isDiag) == 1);
    for (auto& s : a) if (isDiag(s)) {
        const glm::vec2 lo = s.a.x < s.b.x ? s.a : s.b, hi = s.a.x < s.b.x ? s.b : s.a;
        CHECK(lo == glm::vec2(0.0f, -0.5f) && hi == glm::vec2(1.0f, 0.0f));
        CHECK(s.normal.x < 0 && s.normal.y > 0);
    }
    // each half on its own: left / right heights of the diagonal for 5, 6, 7, 8 in cell (1,0)
    const std::pair<int, glm::vec2> cases[] = {{5, {-0.5f, -0.25f}}, {6, {-0.25f, -0.5f}}, {7, {-0.25f, 0.0f}}, {8, {0.0f, -0.25f}}};
    for (auto& [v, h] : cases) {
        auto segs = gridSegs(gridLayer(4, 1, {{{1,0},v}}));
        CHECK(countIf(segs, isDiag) == 1);
        for (auto& s : segs) if (isDiag(s)) {
            const glm::vec2 l = s.a.x < s.b.x ? s.a : s.b, r = s.a.x < s.b.x ? s.b : s.a;
            CHECK(l == glm::vec2(0.5f, h.x) && r == glm::vec2(1.0f, h.y));
            CHECK((s.normal.x < 0) == (h.y > h.x) && s.normal.y > 0); // faces away from the rise
        }
    }
}

TEST(intgrid_ramp_beside_and_on_block_count) {
    // ramp 3 left of a block and ramp 3 on top of it: a triangle (bottom, right wall, one diagonal)
    auto segs = gridSegs(gridLayer(6, 4, {{{1,2},3},{{2,2},1},{{2,1},3}}));
    CHECK(segs.size() == 3);
    CHECK(countIf(segs, isDiag) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {0, -1}) && std::abs(glm::length(s.b - s.a) - 1.0f) < 1e-4f; }) == 1);
    CHECK(countIf(segs, [](auto& s) { return hasNormal(s, {1, 0}) && std::abs(glm::length(s.b - s.a) - 1.0f) < 1e-4f; }) == 1);
}

namespace {
constexpr float kDt = 1.0f / 60.0f;

void setCells(LdtkLayer& L, int y, int x0, int x1, int v) { for (int x = x0; x <= x1; ++x) L.intGrid[size_t(y) * L.cells.x + x] = v; }

std::vector<Segment> polySegs(std::initializer_list<glm::vec2> ccw, bool oneWay = false) {
    std::vector<glm::vec2> p(ccw);
    std::vector<Segment> out;
    for (size_t i = 0; i < p.size(); ++i) {
        const glm::vec2 a = p[i], b = p[(i + 1) % p.size()], d = b - a;
        out.push_back({a, b, glm::normalize(glm::vec2(d.y, -d.x)), oneWay, "r"});
    }
    return out;
}

// The old stepPlatformer loop (input, jump, gravity) driving the controller.
struct Sim {
    Body body;
    glm::vec2 v{0.0f};
    bool grounded = false;
    MoveResult r;
    CollisionQuery q;
    Sim(const CollisionWorld* w, glm::vec2 pos) { q.world = w; body.position = pos; }
    void step(float input, bool jump = false, float dt = kDt) {
        v.x = nextVelocityX(r, input * 4.5f);
        bool jumped = false;
        if (jump && grounded) { v.y = 11.0f; jumped = true; }
        v.y = std::max(v.y - 32.0f * dt, -18.0f);
        r = move(body, v, dt, q, grounded, jumped);
        v = r.velocity;
        grounded = r.grounded;
    }
    void settle(float seconds) { for (int i = 0; i < int(seconds / kDt); ++i) step(0.0f); }
    float bottom() const { return body.position.y - body.halfSize.y; }
};

// 20 x 8 cells: floor top y = -3.5, 45 deg ramp from (3,-3.5) to (4.5,-2), flat top y = -2.
LdtkLayer rampLayer() {
    LdtkLayer L = gridLayer(20, 8, {});
    setCells(L, 7, 0, 19, 1);
    setCells(L, 6, 6, 6, 3); setCells(L, 6, 7, 19, 1);
    setCells(L, 5, 7, 7, 3); setCells(L, 5, 8, 19, 1);
    setCells(L, 4, 8, 8, 3); setCells(L, 4, 9, 19, 1);
    return L;
}
}

TEST(controller_walks_up_and_down_45) {
    CollisionWorld w; w.addRoom("r", gridSegs(rampLayer()));
    Sim s(&w, {1.0f, -2.6f});
    s.settle(0.5f);
    CHECK(s.grounded && std::abs(s.bottom() + 3.5f) < 0.01f);
    int lost = 0, frames = 0;
    while (s.body.position.x < 7.0f && frames++ < 600) { s.step(1.0f); lost += s.grounded ? 0 : 1; }
    CHECK(lost == 0);
    CHECK(s.body.position.x >= 7.0f && std::abs(s.bottom() + 2.0f) < 0.01f);
    frames = 0;
    while (s.body.position.x > 1.5f && frames++ < 600) { s.step(-1.0f); lost += s.grounded ? 0 : 1; }
    CHECK(lost == 0);
    CHECK(s.body.position.x <= 1.5f && std::abs(s.bottom() + 3.5f) < 0.01f);
}

TEST(controller_rests_on_slope) {
    CollisionWorld w; w.addRoom("r", gridSegs(rampLayer()));
    Sim s(&w, {3.75f, -1.0f});
    s.settle(1.0f);
    CHECK(s.grounded);
    const glm::vec2 p = s.body.position;
    s.settle(2.0f);
    CHECK(s.grounded && glm::length(s.body.position - p) < 0.01f);
}

TEST(controller_rests_on_slope_vertex) {
    // valley \/ and peak /\ at x = 3 over a floor at y = -3.5; also rest on the ramp-to-flat vertices
    LdtkLayer valley = gridLayer(12, 8, {});
    setCells(valley, 7, 0, 11, 1); setCells(valley, 6, 0, 4, 1); setCells(valley, 6, 5, 5, 4);
    setCells(valley, 6, 6, 6, 3); setCells(valley, 6, 7, 11, 1);
    LdtkLayer peak = gridLayer(12, 8, {});
    setCells(peak, 7, 0, 11, 1); setCells(peak, 6, 5, 5, 3); setCells(peak, 6, 6, 6, 4);
    for (auto* L : {&valley, &peak})
        for (float x : {3.0f, 2.5f, 3.5f}) {
            CollisionWorld w; w.addRoom("r", gridSegs(*L));
            Sim s(&w, {x, -1.5f});
            s.settle(1.0f);
            CHECK(s.grounded);
            const glm::vec2 p = s.body.position;
            s.settle(2.0f);
            CHECK(s.grounded && glm::length(s.body.position - p) < 0.01f);
        }
}

TEST(controller_slides_on_steep) {
    // 60 deg surface from (2,0) up to (3,1.732), floor y = 0 to its left
    const float h = std::sqrt(3.0f);
    CollisionWorld w; w.addRoom("r", polySegs({{-5, -5}, {10, -5}, {10, h}, {3, h}, {2, 0}, {-5, 0}}));
    Sim s(&w, {2.3f, 3.0f});
    int steep = 0, frames = 0;
    float prevY = s.body.position.y;
    while (!s.grounded && frames++ < 300) {
        s.step(steep ? 1.0f : 0.0f, true); // once on it: pushing into the slope and mashing jump
        if (s.r.onSteep) {
            ++steep;
            CHECK(s.v.y <= 0.0f);                       // never jumped
            CHECK(s.body.position.y <= prevY + 1e-4f);  // input does not climb it
            CHECK(std::abs(s.r.groundNormal.y - 0.5f) < 1e-3f);
        }
        prevY = s.body.position.y;
    }
    CHECK(steep > 3);
    CHECK(s.grounded && std::abs(s.bottom()) < 0.01f && s.body.position.x < 2.3f); // slid down to the floor
}

TEST(controller_no_snag_on_seams) {
    // flat, 22.5 up (5,7), 45 up (3), flat top, 45 down (4), 22.5 down (8,6), flat, then a hole filled
    // by a solid box flush with the floor (segment-to-box seams)
    LdtkLayer L = gridLayer(40, 10, {});
    setCells(L, 9, 0, 39, 1); setCells(L, 8, 0, 39, 1); setCells(L, 8, 24, 25, 0);
    setCells(L, 7, 10, 10, 5); setCells(L, 7, 11, 11, 7); setCells(L, 7, 12, 17, 1);
    setCells(L, 7, 18, 18, 8); setCells(L, 7, 19, 19, 6);
    setCells(L, 6, 12, 12, 3); setCells(L, 6, 13, 16, 1); setCells(L, 6, 17, 17, 4);
    CollisionWorld w; w.addRoom("r", gridSegs(L));
    Sim s(&w, {1.0f, -3.0f});
    s.q.solids.push_back({{12.0f, -4.5f}, {13.0f, -4.0f}});
    s.settle(0.5f);
    CHECK(s.grounded);
    int snags = 0, lost = 0, frames = 0;
    for (float dir : {1.0f, -1.0f}) {
        frames = 0;
        while ((dir > 0 ? s.body.position.x < 15.0f : s.body.position.x > 1.0f) && frames++ < 600) {
            const float x0 = s.body.position.x;
            s.step(dir);
            snags += (s.body.position.x - x0) * dir < 0.9f * 4.5f * kDt || s.v.x == 0.0f ? 1 : 0;
            lost += s.grounded ? 0 : 1;
        }
        CHECK(frames < 600);
    }
    CHECK(snags == 0);
    CHECK(lost == 0);
}

TEST(controller_oneway_from_below_and_drop) {
    LdtkLayer L = gridLayer(12, 10, {});
    setCells(L, 9, 0, 11, 1); setCells(L, 6, 3, 8, 2); // floor top -4.5, one-way top -3.0 (x 1.5..4.5)
    CollisionWorld w; w.addRoom("r", gridSegs(L));
    Sim s(&w, {3.0f, -3.6f});
    s.settle(0.3f);
    CHECK(s.grounded && std::abs(s.bottom() + 4.5f) < 0.01f && !s.r.onOneWay);
    CHECK(!requestDropThrough(s.body, s.r)); // solid floor: a normal jump
    s.step(0.0f, true);
    for (int i = 0; i < 200 && !s.grounded; ++i) s.step(0.0f);
    CHECK(s.grounded && s.r.onOneWay && std::abs(s.bottom() + 3.0f) < 0.01f);
    s.settle(0.2f);
    CHECK(s.grounded && s.r.onOneWay);
    CHECK(requestDropThrough(s.body, s.r));
    int frames = 0;
    do { s.step(0.0f); } while (!s.grounded && ++frames < 200);
    CHECK(frames > 2 && std::abs(s.bottom() + 4.5f) < 0.01f);
}

TEST(controller_drops_through_branch_vertex) {
    // A one-way Terrain branch is a chain of segments: standing on a vertex touches two of them.
    CollisionWorld w;
    w.addRoom("r", polySegs({{-10, -5}, {10, -5}, {10, -4}, {-10, -4}})); // floor top -4
    auto oneWay = [](glm::vec2 a, glm::vec2 b) { const glm::vec2 d = b - a; return Segment{a, b, glm::normalize(glm::vec2(d.y, -d.x)), true, "o"}; };
    w.addRoom("o", {oneWay({0.0f, -1.0f}, {-1.0f, -1.1f}), oneWay({1.0f, -1.1f}, {0.0f, -1.0f})}); // peak at x 0, right to left: normals up
    Sim s(&w, {0.0f, -0.1f});
    s.settle(0.3f);
    CHECK(s.grounded && s.r.onOneWay && std::abs(s.bottom() + 1.0f) < 0.02f);
    CHECK(requestDropThrough(s.body, s.r));
    int frames = 0;
    do { s.step(0.0f); } while (!s.grounded && ++frames < 200);
    CHECK(std::abs(s.bottom() + 4.0f) < 0.01f); // through both segments, down to the floor
}

TEST(oneway_next_to_solid) {
    // one-way top (x 1..2.5, y -1) beside a 2 x 1 block (x 2.5..3.5), floor top y = -2.5
    LdtkLayer L = gridLayer(10, 6, {});
    setCells(L, 5, 0, 9, 1); setCells(L, 2, 2, 4, 2); setCells(L, 2, 5, 6, 1);
    auto segs = gridSegs(L);
    CHECK(segs.size() == 6); // floor top (its bottom is on the room border), one-way top, block top/bottom/left/right
    CHECK(countIf(segs, [](auto& s) { return s.oneWay; }) == 1);
    CHECK(countIf(segs, [](auto& s) { return s.oneWay && std::abs(glm::length(s.b - s.a) - 1.5f) < 1e-4f; }) == 1);
    CollisionWorld w; w.addRoom("r", segs);
    Sim s(&w, {1.6f, 0.5f});
    s.settle(1.0f);
    CHECK(s.grounded && s.r.onOneWay && std::abs(s.bottom() + 1.0f) < 0.01f);
    int lost = 0, frames = 0;
    while (s.body.position.x < 3.2f && frames++ < 300) { s.step(1.0f); lost += s.grounded ? 0 : 1; }
    CHECK(!s.r.onOneWay && std::abs(s.bottom() + 1.0f) < 0.01f);
    frames = 0;
    while (s.body.position.x > 1.6f && frames++ < 300) { s.step(-1.0f); lost += s.grounded ? 0 : 1; }
    CHECK(lost == 0 && s.r.onOneWay);
    CHECK(requestDropThrough(s.body, s.r));
    for (int i = 0; i < 200; ++i) s.step(0.0f);
    CHECK(s.grounded && std::abs(s.bottom() + 2.5f) < 0.01f);
}

TEST(controller_no_tunnel_at_max_fall) {
    CollisionWorld solid; solid.addRoom("r", polySegs({{0, 0}, {4, 0}, {4, 0.25f}, {0, 0.25f}}));
    CollisionWorld oneWay; oneWay.addRoom("r", {Segment{{4, 0.25f}, {0, 0.25f}, {0, 1}, true, "r"}});
    for (auto* w : {&solid, &oneWay}) {
        Sim s(w, {2.0f, 10.0f});
        s.v = {0.0f, -18.0f};
        for (int i = 0; i < 20; ++i) s.step(0.0f, false, 0.1f);
        CHECK(s.grounded && std::abs(s.bottom() - 0.25f) < 0.01f);
    }
}

TEST(controller_ceiling_stops_vy) {
    LdtkLayer L = gridLayer(10, 10, {});
    setCells(L, 9, 0, 9, 1); setCells(L, 4, 2, 6, 1); // floor top -4.5, ceiling bottom -2.5
    CollisionWorld w; w.addRoom("r", gridSegs(L));
    Sim s(&w, {2.5f, -3.6f});
    s.settle(0.3f);
    s.step(0.0f, true);
    int hits = 0;
    for (int i = 0; i < 120; ++i) {
        s.step(0.0f);
        if (s.r.hitCeiling) { ++hits; CHECK(s.v.y <= 0.0f); }
        CHECK(s.body.position.y + s.body.halfSize.y <= -2.5f + 1e-3f);
    }
    CHECK(hits >= 1);
    CHECK(s.grounded && std::abs(s.bottom() + 4.5f) < 0.01f);
}

TEST(controller_flat_jump_matches_old) {
    // decision 3: jump ~3.8 cells (1.9 u) high and land on the same y
    LdtkLayer L = gridLayer(10, 10, {});
    setCells(L, 9, 0, 9, 1);
    CollisionWorld w; w.addRoom("r", gridSegs(L));
    Sim s(&w, {2.5f, -3.6f});
    s.settle(0.3f);
    const float y0 = s.bottom();
    s.step(0.0f, true);
    float apex = y0;
    for (int i = 0; i < 120; ++i) { s.step(0.0f); apex = std::max(apex, s.bottom()); }
    float expected = 0.0f; // the old integration: vy = 11, then vy += g dt before each move
    for (float vy = 11.0f - 32.0f * kDt; vy > 0.0f; vy -= 32.0f * kDt) expected += vy * kDt;
    CHECK(std::abs(apex - y0 - expected) < 0.01f && expected > 1.75f);
    CHECK(s.grounded && std::abs(s.bottom() - y0) < 1e-3f && s.v.y == 0.0f);
}

TEST(controller_speed_on_slope_matches_flat) {
    const float h = 8.0f * std::tan(glm::radians(22.5f));
    CollisionWorld w; w.addRoom("r", polySegs({{-10, -5}, {20, -5}, {20, h}, {8, h}, {0, 0}, {-10, 0}}));
    auto walk = [&](glm::vec2 start, float dir) {
        Sim s(&w, start);
        s.settle(0.5f);
        const float x0 = s.body.position.x;
        for (int i = 0; i < 60; ++i) s.step(dir);
        CHECK(s.grounded);
        return std::abs(s.body.position.x - x0);
    };
    const float flat = walk({-8.0f, 1.0f}, 1.0f);
    CHECK(std::abs(flat - 4.5f) < 0.05f);
    CHECK(std::abs(walk({1.5f, 2.0f}, 1.0f) / flat - 1.0f) < 0.05f);  // up
    CHECK(std::abs(walk({7.0f, 4.5f}, -1.0f) / flat - 1.0f) < 0.05f); // down
}

TEST(controller_steep_slide_frame_rate_independent) {
    // 60 deg surface rising right from (0,0) to (6, 10.4); floor y = 0 to its left
    const float H = 6.0f * std::tan(glm::radians(60.0f));
    CollisionWorld w; w.addRoom("r", polySegs({{-20, -5}, {20, -5}, {20, H}, {6, H}, {0, 0}, {-20, 0}}));
    auto slide = [&](float dt) { // speeds 0.25 s and 0.5 s after touching the slope
        Sim s(&w, {5.0f, 10.4f});
        for (int i = 0; i < 400 && !s.r.onSteep; ++i) s.step(0.0f, false, dt);
        CHECK(s.r.onSteep);
        const int n = int(std::lround(0.5f / dt));
        glm::vec2 out{0.0f};
        for (int i = 1; i <= n; ++i) {
            s.step(0.0f, false, dt);
            CHECK(s.r.onSteep);
            if (i == n / 2) out.x = glm::length(s.v);
        }
        out.y = glm::length(s.v);
        return out;
    };
    const glm::vec2 a = slide(1.0f / 60.0f), b = slide(1.0f / 144.0f);
    CHECK(std::abs(a.y / b.y - 1.0f) < 0.1f);
    const float g = 32.0f * std::sin(glm::radians(60.0f));
    for (glm::vec2 v : {a, b}) CHECK(std::abs((v.y - v.x) / 0.25f / g - 1.0f) < 0.1f);
}

TEST(controller_wedge_is_stable) {
    // 45 deg ramp from (0,0) up to (6,6) between floors y = 0 and y = 6. A wall x 2..3 cuts the ramp:
    // walk up into its left face and down into its right face. Then a ceiling at y = 3.2 over the ramp.
    for (int scenario = 0; scenario < 3; ++scenario) {
        CollisionWorld w; w.addRoom("r", polySegs({{-10, -5}, {20, -5}, {20, 6}, {6, 6}, {0, 0}, {-10, 0}}));
        if (scenario < 2) w.addRoom("x", polySegs({{2, 1}, {3, 1}, {3, 10}, {2, 10}}));
        else w.addRoom("x", polySegs({{-1, 3.2f}, {6, 3.2f}, {6, 5}, {-1, 5}}));
        const float dir = scenario == 1 ? -1.0f : 1.0f;
        Sim s(&w, scenario == 1 ? glm::vec2(8.0f, 6.9f) : glm::vec2(-2.0f, 0.9f));
        s.settle(0.3f);
        for (int i = 0; i < 180; ++i) s.step(dir);
        glm::vec2 lo = s.body.position, hi = lo;
        int toggles = 0;
        bool ceiling = s.r.hitCeiling;
        for (int i = 0; i < 60; ++i) {
            s.step(dir);
            lo = glm::min(lo, s.body.position); hi = glm::max(hi, s.body.position);
            toggles += s.r.hitCeiling != ceiling ? 1 : 0;
            ceiling = s.r.hitCeiling;
        }
        CHECK(hi.x - lo.x <= 0.01f && hi.y - lo.y <= 0.01f);
        CHECK(toggles == 0);
        CHECK(s.body.position.x > 0.5f && s.body.position.x < 5.5f); // it did reach the wedge
    }
}

TEST(controller_walks_onto_oneway_from_root) {
    // flat floor y = 0; a one-way segment rising right at 35 deg from (2,0) to (5,2.1)
    const glm::vec2 hiEnd{5.0f, 3.0f * std::tan(glm::radians(35.0f))}, root{2.0f, 0.0f};
    const glm::vec2 d = root - hiEnd;
    CollisionWorld w;
    w.addRoom("r", polySegs({{-10, -5}, {20, -5}, {20, 0}, {-10, 0}}));
    w.addRoom("o", {Segment{hiEnd, root, glm::normalize(glm::vec2(d.y, -d.x)), true, "o"}});
    Sim s(&w, {0.0f, 0.85f});
    s.settle(0.3f);
    CHECK(s.grounded);
    bool onOneWay = false;
    for (int i = 0; i < 120 && s.body.position.x < 4.0f; ++i) { s.step(1.0f); onOneWay |= s.r.onOneWay; }
    CHECK(onOneWay && s.r.onOneWay);
    CHECK(s.bottom() > 1.0f);
}

TEST(controller_skips_degenerate_solids) {
    CollisionWorld w; w.addRoom("r", polySegs({{-10, -5}, {10, -5}, {10, 0}, {-10, 0}}));
    Sim s(&w, {0.0f, 2.5f});
    s.q.solids = {{{-1.0f, 1.0f}, {1.0f, 1.0f}}, {{0.1f, -1.0f}, {0.1f, 3.0f}}}; // zero height, zero width
    s.settle(1.0f);
    CHECK(std::isfinite(s.body.position.x) && std::isfinite(s.body.position.y));
    CHECK(s.grounded && std::abs(s.bottom()) < 0.01f);
}

namespace {
PlayerObject& platformerPlayer(Scene2D& scene, glm::vec2 pos) { // the Showcase PlayerStart config
    PlayerObject::Config pc;
    pc.movementMode = PlayerObject::Config::MovementMode::Platformer;
    pc.moveSpeed = 4.5f; pc.colliderSize = {0.6f, 1.6f};
    pc.gravity = -32.0f; pc.jumpSpeed = 11.0f; pc.jumpCutFactor = 0.5f; pc.maxFallSpeed = -18.0f;
    PlayerObject& p = scene.createObject<PlayerObject>(pc);
    p.transform.position = pos;
    return p;
}
void stepPlayer(Scene2D& scene, PlayerObject& p, const InputState& in, int frames = 1) {
    for (int i = 0; i < frames; ++i) { p.applyInput(in); scene.step(kDt); }
}
}

TEST(player_jump_height_unchanged) {
    Scene2D scene;
    LdtkLayer L = gridLayer(20, 10, {});
    setCells(L, 9, 0, 19, 1); // floor top y = -4.5
    scene.collision.addRoom("r", gridSegs(L));
    PlayerObject& p = platformerPlayer(scene, {3.0f, -3.6f});
    stepPlayer(scene, p, {}, 30);
    const float y0 = p.transform.position.y - 0.8f;
    CHECK(p.isGrounded() && std::abs(y0 + 4.5f) < 0.01f);
    InputState jump; jump.jump.pressed = jump.jump.down = true;
    InputState hold; hold.jump.down = true;
    stepPlayer(scene, p, jump);
    float apex = y0;
    for (int i = 0; i < 120; ++i) { stepPlayer(scene, p, hold); apex = std::max(apex, p.transform.position.y - 0.8f); }
    float expected = 0.0f; // the old stepPlatformer: vy = 11, then vy += g dt before each move
    for (float vy = 11.0f - 32.0f * kDt; vy > 0.0f; vy -= 32.0f * kDt) expected += vy * kDt;
    CHECK(std::abs(apex - y0 - expected) < 0.01f && apex - y0 > 1.75f); // ~1.89 u (3.8 cells) at dt -> 0
    CHECK(p.isGrounded() && std::abs(p.transform.position.y - 0.8f - y0) < 1e-3f);
    InputState right; right.moveRight.down = true;
    const float x0 = p.transform.position.x;
    stepPlayer(scene, p, right, 60);
    CHECK(std::abs(p.transform.position.x - x0 - 4.5f) < 0.05f && p.isGrounded());
}

TEST(player_down_jump_drops_through_oneway) {
    Scene2D scene;
    LdtkLayer L = gridLayer(12, 10, {});
    setCells(L, 9, 0, 11, 1); setCells(L, 6, 3, 8, 2); // floor top -4.5, one-way top -3.0 (x 1.5..4.5)
    scene.collision.addRoom("r", gridSegs(L));
    PlayerObject& p = platformerPlayer(scene, {3.0f, -1.5f});
    stepPlayer(scene, p, {}, 30);
    CHECK(p.isGrounded() && std::abs(p.transform.position.y - 0.8f + 3.0f) < 0.01f);
    InputState downJump; downJump.moveDown.down = downJump.moveDown.pressed = true; downJump.jump.pressed = downJump.jump.down = true;
    stepPlayer(scene, p, downJump);
    CHECK(p.velocity.y <= 0.0f); // no jump
    stepPlayer(scene, p, {}, 60);
    CHECK(p.isGrounded() && std::abs(p.transform.position.y - 0.8f + 4.5f) < 0.01f);
    stepPlayer(scene, p, downJump); // on solid ground: a normal jump
    CHECK(p.velocity.y > 5.0f);
}

TEST(slime_turns_at_ledge_on_slope) {
    // floor top -4.0; a 22.5 deg ramp (5, 7) on x 3..4 up to -3.5, then a 0.5 u drop back to -4.0
    LdtkLayer L = gridLayer(20, 10, {});
    setCells(L, 9, 0, 19, 1); setCells(L, 8, 0, 19, 1); setCells(L, 7, 6, 6, 5); setCells(L, 7, 7, 7, 7);
    Scene2D scene;
    scene.collision.addRoom("r", gridSegs(L));
    SlimeEnemy::Config sc;
    sc.patrolDistance = 20.0f;
    SlimeEnemy& slime = scene.createObject<SlimeEnemy>(sc);
    slime.transform.position = {1.5f, -3.6f};
    float maxX = 0.0f, minBottom = 0.0f;
    for (int i = 0; i < 360; ++i) {
        scene.step(kDt);
        maxX = std::max(maxX, slime.transform.position.x);        if (slime.transform.position.x > 3.5f) minBottom = std::min(minBottom, slime.transform.position.y - 0.4f);
    }
    CHECK(maxX > 3.4f && maxX < 4.0f);              // reached the edge, never went over it
    CHECK(minBottom > -3.7f);                        // and stayed on the ramp up there
    CHECK(slime.transform.position.x < maxX - 0.5f); // walking back
}

TEST(slime_walks_down_45_ramp) {
    Scene2D scene;
    scene.collision.addRoom("r", gridSegs(rampLayer())); // flat top -2 from x 4.5 to the room edge at 10
    SlimeEnemy::Config sc;
    sc.patrolDistance = 20.0f;
    SlimeEnemy& slime = scene.createObject<SlimeEnemy>(sc);
    slime.transform.position = {6.0f, -1.6f};
    float minX = 10.0f;
    for (int i = 0; i < 720; ++i) { scene.step(kDt); minX = std::min(minX, slime.transform.position.x); }
    CHECK(minX < 2.5f); // turned at the room edge, then went down the ramp without turning on it
}

TEST(slime_patrols_around_where_it_was_placed) {
    Scene2D scene;
    LdtkLayer L = gridLayer(40, 10, {});
    setCells(L, 9, 0, 39, 1); // floor top -4.5, x 0..20
    scene.collision.addRoom("r", gridSegs(L));
    SlimeEnemy& slime = scene.createObject<SlimeEnemy>(SlimeEnemy::Config{}); // patrol 2 u
    slime.transform.position = {10.0f, -4.1f}; // placed after onCreate, as Spawn::slime does
    float lo = 10.0f, hi = 10.0f;
    for (int i = 0; i < 600; ++i) {
        scene.step(kDt);
        lo = std::min(lo, slime.transform.position.x); hi = std::max(hi, slime.transform.position.x);
    }
    CHECK(lo > 7.9f && hi < 12.1f && hi > 11.9f);
}

TEST(player_teleport_resets_controller_state) {
    Scene2D scene;
    LdtkLayer L = gridLayer(20, 10, {});
    setCells(L, 9, 0, 19, 1); // floor top y = -4.5
    scene.collision.addRoom("r", gridSegs(L));
    PlayerObject& p = platformerPlayer(scene, {3.0f, -3.6f});
    stepPlayer(scene, p, {}, 30);
    CHECK(p.isGrounded());
    p.teleport({8.0f, -4.5f + 0.8f + 0.25f}); // 0.25 above the floor: a kept `grounded` would snap it down
    CHECK(!p.isGrounded() && p.velocity == glm::vec2(0.0f));
    stepPlayer(scene, p, {});
    CHECK(p.transform.position.y - 0.8f > -4.5f + 0.2f);
}

TEST(hot_reload_depenetrates_or_respawns) {
    // One room 20 x 10 cells; version 0 = floor (top y -4.5); the reloaded versions bury the player's spot.
    int version = 0;
    auto layerFor = [](int v) {
        LdtkLayer L = gridLayer(20, 10, {});
        setCells(L, 9, 0, 19, 1);
        if (v == 1) setCells(L, 8, 0, 19, 1);                      // floor raised by one cell: top y -4.0
        if (v == 2) for (int y = 2; y < 10; ++y) setCells(L, y, 0, 19, 1); // solid 3.5 u over the player (room-border faces are not generated)
        return L;
    };
    LdtkProject p = roomsInRow({320});
    CollisionWorld cw;
    World w;
    w.onLoad = [&](int room) { cw.addRoom(p.levels[room].iid, gridSegs(layerFor(version))); };
    w.onUnload = [&](int room) { cw.removeRoom(p.levels[room].iid); };
    LevelLoader loader;
    const CollisionQuery q{&cw, {}};
    const glm::vec2 standing{3.0f, -4.5f + 0.8f};
    w.open(p, loader, nullptr);
    w.loadAround(0);
    Body b; b.position = standing;
    CHECK(depenetrate(b, q) && b.position == standing); // free: stays

    for (int v : {1, 2}) { // F5 / hot reload: same World, project parsed again, the room reloaded
        version = v;
        cw.removeRoom(p.levels[0].iid);
        w.open(p, loader, nullptr);
        w.loadAround(0);
        b.position = standing;
        const bool placed = depenetrate(b, q);
        if (v == 1) CHECK(placed && std::abs(b.position.y - 0.8f + 4.0f) < 0.02f && std::abs(b.position.x - 3.0f) < 1e-4f);
        else CHECK(!placed && b.position == standing); // nowhere within 2 u: the caller respawns at the checkpoint
    }
}

TEST(wall_across_bottom_border_has_no_shelf) {
    // wall column x = 3 from the top to the room's bottom border; the player falls beside it pressing into it
    LdtkLayer L = gridLayer(8, 8, {});
    for (int y = 0; y < 8; ++y) setCells(L, y, 3, 3, 1);
    CollisionWorld w; w.addRoom("r", gridSegs(L));
    CHECK(countIf(gridSegs(L), [](auto& s) { return std::abs(s.normal.y) > 0.5f && std::abs(s.a.y - s.b.y) < 1e-4f && (s.a.y == -4.0f || s.a.y == 0.0f); }) == 0);
    Sim s(&w, {1.2f, -0.5f});
    float lastVy = 0.0f;
    for (int i = 0; i < 90; ++i) {
        s.step(1.0f);
        CHECK(!s.grounded);
        if (i > 0) CHECK(s.v.y <= lastVy + 1e-4f); // never reset upwards by a shelf
        lastVy = s.v.y;
    }
}

TEST(controller_airborne_landing_on_uphill_keeps_vx_sign) {
    CollisionWorld w; w.addRoom("r", gridSegs(rampLayer()));
    Sim s(&w, {3.2f, -0.5f});
    bool landed = false;
    for (int i = 0; i < 120 && !landed; ++i) {
        s.step(1.0f);
        CHECK(s.v.x > 0.0f);
        landed = s.grounded;
    }
    CHECK(landed);
}

TEST(slime_lands_on_ramp_without_turning) {
    Scene2D scene;
    scene.collision.addRoom("r", gridSegs(rampLayer()));
    SlimeEnemy::Config sc;
    sc.patrolDistance = 20.0f;
    SlimeEnemy& slime = scene.createObject<SlimeEnemy>(sc);
    slime.transform.position = {3.3f, -1.0f};
    float x = slime.transform.position.x;
    for (int i = 0; i < 60; ++i) { scene.step(kDt); CHECK(slime.transform.position.x >= x - 1e-4f); x = slime.transform.position.x; }
    CHECK(x > 3.6f); // walked on to the right: no turn on landing
}

TEST(depenetrate_leaves_body_resting_on_ramp) {
    CollisionWorld w; w.addRoom("r", gridSegs(rampLayer()));
    Sim s(&w, {3.75f, -1.0f});
    s.settle(1.0f);
    CHECK(s.grounded);
    Body b = s.body;
    CHECK(depenetrate(b, s.q) && b.position == s.body.position); // resting contact is not overlap
}

TEST(terrain_style_falls_back_to_forest) {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "mush2d_terrain_style_test";
    fs::remove_all(root);
    for (const char* style : {"forest", "cave"}) {
        fs::create_directories(root / style);
        std::ofstream(root / style / "edge.png") << "x";
    }
    CHECK(terrainStyleDir(root, "cave") == root / "cave");
    CHECK(terrainStyleDir(root, "lava") == root / "forest"); // warned once, not thrown
    CHECK(terrainStyleDir(root, "lava") == root / "forest");
    fs::remove_all(root);
}

TEST(collides_at_position_sees_segments) {
    Scene2D scene;
    scene.collision.addRoom("r", polySegs({{0, 0}, {4, 0}, {4, 1}, {0, 1}}));
    GameObject& m = scene.createObject<GameObject>();
    m.collider = ColliderData{.size = {1.0f, 1.0f}};
    scene.flush();
    CHECK(Collision2D::collidesAtPosition(scene, m.id, {2.0f, 1.4f}));  // overlaps the top by 0.1
    CHECK(!Collision2D::collidesAtPosition(scene, m.id, {2.0f, 1.5f})); // resting on it
    CHECK(!Collision2D::collidesAtPosition(scene, m.id, {6.0f, 0.5f}));
}

int main() {
    for (auto& [name, fn] : registry()) {
        std::printf("[ RUN ] %s\n", name);
        fn();
    }
    std::printf(failures ? "FAILED (%d)\n" : "OK\n", failures);
    return failures ? 1 : 0;
}
