#pragma once

#include "engine/world/Region.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

class Audio;
class LevelLoader;
class World;

// The look regions of an LDtk world (dir/<name>.region.json): every region loaded once, the current room's
// region and the look blend into it, and the region overrides of the tile layers.
class RegionSet {
public:
    static constexpr float BlendSeconds = 1.5f;

    // dir holds the region files; region images are resolved once, through materials, when a region loads.
    // audio (optional) is asked for the current region's music whenever the region changes.
    void init(MaterialLibrary& materials, std::filesystem::path dir, Audio* audio = nullptr);
    // After World::open, before its first room loads. keepLoaded (hot reload of the .ldtk alone) keeps the
    // loaded regions with their unsaved edits; otherwise the files are read again. Preloads every region the
    // project references (no file read when a room first streams in). startRoom's region becomes current
    // ("forest" without rooms), with no blend into it.
    void open(const World& world, int startRoom, bool keepLoaded);

    Region& get(const std::string& name);         // loaded on first use
    Region& ofRoom(const std::string& roomIid);   // that room's region; "forest" if it is no room
    Region& current() { return get(current_); }
    const std::string& currentName() const { return current_; }

    // A tile layer's overrides belong to the region of the rooms that own it: applied once, when the layer
    // first appears in loader (its room loaded), from that region's file.
    void applyNewLayerOverrides(LevelLoader& loader);
    // Follows the world's current room: the background swaps at once, the look blends from `shown` over
    // BlendSeconds (a death teleport snaps: the fade-in shows the new look). Returns the look to show.
    FrameSettings update(const FrameSettings& shown, float dt);
    // The current region's background layers.
    void submit(const Camera2D& camera, VkExtent2D viewport, SpriteBatch& batch);
    // Tile layers of the current region's rooms go to it (other regions' layers never cross over); each light
    // to its own room's region. Overrides of layers and lights not loaded now are kept.
    void save(const LevelLoader& loader, const std::vector<EditableLight>& lights);

private:
    std::string roomRegion(const std::string& roomIid) const;
    std::filesystem::path file(const std::string& name) const { return dir_ / (name + ".region.json"); }

    MaterialLibrary* materials_ = nullptr;
    Audio* audio_ = nullptr;
    std::filesystem::path dir_;
    const World* world_ = nullptr;
    std::map<std::string, Region> regions_;
    std::string current_;            // region of the current room; the panel edits and saves it
    FrameSettings blendFrom_;        // look shown when the region changed
    float blendTime_ = 0.0f;
    std::size_t layersApplied_ = 0;  // loader layers that got their region's overrides
};
