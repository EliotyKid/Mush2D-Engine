#include "ikigai/IkigaiGame.hpp"

#include "engine/core/Paths.hpp"
#include "engine/render/SpriteBatch.hpp"

void IkigaiGame::init(EngineServices& s) {
    prefabs = PrefabLibrary(assetPath("assets/prefabs")); // where the player prefab lives
    s.scene.setService(&prefabs);                          // the level loader finds prefabs through the scene
    LdtkProject project = loadLdtkProject(assetPath("assets/levels/ikigai.ldtk")); // read the room file
    world.open(project, levelLoader, &s);                  // hand the room to the world
    world.loadAround(0);                                   // load room 0: its collision and its entities
    s.scene.flush();                                       // make the new objects visible to forEach
    const glm::vec4 b = world.worldBounds();               // the room's rectangle: minX, minY, maxX, maxY
    s.camera.bounds = Camera2D::WorldBounds{b.x, b.z, b.y, b.w}; // the camera stays inside the room
    s.scene.forEach([&](GameObject& o) {                   // look for the object named "Player"
        if (o.name == "Player") s.camera.targetId = o.id;  // and make the camera follow it
    });
}

void IkigaiGame::update(EngineServices& s, float dt) {
    s.scene.step(dt);                                      // runs every object and component (the mover walks here)
    s.scene.lateStep(dt);                                  // the second pass of the step
    SpriteBatch& batch = s.renderer.sprites();             // the list of sprites to draw this frame
    batch.clear();                                         // start the list empty
    levelLoader.appendStatic(batch);                       // add the room's tiles
    // There is no atlas yet: every sprite is drawn as a plain colored rectangle (the white material).
    appendSceneSprites(s.scene, batch, [&](std::uint32_t, const std::string&) {
        return std::pair{s.materials.white(), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
    }, &s.materials.white());
    s.renderer.lights().clear();                           // no lights in this room
}

void IkigaiGame::shutdown(EngineServices& s) {
    s.scene.clear();                                       // remove every object
    s.scene.setService<PrefabLibrary>(nullptr);            // the scene must not keep a pointer to a dead library
}
