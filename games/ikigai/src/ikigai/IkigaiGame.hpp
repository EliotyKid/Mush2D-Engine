#pragma once

#include "engine/core/IGame.hpp"
#include "engine/world/LevelLoader.hpp"
#include "engine/world/Prefab.hpp"
#include "engine/world/World.hpp"

// The starting point of the Ikigai game: it loads one room, and the camera follows the player.
// Everything else (characters, rooms, rules) is yours to add here and in games/ikigai/assets/.
class IkigaiGame : public IGame {
public:
    void init(EngineServices& s) override;
    void update(EngineServices& s, float dt) override;
    void shutdown(EngineServices& s) override;

private:
    PrefabLibrary prefabs; // builds objects from assets/prefabs/*.prefab.json
    LevelLoader levelLoader; // turns the LDtk room into collision and objects
    World world;           // keeps the loaded room(s) in the scene
};
