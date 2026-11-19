#pragma once

#include "engine/world/Ldtk.hpp"

#include <functional>
#include <vector>

#include <glm/glm.hpp>

class LevelLoader;
struct EngineServices;

// Every room of an LDtk project lives in one Scene2D; World keeps the room holding the focus plus its
// neighbours loaded. A room leaves only when it is no longer a neighbour of the current room AND the focus
// is more than UnloadDistance from its rect, so standing on a border never thrashes.
class World {
public:
    static constexpr float UnloadDistance = 4.0f; // world units

    // Tests (no GPU): when set, these replace LevelLoader::loadRoom / unloadRoom. Argument = room index.
    std::function<void(int)> onLoad, onUnload;

    // Copies the project and forgets loaded rooms (nothing is unloaded: the caller clears the scene first).
    // services may be null only when onLoad/onUnload are set.
    void open(const LdtkProject& project, LevelLoader& loader, EngineServices* services);
    // Picks the current room (spec decision 4), loads it and its neighbours, unloads far rooms.
    void update(glm::vec2 focus);
    // Synchronously makes room current and loads it and its neighbours.
    void loadAround(int room);

    int currentRoom() const { return current_; }
    const std::vector<int>& loadedRooms() const { return loaded_; } // ascending
    // First room containing pos (edges inclusive), or -1.
    int roomAt(glm::vec2 pos) const;
    // Rect around all rooms: (minX, minY, maxX, maxY), world units.
    glm::vec4 worldBounds() const;
    const LdtkProject& project() const { return project_; }

private:
    glm::vec4 rect(int room) const; // (minX, minY, maxX, maxY)
    void load(int room);
    void unload(int room);

    LdtkProject project_;
    std::vector<std::vector<int>> neighbours_;
    std::vector<int> loaded_;
    int current_ = -1;
    LevelLoader* loader_ = nullptr;
    EngineServices* services_ = nullptr;
};
