#pragma once

#include "engine/world/Ldtk.hpp"
#include "engine/world/PersistentComponent.hpp"
#include "engine/world/WorldState.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class LevelLoader;
struct EngineServices;

// Every room of an LDtk project lives in one Scene2D; World keeps the room holding the focus plus its
// neighbours loaded. A room leaves only when it is no longer a neighbour of the current room AND the focus
// is more than UnloadDistance from its rect, so standing on a border never thrashes.
class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    static constexpr float UnloadDistance = 4.0f; // world units
    static constexpr double RespawnCheckInterval = 0.5; // seconds

    // Tests (no GPU): when set, these replace LevelLoader::loadRoom / unloadRoom / spawnEntity. Argument = room index.
    std::function<void(int)> onLoad, onUnload;
    std::function<void(int room, const LdtkEntity&)> onSpawn;
    // A room left the loaded set (streaming, or death when the respawn is far from it); a room the death
    // reloads within the same call does not fire. Not a test hook: runs with or without onUnload.
    std::function<void(const std::string& roomIid)> onRoomUnloaded;

    // Persistent state by iid. Survives open(): reloading the project keeps doors open and enemies dead.
    WorldState state;

    // Copies the project and forgets loaded rooms (nothing is unloaded: the caller clears the scene first).
    // services may be null only when onLoad/onUnload are set.
    void open(const LdtkProject& project, LevelLoader& loader, EngineServices* services);
    // Picks the current room (spec decision 4), loads it and its neighbours, unloads far rooms.
    void update(glm::vec2 focus);
    // Synchronously makes room current and loads it and its neighbours.
    void loadAround(int room);
    // At most every RespawnCheckInterval: a gone Respawn entity comes back (factory re-run with its original
    // LdtkEntity) when its room is loaded, the player is farther than its respawnDistance from spawnPos and
    // spawnPos is outside view = (minX, minY, maxX, maxY).
    void checkRespawns(glm::vec2 playerPos, glm::vec4 view, double time);

    static constexpr float FadeSeconds = 0.2f; // each way: dark, die, light (spec decision 7)
    // Death: puts the current room's state back as it was on entry (the snapshot loadAround takes; values
    // "activated" are kept, so a checkpoint stays the respawn point), reloads that room, synchronously streams
    // around respawnPoint and calls teleportPlayer(respawnPoint).
    void onPlayerDeath(glm::vec2 respawnPoint, std::function<void(glm::vec2)> teleportPlayer);
    // Death behind a fade: stepFade darkens over FadeSeconds, runs onPlayerDeath at black and lightens again.
    // Ignored while a death is fading.
    void killPlayer(glm::vec2 respawnPoint, std::function<void(glm::vec2)> teleportPlayer);
    bool dying() const { return deathTime_ >= 0.0f; }
    float stepFade(float dt); // advances the death fade; returns FrameSettings::fade (1 = visible)

    int currentRoom() const { return current_; }
    const std::vector<int>& loadedRooms() const { return loaded_; } // ascending
    // First room containing pos (edges inclusive), or -1.
    int roomAt(glm::vec2 pos) const;
    // Index of the room whose level iid is roomIid, or -1.
    int roomOf(const std::string& roomIid) const;
    const LdtkLevel& room(int i) const { return project_.levels[i]; }
    // Rect around all rooms: (minX, minY, maxX, maxY), world units.
    glm::vec4 worldBounds() const;
    const LdtkProject& project() const { return project_; }

private:
    // A persistent entity (policy != None), kept while the project is open (spec decision 8).
    struct Tracked { int room; const LdtkEntity* entity; Persistence policy; float respawnDistance; };

    glm::vec4 rect(int room) const; // (minX, minY, maxX, maxY)
    void load(int room);
    void unload(int room, bool notify = true); // notify: fire onRoomUnloaded
    void spawn(int room, const LdtkEntity& e);

    LdtkProject project_;
    std::vector<std::vector<int>> neighbours_;
    std::vector<int> loaded_;
    std::map<std::string, Tracked> tracked_; // by iid; entity points into project_
    std::vector<std::vector<std::string>> roomIids_; // per room, every entity iid (the snapshot's scope)
    std::map<std::string, EntityState> snapshot_;    // the current room's entries on entry
    float deathTime_ = -1.0f;                        // < 0: no death fading
    glm::vec2 deathPoint_{0.0f};
    std::function<void(glm::vec2)> deathTeleport_;
    double nextRespawnCheck_ = 0.0;
    int current_ = -1;
    LevelLoader* loader_ = nullptr;
    EngineServices* services_ = nullptr;
};
