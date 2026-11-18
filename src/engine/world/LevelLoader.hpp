#pragma once

#include "engine/core/IGame.hpp"
#include "engine/render/SpriteBatch.hpp"
#include "engine/world/Ldtk.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

// worldPos = entity box centre in world units (LDtk Y flipped, 32 px per unit), parallax anchor applied.
using EntityFactory = std::function<void(const LdtkEntity&, glm::vec2 worldPos, EngineServices&)>;

constexpr float LdtkPixelsPerUnit = 32.0f;

class LevelLoader {
public:
    void registerEntity(std::string identifier, EntityFactory factory) { factories_[std::move(identifier)] = std::move(factory); }
    // Tiles -> static SpriteInstances kept here; IntGrid "Collision" (value 1) -> static collider objects;
    // entities -> registered factories (unknown identifiers are warned about and ignored).
    void load(const LdtkLevel& level, EngineServices& services, MaterialLibrary& materials);
    void appendStatic(SpriteBatch& batch) const;
    void clear() { sprites_.clear(); layers_.clear(); }

    // Tile layers kept as static sprites, editable by LDtk identifier (initial values come from the level).
    struct LayerState { std::string identifier; glm::vec2 parallax; float depth; };
    const std::vector<LayerState>& layers() const { return layers_; }
    // Rewrites that layer's instances (parallax anchor recomputed from the authored position); no-op if unchanged.
    void setLayer(const std::string& identifier, glm::vec2 parallax, float depth);

private:
    struct StaticSprite { SpriteInstance instance; int order; int layer; glm::vec2 authoredPos; };
    std::map<std::string, EntityFactory> factories_;
    std::vector<StaticSprite> sprites_;
    std::vector<LayerState> layers_;
    glm::vec2 levelCentre_{0.0f};
};
