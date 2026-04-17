#pragma once

#include <cstdint>
#include <stdexcept>
#include <unordered_map>

template<typename TSpriteId>
class SpriteCatalog {
public:
    struct Entry {
        std::uint32_t textureIndex = 0;
    };

public:
    void registerSprite(TSpriteId id, Entry entry) {
        entries[id] = entry;
    }

    const Entry& get(TSpriteId id) const {
        auto it = entries.find(id);
        if (it == entries.end()) {
            throw std::runtime_error("SpriteId nao registrado no catalogo.");
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

    std::unordered_map<TSpriteId, Entry, EnumClassHash> entries;
};