#include "engine/core/Paths.hpp"

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

int main() {
    for (auto& [name, fn] : registry()) {
        std::printf("[ RUN ] %s\n", name);
        fn();
    }
    std::printf(failures ? "FAILED (%d)\n" : "OK\n", failures);
    return failures ? 1 : 0;
}
