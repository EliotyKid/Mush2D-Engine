#include "engine/world/World.hpp"

#include "engine/world/LevelLoader.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

void World::open(const LdtkProject& project, LevelLoader& loader, EngineServices* services) {
    project_ = project;
    neighbours_ = computeNeighbours(project_.levels);
    loaded_.clear();
    current_ = -1;
    loader_ = &loader;
    services_ = services;
    nextRespawnCheck_ = 0.0;
    tracked_.clear();
    roomIids_.assign(project_.levels.size(), {});
    snapshot_.clear();
    deathTime_ = -1.0f;
    for (int r = 0; r < int(project_.levels.size()); ++r)
        for (const LdtkLayer& L : project_.levels[r].layers)
            for (const LdtkEntity& e : L.entities) {
                if (!e.iid.empty()) roomIids_[r].push_back(e.iid);
                // A wired target (lever -> door) joins this room's death snapshot, even from another room.
                for (const auto& f : e.fields)
                    if (f.is_object() && f.contains("entityIid") && f["entityIid"].is_string()) {
                        std::vector<std::string>& ids = roomIids_[r];
                        const std::string t = f["entityIid"].get<std::string>();
                        if (std::find(ids.begin(), ids.end(), t) == ids.end()) ids.push_back(t);
                    }
                const Persistence p = parsePersistence(e);
                if (p != Persistence::None && !e.iid.empty()) tracked_[e.iid] = {r, &e, p, parseRespawnDistance(e)};
            }
    for (const auto& [iid, p] : prefabTracked_) trackPrefab(iid, p, -1); // a gone one is not spawned again to re-register
}

void World::track(const std::string& iid, int room, Persistence policy, float respawnDistance) {
    if (iid.empty() || policy == Persistence::None) return;
    prefabTracked_[iid] = {policy, respawnDistance};
    trackPrefab(iid, prefabTracked_[iid], room);
}

void World::trackPrefab(const std::string& iid, const PrefabPolicy& p, int roomHint) {
    auto it = tracked_.find(iid);
    if (it != tracked_.end() && parsePersistence(*it->second.entity) != Persistence::None) return; // LDtk wins
    for (int r = 0; r < int(project_.levels.size()); ++r) {
        if (roomHint >= 0 && r != roomHint) continue;
        for (const LdtkLayer& L : project_.levels[r].layers)
            for (const LdtkEntity& e : L.entities)
                if (e.iid == iid) {
                    tracked_[iid] = {r, &e, p.policy, p.respawnDistance};
                    return;
                }
    }
}

void World::checkRespawns(glm::vec2 playerPos, glm::vec4 view, double time) {
    if (dying()) return;
    if (time < nextRespawnCheck_) return;
    nextRespawnCheck_ = time + RespawnCheckInterval;
    for (const auto& [iid, t] : tracked_) {
        const EntityState* s = state.find(iid);
        if (t.policy != Persistence::Respawn || !s || !s->gone) continue;
        if (!std::binary_search(loaded_.begin(), loaded_.end(), t.room)) continue;
        const glm::vec2 p = s->spawnPos;
        // 1-unit margin: a sprite centred just outside the view would still pop in half-visible.
        const bool onScreen = p.x >= view.x - 1.0f && p.x <= view.z + 1.0f && p.y >= view.y - 1.0f && p.y <= view.w + 1.0f;
        if (onScreen || glm::distance(playerPos, p) <= t.respawnDistance) continue;
        state.clearGone(iid);
        spawn(t.room, *t.entity);
    }
}

void World::onPlayerDeath(glm::vec2 respawnPoint, std::function<void(glm::vec2)> teleportPlayer) {
    const int died = current_;
    bool reloaded = false; // unloaded here: onRoomUnloaded only if loadAround does not bring it back
    if (current_ >= 0) {
        const std::vector<std::string>& iids = roomIids_[current_];
        std::map<std::string, double> activated; // checkpoints keep their activation (never rolled back)
        for (const std::string& iid : iids)
            if (const double a = state.get(iid, "activated", -1.0); a >= 0.0) activated[iid] = a;
        state.restore(iids, snapshot_);
        for (const auto& [iid, a] : activated) state.set(iid, "activated", a);
        // Reloaded by loadAround below when it is near the respawn point; a far one stays unloaded.
        if (std::find(loaded_.begin(), loaded_.end(), current_) != loaded_.end()) {
            unload(current_, false);
            reloaded = true;
        }
    }
    const int target = roomAt(respawnPoint);
    if (target >= 0) loadAround(target);
    else if (current_ >= 0) loadAround(current_);
    if (reloaded && !std::binary_search(loaded_.begin(), loaded_.end(), died) && onRoomUnloaded)
        onRoomUnloaded(project_.levels[died].iid);
    teleportPlayer(respawnPoint);
}

void World::killPlayer(glm::vec2 respawnPoint, std::function<void(glm::vec2)> teleportPlayer) {
    if (deathTime_ >= 0.0f) return;
    deathTime_ = 0.0f;
    deathPoint_ = respawnPoint;
    deathTeleport_ = std::move(teleportPlayer);
}

float World::stepFade(float dt) {
    if (deathTime_ < 0.0f) return 1.0f;
    const bool dies = deathTime_ < FadeSeconds && deathTime_ + dt >= FadeSeconds;
    deathTime_ += dt;
    if (dies) onPlayerDeath(deathPoint_, deathTeleport_);
    if (deathTime_ >= 2.0f * FadeSeconds) {
        deathTime_ = -1.0f;
        return 1.0f;
    }
    return std::abs(deathTime_ - FadeSeconds) / FadeSeconds;
}

void World::spawn(int room, const LdtkEntity& e) {
    if (onSpawn) onSpawn(room, e);
    else if (services_) loader_->spawnEntity(project_.levels[room], e, *services_);
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

int World::roomOf(const std::string& roomIid) const {
    for (int i = 0; i < int(project_.levels.size()); ++i)
        if (project_.levels[i].iid == roomIid) return i;
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
    if (dying()) return; // the room and its snapshot are frozen until the fade ends
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
    snapshot_ = state.snapshot(roomIids_[room]); // entering the room: death puts it back this way
    load(room);
    for (int n : neighbours_[room]) load(n);
}

void World::load(int room) {
    if (std::find(loaded_.begin(), loaded_.end(), room) != loaded_.end()) return;
    static const bool perfLog = std::getenv("MUSH2D_PERF_LOG") != nullptr; // CPU cost of each room load
    const auto start = std::chrono::steady_clock::now();
    // shortcut: a whole room is built in one frame; slice by entities per frame if a room passes ~2 ms.
    if (onLoad) onLoad(room);
    else loader_->loadRoom(project_.levels[room], *services_, services_->materials);
    // Gone entities (killed, collected) are not created; Respawn ones come back through checkRespawns.
    for (const LdtkLayer& L : project_.levels[room].layers)
        for (const LdtkEntity& e : L.entities)
            if (!state.isGone(e.iid)) spawn(room, e);
    loaded_.insert(std::upper_bound(loaded_.begin(), loaded_.end(), room), room); // only once it loaded
    if (perfLog)
        std::cout << "room " << project_.levels[room].identifier << " load_ms="
                  << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() << std::endl;
}

void World::unload(int room, bool notify) {
    loaded_.erase(std::find(loaded_.begin(), loaded_.end(), room));
    if (onUnload) onUnload(room);
    else loader_->unloadRoom(project_.levels[room].iid, services_->scene);
    if (notify && onRoomUnloaded) onRoomUnloaded(project_.levels[room].iid);
}
