#include "engine/core/Paths.hpp"
#include "engine/render/Materials.hpp"

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

int main() {
    for (auto& [name, fn] : registry()) {
        std::printf("[ RUN ] %s\n", name);
        fn();
    }
    std::printf(failures ? "FAILED (%d)\n" : "OK\n", failures);
    return failures ? 1 : 0;
}
