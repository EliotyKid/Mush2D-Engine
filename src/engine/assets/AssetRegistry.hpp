#pragma once

#include "engine/assets/TextureAtlas.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Every Aseprite atlas (JSON hash) found under a folder, by file name: assets/art/characters/hero_animals.json
// -> "hero_animals". Animations are its frameTags, frames its frame names. Registered as a Scene2D service, so
// prefabs and SpriteAnimator resolve atlas names and "atlas/frame" / "atlas/animation" refs.
class AssetRegistry {
public:
    struct Atlas {
        std::string name;
        std::filesystem::path json, image; // image = meta.image, next to the json
        TextureAtlas data;
    };

    // Replaces the registry with every *.json under root (recursively) that has "frames" (an object) and a
    // "meta.image" string; other JSON files are skipped. Atlas index = position in atlases(): the names in
    // `first` take 0, 1, 2... in that order (shortcut ids such as GameAtlasId), the others follow sorted by
    // name. Throws std::runtime_error when a `first` name has no atlas (every later index would shift).
    // A second atlas with the same name is warned about and skipped.
    void scan(const std::filesystem::path& root, const std::vector<std::string>& first = {});

    const TextureAtlas* atlas(const std::string& name) const;
    std::optional<std::uint32_t> atlasIndex(const std::string& name) const;
    const std::vector<Atlas>& atlases() const { return atlases_; } // by index

    // "atlas/frame" or "atlas/animation" -> {atlas, frame or animation}, when both exist.
    std::optional<std::pair<std::string, std::string>> resolve(const std::string& ref) const;

private:
    std::vector<Atlas> atlases_;
};
