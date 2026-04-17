#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

template<typename TAnimationId>
class AnimationCatalog {
public:
    struct Entry {
        std::uint32_t atlasTextureIndex = 0;
        std::string animationName;
    };

public:
    void registerAnimation(TAnimationId id, Entry entry) {
        entries[id] = entry;
    }

    const Entry& get(TAnimationId id) const {
        auto it = entries.find(id);
        if (it == entries.end()) {
            throw std::runtime_error("AnimationId nao registrado no catalogo.");
        }

        return it->second;
    }

private:
    struct EnumClassHash {
        template<typename T>
        std::size_t operator()(T value) const {
            return static_cast<std::size_t>(value);
        }
    };

    std::unordered_map<TAnimationId, Entry, EnumClassHash> entries;
};