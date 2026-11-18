#include "GameAssets.hpp"

#include "engine/core/Paths.hpp"
#include "engine/assets/AtlasLoader.hpp"

#include <filesystem>
#include <stdexcept>

#include "GameAtlasId.hpp"

void GameAssets::registerDefaults() {
    atlases.clear();
    atlasNamesByTextureIndex.clear();

    static constexpr const char* names[GameAtlasId::Count] = {
        "characters", "enemies", "environment", "kaa_player", "kaa_animals", "kaa_props", "crystals"};
    for (std::uint32_t i = 0; i < GameAtlasId::Count; ++i) {
        std::filesystem::path image = assetPath(GameAtlasId::ImagePaths[i]);
        std::filesystem::path json = image;
        json.replace_extension(".json");
        registerAtlas(i, names[i], json.string(), image.string());
    }

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
    auto it = atlases.find(atlasName);
    if (it == atlases.end()) {
        throw std::runtime_error("Atlas nao encontrado em GameAssets: " + atlasName);
    }

    return it->second;
}

const TextureAtlas& GameAssets::getAtlasByTextureIndex(std::uint32_t textureIndex) const {
    if (textureIndex >= atlasNamesByTextureIndex.size()) {
        throw std::runtime_error("Texture index sem atlas associado.");
    }

    const std::string& atlasName = atlasNamesByTextureIndex[textureIndex];
    if (atlasName.empty()) {
        throw std::runtime_error("Texture index sem nome de atlas associado.");
    }

    return getAtlas(atlasName);
}

void GameAssets::registerAtlas(
    std::uint32_t textureIndex,
    const std::string& atlasName,
    const std::string& jsonPath,
    const std::string& imagePath
) {
    ensureAtlasIndexCapacity(textureIndex);

    atlases[atlasName] = AtlasLoader::loadFromAsepriteJsonHash(jsonPath, imagePath);
    atlasNamesByTextureIndex[textureIndex] = atlasName;
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

void GameAssets::ensureAtlasIndexCapacity(std::uint32_t textureIndex) {
    if (textureIndex >= atlasNamesByTextureIndex.size()) {
        atlasNamesByTextureIndex.resize(textureIndex + 1);
    }
}