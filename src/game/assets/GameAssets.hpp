#pragma once

#include "engine/assets/AssetRegistry.hpp"
#include "engine/assets/TextureAtlas.hpp"
#include "engine/graphics/AnimationCatalog.hpp"
#include "engine/graphics/SpriteCatalog.hpp"
#include "GameAnimationId.hpp"
#include "GameSpriteId.hpp"
#include "engine/world/Scene2D.hpp"

#include <cstdint>
#include <string>

struct GameAssets {
    SpriteCatalog<GameSpriteId> sprites;
    AnimationCatalog<GameAnimationId> animations;
    AssetRegistry registry; // every atlas under assets/, by name; index = atlas texture index

    void registerDefaults();

    const TextureAtlas& getAtlas(const std::string& atlasName) const;
    const TextureAtlas& getAtlasByTextureIndex(std::uint32_t textureIndex) const;

private:
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
};

// The game's GameAssets scene service (nullptr in scenes without it, e.g. tests).
inline GameAssets* gameAssetsOf(const Scene2D& scene) {
    return scene.service<GameAssets>();
}
