#pragma once

#include "game/assets/GameAssets.hpp"
#include "game/objects/PivotSprite.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

// Decor animal: loops one atlas animation, standing with its pivot on transform.position.
class CritterObject : public GameObject {
    MUSH2D_OBJECT(CritterObject, GameObject)
public:
    CritterObject(std::uint32_t atlasIndex, std::string animation, bool flip)
        : atlasIndex_(atlasIndex), animation_(std::move(animation)), flip_(flip) {}

    void onCreate(Scene2D& scene) override {
        name = "Critter";
        sprite = SpriteData{.atlasTextureIndex = atlasIndex_, .layer = 1};
        applyFrame(scene);
    }

    void onStep(Scene2D& scene, float dt) override {
        const AtlasAnimation* anim = animation(scene);
        if (!anim) return;
        timer_ += dt;
        float duration = anim->frames[frame_].durationMs > 0 ? anim->frames[frame_].durationMs / 1000.0f : 0.1f;
        while (timer_ >= duration) {
            timer_ -= duration;
            frame_ = (frame_ + 1) % anim->frames.size();
            duration = anim->frames[frame_].durationMs > 0 ? anim->frames[frame_].durationMs / 1000.0f : 0.1f;
        }
        applyFrame(scene);
    }

private:
    std::uint32_t atlasIndex_;
    std::string animation_;
    bool flip_;
    std::size_t frame_ = 0;
    float timer_ = 0.0f;

    const AtlasAnimation* animation(const Scene2D& scene) const {
        const GameAssets* assets = gameAssetsOf(scene);
        if (!assets) return nullptr;
        const TextureAtlas& atlas = assets->getAtlasByTextureIndex(atlasIndex_);
        if (!atlas.hasAnimation(animation_)) return nullptr;
        const AtlasAnimation& anim = atlas.getAnimation(animation_);
        return anim.frames.empty() ? nullptr : &anim;
    }

    void applyFrame(Scene2D& scene) {
        const AtlasAnimation* anim = animation(scene);
        if (!anim) return;
        const TextureAtlas& atlas = gameAssetsOf(scene)->getAtlasByTextureIndex(atlasIndex_);
        const AtlasFrame& f = atlas.getFrame(anim->frames[frame_].frameName);
        sprite->frameName = f.name;
        placeFrameAtPivot(*this, f, {0.0f, 0.0f}, flip_);
    }
};
