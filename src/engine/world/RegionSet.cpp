#include "engine/world/RegionSet.hpp"

#include "engine/audio/Audio.hpp"
#include "engine/world/LevelLoader.hpp"
#include "engine/world/World.hpp"

#include <algorithm>
#include <set>
#include <utility>

void RegionSet::init(MaterialLibrary& materials, std::filesystem::path dir, Audio* audio) {
    materials_ = &materials;
    audio_ = audio;
    dir_ = std::move(dir);
}

void RegionSet::open(const World& world, int startRoom, bool keepLoaded) {
    world_ = &world;
    layersApplied_ = 0; // the caller cleared the LevelLoader
    if (!keepLoaded) regions_.clear(); // re-read the region files (F5)
    const std::vector<LdtkLevel>& levels = world.project().levels;
    for (const LdtkLevel& l : levels) get(l.region);
    current_ = levels.empty() ? "forest" : levels[startRoom].region;
    blendTime_ = BlendSeconds; // no blend into the first region
    if (audio_) audio_->playMusic(current().music); // same music (a reload) goes on
}

Region& RegionSet::get(const std::string& name) {
    auto it = regions_.find(name);
    if (it == regions_.end()) {
        it = regions_.emplace(name, loadRegionOrDefault(file(name), FrameSettings{})).first;
        resolveRegionMaterials(it->second, *materials_);
    }
    return it->second;
}

std::string RegionSet::roomRegion(const std::string& roomIid) const {
    const int room = world_->roomOf(roomIid);
    return room >= 0 ? world_->room(room).region : "forest";
}

Region& RegionSet::ofRoom(const std::string& roomIid) { return get(roomRegion(roomIid)); }

void RegionSet::applyNewLayerOverrides(LevelLoader& loader) {
    const std::vector<LevelLoader::LayerState>& layers = loader.layers();
    for (; layersApplied_ < layers.size(); ++layersApplied_) {
        const LevelLoader::LayerState& l = layers[layersApplied_];
        const auto& overrides = get(l.region).look.layers;
        if (auto it = overrides.find(l.identifier); it != overrides.end())
            loader.setLayer(l.region, l.identifier, it->second.parallax, it->second.depth);
    }
}

FrameSettings RegionSet::update(const FrameSettings& shown, float dt) {
    if (world_->currentRoom() >= 0) {
        const std::string& name = world_->room(world_->currentRoom()).region;
        if (name != current_) { // background swaps now, the look blends
            blendFrom_ = shown;
            current_ = name;
            blendTime_ = world_->dying() ? BlendSeconds : 0.0f; // death teleport: the fade-in shows the new look
            if (audio_) audio_->playMusic(current().music);    // the same music does nothing
        }
    }
    blendTime_ = std::min(blendTime_ + dt, BlendSeconds);
    return lerpSettings(blendFrom_, current().look.settings, blendTime_ / BlendSeconds);
}

void RegionSet::submit(const Camera2D& camera, VkExtent2D viewport, SpriteBatch& batch) {
    appendRegionLayers(current(), camera, viewport, batch);
}

void RegionSet::save(const LevelLoader& loader, const std::vector<EditableLight>& lights) {
    std::set<std::string> touched{current_};
    Region& cur = current();
    for (const LevelLoader::LayerState& l : loader.layers())
        if (l.region == current_) cur.look.layers[l.identifier] = {l.parallax, l.depth};
    for (const EditableLight& l : lights) {
        const std::string name = roomRegion(l.roomIid);
        get(name).look.lights[l.iid] = l.value;
        touched.insert(name);
    }
    for (const std::string& name : touched) saveRegion(file(name), get(name));
}
