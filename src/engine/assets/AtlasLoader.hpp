#pragma once

#include "TextureAtlas.hpp"

#include <string>

class AtlasLoader {
public:
    static TextureAtlas loadFromAsepriteJsonHash(
        const std::string& jsonPath,
        const std::string& imagePath
    );
};