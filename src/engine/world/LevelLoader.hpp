#pragma once

#include "engine/core/IGame.hpp"
#include "engine/render/SpriteBatch.hpp"
#include "engine/world/Ldtk.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

// worldPos = entity box centre in world units (ldtkToWorld); parallax entities anchored at their room's centre.
using EntityFactory = std::function<void(const LdtkEntity&, glm::vec2 worldPos, EngineServices&)>;

constexpr float LdtkPixelsPerUnit = 32.0f;

class LevelLoader {
public:
    void registerEntity(std::string identifier, EntityFactory factory) { factories_[std::move(identifier)] = std::move(factory); }
    // One room, in world coordinates: tiles -> static SpriteInstances kept under the room's iid; IntGrid
    // "Collision" and "Terrain" entities -> collision segments (scene.collision, room level.iid).
    // Entities: spawnEntity (World).
    void loadRoom(const LdtkLevel& level, EngineServices& services, MaterialLibrary& materials);
    // Runs the entity's registered factory (unknown identifiers are warned about and ignored); every object
    // created meanwhile gets roomIid = level.iid.
    void spawnEntity(const LdtkLevel& level, const LdtkEntity& entity, EngineServices& services);
    // Drops the room's static sprites and collision segments and destroys its objects that are not persistent (flushed before return).
    void unloadRoom(const std::string& roomIid, Scene2D& scene);
    // Static sprites of the loaded rooms only.
    void appendStatic(SpriteBatch& batch) const;
    void clear() { rooms_.clear(); layers_.clear(); }

    // Tile layers kept as static sprites, editable by (region, LDtk identifier): shared by the rooms of one
    // region using that identifier, independent across regions (initial values come from the first room loaded
    // with it; append-only until clear). Parallax tiles are anchored at the world origin: with the camera at 0
    // they show at their authored place.
    struct LayerState { std::string region, identifier; glm::vec2 parallax; float depth; };
    const std::vector<LayerState>& layers() const { return layers_; }
    // Rewrites that layer's instances; no-op if unchanged.
    void setLayer(const std::string& region, const std::string& identifier, glm::vec2 parallax, float depth);

private:
    struct StaticSprite { SpriteInstance instance; int order; int layer; };
    static void appendTerrainSprites(const LdtkEntity& e, const LdtkLevel& level, int layerGridPx, int order,
                                     MaterialLibrary& materials, std::vector<StaticSprite>& sprites);
    std::map<std::string, EntityFactory> factories_;
    std::map<std::string, std::vector<StaticSprite>> rooms_; // by room iid
    std::vector<LayerState> layers_;
};
