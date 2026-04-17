#pragma once

#include "../../engine/assets/TextureAtlas.hpp"
#include "../../engine/graphics/AnimationCatalog.hpp"
#include "../../engine/graphics/SpriteCatalog.hpp"
#include "GameAnimationId.hpp"
#include "GameSpriteId.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct GameAssets {
    SpriteCatalog<GameSpriteId> sprites;
    AnimationCatalog<GameAnimationId> animations;
    std::unordered_map<std::string, TextureAtlas> atlases;

    void registerDefaults();

    const TextureAtlas& getAtlas(const std::string& atlasName) const;
    const TextureAtlas& getAtlasByTextureIndex(std::uint32_t textureIndex) const;

private:
    std::vector<std::string> atlasNamesByTextureIndex;

private:
    void registerAtlas(
        std::uint32_t textureIndex,
        const std::string& atlasName,
        const std::string& jsonPath,
        const std::string& imagePath
    );

    void registerSprite(
        GameSpriteId spriteId,
        std::uint32_t atlasTextureIndex,
        const std::string& frameName
    );

    void registerAnimation(
        GameAnimationId animationId,
        std::uint32_t atlasTextureIndex,
        const std::string& animationName
    );

    void ensureAtlasIndexCapacity(std::uint32_t textureIndex);
};