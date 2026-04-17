#pragma once

#include "AtlasTypes.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class TextureAtlas {
public:
    void setMeta(const AtlasMeta& newMeta);

    const AtlasMeta& getMeta() const;

    void addFrame(const AtlasFrame& frame);

    bool hasFrame(const std::string& frameName) const;

    const AtlasFrame& getFrame(const std::string& frameName) const;

    std::vector<std::string> getAllFrameNames() const;

    void addAnimation(const AtlasAnimation& animation);

    bool hasAnimation(const std::string& animationName) const;

    const AtlasAnimation& getAnimation(const std::string& animationName) const;

    std::vector<std::string> getAllAnimationNames() const;

private:
    AtlasMeta meta{};
    std::unordered_map<std::string, AtlasFrame> frames;
    std::unordered_map<std::string, AtlasAnimation> animations;
};