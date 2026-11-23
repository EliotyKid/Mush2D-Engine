#include "engine/assets/AssetRegistry.hpp"

#include "engine/assets/AtlasLoader.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>

#include <nlohmann/json.hpp>

void AssetRegistry::scan(const std::filesystem::path& root, const std::vector<std::string>& first) {
    namespace fs = std::filesystem;
    std::map<std::string, Atlas> found; // sorted by name
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::path& path = it->path();
        if (path.extension() != ".json" || !it->is_regular_file(ec)) continue;
        std::ifstream file(path);
        const nlohmann::json j = nlohmann::json::parse(file, nullptr, false); // not JSON: discarded, skipped
        if (!j.is_object() || !j.contains("frames") || !j["frames"].is_object() || !j.contains("meta") ||
            !j["meta"].is_object() || !j["meta"].contains("image") || !j["meta"]["image"].is_string())
            continue;
        const std::string name = path.stem().string();
        if (found.count(name)) {
            std::fprintf(stderr, "AssetRegistry: atlas '%s' found twice, %s skipped\n", name.c_str(), path.string().c_str());
            continue;
        }
        const fs::path image = path.parent_path() / j["meta"]["image"].get<std::string>();
        try {
            found[name] = Atlas{name, path, image, AtlasLoader::loadFromAsepriteJsonHash(path.string(), image.string())};
        } catch (const std::exception& e) {
            std::fprintf(stderr, "AssetRegistry: %s skipped: %s\n", path.string().c_str(), e.what());
        }
    }

    atlases_.clear();
    for (const std::string& name : first) {
        auto it = found.find(name);
        if (it == found.end()) throw std::runtime_error("AssetRegistry: atlas '" + name + "' not found under " + root.string());
        atlases_.push_back(std::move(it->second));
        found.erase(it);
    }
    for (auto& [name, atlas] : found) atlases_.push_back(std::move(atlas));
}

const TextureAtlas* AssetRegistry::atlas(const std::string& name) const {
    const std::optional<std::uint32_t> i = atlasIndex(name);
    return i ? &atlases_[*i].data : nullptr;
}

std::optional<std::uint32_t> AssetRegistry::atlasIndex(const std::string& name) const {
    for (std::size_t i = 0; i < atlases_.size(); ++i)
        if (atlases_[i].name == name) return std::uint32_t(i);
    return std::nullopt;
}

std::optional<std::pair<std::string, std::string>> AssetRegistry::resolve(const std::string& ref) const {
    const auto slash = ref.find('/');
    if (slash == std::string::npos) return std::nullopt;
    std::string atlasName = ref.substr(0, slash), item = ref.substr(slash + 1);
    const TextureAtlas* a = atlas(atlasName);
    if (!a || (!a->hasFrame(item) && !a->hasAnimation(item))) return std::nullopt;
    return std::make_pair(std::move(atlasName), std::move(item));
}
