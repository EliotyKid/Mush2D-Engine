#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

// Slot-map handle: a stale id (slot freed or reused) has a mismatched generation.
struct GameObjectId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0; // 0 = invalid

    explicit operator bool() const { return generation != 0; }
    bool operator==(const GameObjectId&) const = default;
};
inline constexpr GameObjectId InvalidGameObjectId{};

template<>
struct std::hash<GameObjectId> {
    std::size_t operator()(const GameObjectId& id) const noexcept {
        return std::hash<std::uint64_t>{}((std::uint64_t(id.generation) << 32) | id.index);
    }
};
