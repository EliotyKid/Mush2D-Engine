#include "GameAssets.hpp"

void GameAssets::registerDefaults() {
    sprites.registerSprite(GameSpriteId::PlayerIdle,  { .textureIndex = 1 });
    sprites.registerSprite(GameSpriteId::PlayerWalk,  { .textureIndex = 1 });
    sprites.registerSprite(GameSpriteId::PlayerDash,  { .textureIndex = 1 });
    sprites.registerSprite(GameSpriteId::PlayerHit,   { .textureIndex = 1 });

    sprites.registerSprite(GameSpriteId::SlimeIdle,   { .textureIndex = 0 });
    sprites.registerSprite(GameSpriteId::SlimeWalk,   { .textureIndex = 0 });

    sprites.registerSprite(GameSpriteId::Wall,        { .textureIndex = 0 });
    sprites.registerSprite(GameSpriteId::Checkpoint,  { .textureIndex = 1 });
    sprites.registerSprite(GameSpriteId::Teleport,    { .textureIndex = 1 });
}