#pragma once

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// State of one LDtk entity that outlives its room: dead/collected plus generic values ("open" -> 1).
struct EntityState {
    bool gone = false;
    double goneTime = 0;
    glm::vec2 spawnPos{0.0f};
    std::map<std::string, double> values;
};

// Persistent world state by LDtk iid. Plain data, so a save item can serialise it later.
class WorldState {
public:
    double clock = 0.0; // session seconds, advanced by the game; stamps goneTime

    bool isGone(const std::string& iid) const;
    void markGone(const std::string& iid, glm::vec2 spawnPos, double time);
    void clearGone(const std::string& iid);
    double get(const std::string& iid, const std::string& key, double def) const;
    void set(const std::string& iid, const std::string& key, double value);
    const EntityState* find(const std::string& iid) const; // null when the iid has no entry

    // Copies of the entries of these iids (missing ones are left out).
    std::map<std::string, EntityState> snapshot(const std::vector<std::string>& iids) const;
    // Puts these iids back as in snap: entries absent from snap are erased.
    void restore(const std::vector<std::string>& iids, const std::map<std::string, EntityState>& snap);

private:
    std::map<std::string, EntityState> entries_;
};
