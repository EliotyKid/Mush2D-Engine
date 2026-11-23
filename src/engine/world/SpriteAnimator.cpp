#include "engine/world/SpriteAnimator.hpp"

#include "engine/assets/AssetRegistry.hpp"
#include "engine/world/Scene2D.hpp"

#include <tuple>

void SpriteAnimator::onStep(GameObject& owner, Scene2D& scene, float dt) {
    const AssetRegistry* assets = scene.service<AssetRegistry>();
    auto it = animations.find(state);
    if (!assets || !owner.sprite || it == animations.end()) return;

    std::string atlasName = owner.sprite->atlasName, animName = it->second;
    if (auto ref = assets->resolve(animName)) std::tie(atlasName, animName) = *ref;
    const auto index = assets->atlasIndex(atlasName);
    const TextureAtlas* atlas = assets->atlas(atlasName);
    if (!atlas || !atlas->hasAnimation(animName) || atlas->getAnimation(animName).frames.empty()) return;
    const AtlasAnimation& anim = atlas->getAnimation(animName);

    if (playing_ != state) {
        playing_ = state;
        frame_ = 0;
        timer_ = 0.0f;
    } else {
        // shortcut: every animation loops (pingpong tags too), like the old CritterObject; add modes with D2's state machine.
        auto duration = [&] { return anim.frames[frame_].durationMs > 0 ? anim.frames[frame_].durationMs / 1000.0f : 0.1f; };
        frame_ %= anim.frames.size(); // the atlas may have been reloaded with fewer frames
        timer_ += dt;
        while (timer_ >= duration()) {
            timer_ -= duration();
            frame_ = (frame_ + 1) % anim.frames.size();
        }
    }
    const AtlasFrame& f = atlas->getFrame(anim.frames[frame_].frameName);
    owner.sprite->atlasTextureIndex = *index;
    owner.sprite->frameName = f.name;
    placeFrameAtPivot(owner, f, {0.0f, 0.0f}, owner.flipX);
}
