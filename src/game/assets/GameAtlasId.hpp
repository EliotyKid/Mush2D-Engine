#pragma once

#include <cstdint>

namespace GameAtlasId {
    constexpr std::uint32_t Characters = 0;
    constexpr std::uint32_t Enemies = 1;
    constexpr std::uint32_t Environment = 2;
    constexpr std::uint32_t KaaPlayer = 3;
    constexpr std::uint32_t KaaAnimals = 4;
    constexpr std::uint32_t KaaProps = 5;
    constexpr std::uint32_t Crystals = 6;
    constexpr std::uint32_t Count = 7;

    // Atlas image paths by index (json next to it); materials are loaded from these.
    inline constexpr const char* ImagePaths[Count] = {
        "assets/atlases/characters.png", "assets/atlases/enemies.png", "assets/atlases/environment.png",
        "assets/kaa/characters/kaa_player.png", "assets/kaa/characters/kaa_animals.png",
        "assets/kaa/props/kaa_props.png", "assets/props/crystals.png"};
}