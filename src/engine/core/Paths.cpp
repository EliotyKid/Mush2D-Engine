#include "Paths.hpp"

#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
std::filesystem::path executableDir() {
#ifdef _WIN32
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size()) {
            return std::filesystem::path(std::wstring(buf.data(), n)).parent_path();
        }
        buf.resize(buf.size() * 2);
    }
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}
}

std::filesystem::path assetPath(std::string_view relative) {
    return executableDir() / std::filesystem::path(relative);
}

std::filesystem::path sourceAssetPath(std::string_view relative) {
#ifdef MUSH2D_SOURCE_DIR
    return std::filesystem::path(MUSH2D_SOURCE_DIR) / std::filesystem::path(relative);
#else
    return assetPath(relative);
#endif
}
