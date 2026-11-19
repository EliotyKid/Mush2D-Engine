#include "engine/world/World.hpp"

#include "engine/world/LevelLoader.hpp"

#include <algorithm>

void World::open(const LdtkProject& project, LevelLoader& loader, EngineServices* services) {
    project_ = project;
    neighbours_ = computeNeighbours(project_.levels);
    loaded_.clear();
    current_ = -1;
    loader_ = &loader;
    services_ = services;
}

glm::vec4 World::rect(int room) const {
    const LdtkLevel& l = project_.levels[room];
    const glm::vec2 a = ldtkToWorld(l.worldPx, {0.0f, 0.0f});               // top-left
    const glm::vec2 b = ldtkToWorld(l.worldPx, glm::vec2(l.pxSize));         // bottom-right
    return {a.x, b.y, b.x, a.y};
}

int World::roomAt(glm::vec2 pos) const {
    for (int i = 0; i < int(project_.levels.size()); ++i) {
        const glm::vec4 r = rect(i);
        if (pos.x >= r.x && pos.x <= r.z && pos.y >= r.y && pos.y <= r.w) return i;
    }
    return -1;
}

glm::vec4 World::worldBounds() const {
    if (project_.levels.empty()) return glm::vec4(0.0f);
    glm::vec4 b = rect(0);
    for (int i = 1; i < int(project_.levels.size()); ++i) {
        const glm::vec4 r = rect(i);
        b = {std::min(b.x, r.x), std::min(b.y, r.y), std::max(b.z, r.z), std::max(b.w, r.w)};
    }
    return b;
}

void World::update(glm::vec2 focus) {
    // Spec decision 4: the current room holds while it contains the focus; in a gap the last one stays.
    bool keep = false;
    if (current_ >= 0) {
        const glm::vec4 r = rect(current_);
        keep = focus.x >= r.x && focus.x <= r.z && focus.y >= r.y && focus.y <= r.w;
    }
    if (!keep) {
        const int room = roomAt(focus);
        if (room >= 0 && room != current_) loadAround(room);
    }
    if (current_ < 0) return;

    const std::vector<int>& adjacent = neighbours_[current_];
    for (int room : std::vector<int>(loaded_)) { // copy: unload edits loaded_
        if (room == current_ || std::find(adjacent.begin(), adjacent.end(), room) != adjacent.end()) continue;
        const glm::vec4 r = rect(room);
        const glm::vec2 d = glm::max(glm::vec2(r.x, r.y) - focus, focus - glm::vec2(r.z, r.w));
        if (glm::length(glm::max(d, glm::vec2(0.0f))) > UnloadDistance) unload(room);
    }
}

void World::loadAround(int room) {
    current_ = room;
    load(room);
    for (int n : neighbours_[room]) load(n);
}

void World::load(int room) {
    if (std::find(loaded_.begin(), loaded_.end(), room) != loaded_.end()) return;
    // shortcut: a whole room is built in one frame; slice by entities per frame if a room passes ~2 ms.
    if (onLoad) onLoad(room);
    else loader_->loadRoom(project_.levels[room], *services_, services_->materials);
    loaded_.insert(std::upper_bound(loaded_.begin(), loaded_.end(), room), room); // only once it loaded
}

void World::unload(int room) {
    loaded_.erase(std::find(loaded_.begin(), loaded_.end(), room));
    if (onUnload) onUnload(room);
    else loader_->unloadRoom(project_.levels[room].iid, services_->scene);
}
