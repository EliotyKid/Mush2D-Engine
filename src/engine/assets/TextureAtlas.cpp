#include "TextureAtlas.hpp"

#include <stdexcept>

void TextureAtlas::setMeta(const AtlasMeta& newMeta) {
    meta = newMeta;
}

const AtlasMeta& TextureAtlas::getMeta() const {
    return meta;
}

void TextureAtlas::addFrame(const AtlasFrame& frame) {
    frames[frame.name] = frame;
}

bool TextureAtlas::hasFrame(const std::string& frameName) const {
    return frames.find(frameName) != frames.end();
}

const AtlasFrame& TextureAtlas::getFrame(const std::string& frameName) const {
    auto it = frames.find(frameName);
    if (it == frames.end()) {
        throw std::runtime_error("Frame nao encontrado no atlas: " + frameName);
    }

    return it->second;
}

std::vector<std::string> TextureAtlas::getAllFrameNames() const {
    std::vector<std::string> names;
    names.reserve(frames.size());

    for (const auto& [name, frame] : frames) {
        names.push_back(name);
    }

    return names;
}

void TextureAtlas::addAnimation(const AtlasAnimation& animation) {
    animations[animation.name] = animation;
}

bool TextureAtlas::hasAnimation(const std::string& animationName) const {
    return animations.find(animationName) != animations.end();
}

const AtlasAnimation& TextureAtlas::getAnimation(const std::string& animationName) const {
    auto it = animations.find(animationName);
    if (it == animations.end()) {
        throw std::runtime_error("Animacao nao encontrada no atlas: " + animationName);
    }

    return it->second;
}

std::vector<std::string> TextureAtlas::getAllAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(animations.size());

    for (const auto& [name, animation] : animations) {
        names.push_back(name);
    }

    return names;
}