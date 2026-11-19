#include "engine/world/WorldState.hpp"

#include "engine/world/PersistentComponent.hpp"

bool WorldState::isGone(const std::string& iid) const {
    const EntityState* s = find(iid);
    return s && s->gone;
}

void WorldState::markGone(const std::string& iid, glm::vec2 spawnPos, double time) {
    EntityState& s = entries_[iid];
    s.gone = true;
    s.goneTime = time;
    s.spawnPos = spawnPos;
}

void WorldState::clearGone(const std::string& iid) {
    if (auto it = entries_.find(iid); it != entries_.end()) it->second.gone = false;
}

double WorldState::get(const std::string& iid, const std::string& key, double def) const {
    const EntityState* s = find(iid);
    if (!s) return def;
    auto it = s->values.find(key);
    return it != s->values.end() ? it->second : def;
}

void WorldState::set(const std::string& iid, const std::string& key, double value) { entries_[iid].values[key] = value; }

const EntityState* WorldState::find(const std::string& iid) const {
    auto it = entries_.find(iid);
    return it != entries_.end() ? &it->second : nullptr;
}

std::map<std::string, EntityState> WorldState::snapshot(const std::vector<std::string>& iids) const {
    std::map<std::string, EntityState> out;
    for (const std::string& iid : iids)
        if (const EntityState* s = find(iid)) out[iid] = *s;
    return out;
}

void WorldState::restore(const std::vector<std::string>& iids, const std::map<std::string, EntityState>& snap) {
    for (const std::string& iid : iids) {
        auto it = snap.find(iid);
        if (it != snap.end()) entries_[iid] = it->second;
        else entries_.erase(iid);
    }
}

Persistence parsePersistence(const LdtkEntity& e) {
    if (auto it = e.fields.find("respawn"); it != e.fields.end() && it->is_boolean())
        return it->get<bool>() ? Persistence::Respawn : Persistence::Permanent;
    if (auto it = e.fields.find("persistence"); it != e.fields.end() && it->is_string()) {
        const std::string v = it->get<std::string>();
        if (v == "permanent") return Persistence::Permanent;
        if (v == "respawn") return Persistence::Respawn;
    }
    return Persistence::None;
}

float parseRespawnDistance(const LdtkEntity& e) {
    auto it = e.fields.find("respawnDistance");
    return it != e.fields.end() && it->is_number() ? it->get<float>() : 12.0f;
}
