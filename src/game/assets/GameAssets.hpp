#pragma once

#include "../../engine/graphics/SpriteCatalog.hpp"
#include "GameSpriteId.hpp"

struct GameAssets {
    SpriteCatalog<GameSpriteId> sprites;

    void registerDefaults();
};