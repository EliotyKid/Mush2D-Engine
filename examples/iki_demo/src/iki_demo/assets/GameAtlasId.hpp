#pragma once

#include <cstdint>

// Shortcut ids for older code (Sandbox, Test, the player): the atlas index of each name. The AssetRegistry
// finds every atlas under assets/ by itself; these names only pin the first indices (GameAssets::registerDefaults).
namespace GameAtlasId {
    constexpr std::uint32_t Characters = 0;
    constexpr std::uint32_t Enemies = 1;
    constexpr std::uint32_t Environment = 2;
    constexpr std::uint32_t KaaPlayer = 3;
    constexpr std::uint32_t KaaAnimals = 4;
    constexpr std::uint32_t KaaProps = 5;
    constexpr std::uint32_t Crystals = 6;
    constexpr std::uint32_t Count = 7;

    inline constexpr const char* Names[Count] = {
        "characters", "enemies", "environment", "kaa_player", "kaa_animals", "kaa_props", "crystals"};
}
