#include "engine/core/Paths.hpp"
#include "engine/render/Materials.hpp"
#include "engine/render/PixelScale.hpp"
#include "engine/render/SceneRenderer.hpp"

#include <filesystem>
#include <fstream>

#include <cstdio>
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

int main() {
    for (auto& [name, fn] : registry()) {
        std::printf("[ RUN ] %s\n", name);
        fn();
    }
    std::printf(failures ? "FAILED (%d)\n" : "OK\n", failures);
    return failures ? 1 : 0;
}
