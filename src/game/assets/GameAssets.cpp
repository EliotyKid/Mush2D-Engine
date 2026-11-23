#include "GameAssets.hpp"

#include "engine/core/Paths.hpp"

#include <iterator>
#include <stdexcept>
#include <vector>

#include "GameAtlasId.hpp"

void GameAssets::registerDefaults() {
    // Every atlas under assets/; the GameAtlasId names take the first indices, in order.
    registry.scan(assetPath("assets"), std::vector<std::string>(std::begin(GameAtlasId::Names), std::end(GameAtlasId::Names)));

    registerSprite(GameSpriteId::PlayerIdle, GameAtlasId::Characters, "Sprite-0001 0.");
    registerSprite(GameSpriteId::PlayerWalk, GameAtlasId::Characters, "Sprite-0001 5.");
    registerSprite(GameSpriteId::PlayerDash, GameAtlasId::Characters, "Sprite-0001 10.");
    registerSprite(GameSpriteId::PlayerHit, GameAtlasId::Characters, "Sprite-0001 15.");

    registerSprite(GameSpriteId::SlimeIdle, GameAtlasId::Enemies, "slime_idle_0");
    registerSprite(GameSpriteId::SlimeWalk, GameAtlasId::Enemies, "slime_walk_0");

    registerSprite(GameSpriteId::Wall, GameAtlasId::Environment, "wall_0");
    registerSprite(GameSpriteId::Checkpoint, GameAtlasId::Environment, "checkpoint_0");
    registerSprite(GameSpriteId::Teleport, GameAtlasId::Environment, "teleport_0");

    registerAnimation(GameAnimationId::PlayerIdle, GameAtlasId::Characters, "player_idle");
    registerAnimation(GameAnimationId::PlayerWalk, GameAtlasId::Characters, "player_walk");
    registerAnimation(GameAnimationId::PlayerDash, GameAtlasId::Characters, "player_dash");
    registerAnimation(GameAnimationId::PlayerHit, GameAtlasId::Characters, "player_hit");

    registerAnimation(GameAnimationId::KaaIdle, GameAtlasId::KaaPlayer, "idle");
    registerAnimation(GameAnimationId::KaaRun, GameAtlasId::KaaPlayer, "run");
    registerAnimation(GameAnimationId::KaaJump, GameAtlasId::KaaPlayer, "jump");
    registerAnimation(GameAnimationId::KaaFall, GameAtlasId::KaaPlayer, "fall");

    registerAnimation(GameAnimationId::SlimeIdle, GameAtlasId::Enemies, "slime_idle");
    registerAnimation(GameAnimationId::SlimeWalk, GameAtlasId::Enemies, "slime_walk");
}

const TextureAtlas& GameAssets::getAtlas(const std::string& atlasName) const {
    const TextureAtlas* atlas = registry.atlas(atlasName);
    if (!atlas) {
        throw std::runtime_error("Atlas nao encontrado em GameAssets: " + atlasName);
    }
    return *atlas;
}

const TextureAtlas& GameAssets::getAtlasByTextureIndex(std::uint32_t textureIndex) const {
    if (textureIndex >= registry.atlases().size()) {
        throw std::runtime_error("Texture index sem atlas associado.");
    }
    return registry.atlases()[textureIndex].data;
}

void GameAssets::registerSprite(
    GameSpriteId spriteId,
    std::uint32_t atlasTextureIndex,
    const std::string& frameName
) {
    sprites.registerSprite(spriteId, {
        .atlasTextureIndex = atlasTextureIndex,
        .frameName = frameName
    });
}

void GameAssets::registerAnimation(
    GameAnimationId animationId,
    std::uint32_t atlasTextureIndex,
    const std::string& animationName
) {
    animations.registerAnimation(animationId, {
        .atlasTextureIndex = atlasTextureIndex,
        .animationName = animationName
    });
}
