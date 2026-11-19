#pragma once

#include "engine/core/IGame.hpp"
#include "engine/render/SpriteBatch.hpp"
#include "engine/world/Ldtk.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

// worldPos = entity box centre in world units (ldtkToWorld), parallax anchor applied.
using EntityFactory = std::function<void(const LdtkEntity&, glm::vec2 worldPos, EngineServices&)>;

constexpr float LdtkPixelsPerUnit = 32.0f;

class LevelLoader {
public:
    void registerEntity(std::string identifier, EntityFactory factory) { factories_[std::move(identifier)] = std::move(factory); }
    // One room, in world coordinates: tiles -> static SpriteInstances kept under the room's iid; IntGrid
    // "Collision" (value 1) -> static collider objects; entities -> registered factories (unknown identifiers
    // are warned about and ignored). Every object created meanwhile gets roomIid = level.iid.
    void loadRoom(const LdtkLevel& level, EngineServices& services, MaterialLibrary& materials);
    // Drops the room's static sprites and destroys (deferred) its objects that are not persistent.
    void unloadRoom(const std::string& roomIid, Scene2D& scene);
    // Static sprites of the loaded rooms only.
    void appendStatic(SpriteBatch& batch) const;
    void clear() { rooms_.clear(); layers_.clear(); }

    // World point where layers with parallax != 1 (tiles and entities) show at their authored place.
    // The world origin by default; the same for every room, so rooms never use their own centre.
    glm::vec2 parallaxAnchor{0.0f};

    // Tile layers kept as static sprites, editable by LDtk identifier and shared by every room using that
    // identifier (initial values come from the first room loaded with it).
    struct LayerState { std::string identifier; glm::vec2 parallax; float depth; };
    const std::vector<LayerState>& layers() const { return layers_; }
    // Rewrites that layer's instances (parallax anchor recomputed from the authored position); no-op if unchanged.
    void setLayer(const std::string& identifier, glm::vec2 parallax, float depth);

private:
    struct StaticSprite { SpriteInstance instance; int order; int layer; glm::vec2 authoredPos; };
    std::map<std::string, EntityFactory> factories_;
    std::map<std::string, std::vector<StaticSprite>> rooms_; // by room iid
    std::vector<LayerState> layers_;
};
