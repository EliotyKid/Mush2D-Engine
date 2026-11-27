#pragma once

#include "iki_demo/assets/GameAssets.hpp"
#include "iki_demo/assets/GameSpriteId.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

// Solid while closed. Its "open" value lives in the WorldState service under its entityIid, so a lever can open
// it while its room is unloaded. Prefab: assets/prefabs/door.prefab.json; from LDtk it takes the entity box's size.
class DoorObject : public GameObject {
    MUSH2D_OBJECT(DoorObject, GameObject)
public:
    void onCreate(Scene2D& scene) override {
        state = scene.service<WorldState>();
        name = "Door";
        addTag("door");
        sprite = SpriteData{.layer = 1, .color = glm::vec4(0.55f, 0.4f, 0.3f, 1.0f)};
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Wall);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }
        collider = ColliderData{};
        refresh();
    }

    void onPrefabApplied(Scene2D&) override {
        glm::vec2 size{1.0f};
        if (extra().contains("ldtkSize") && reflect::vecFromJson(extra()["ldtkSize"], size)) {
            transform.scale = size;
            collider->size = size;
        }
        refresh();
    }

    void onStep(Scene2D&, float) override { refresh(); }

    bool isOpen() const { return state && state->get(entityIid, "open", 0.0) != 0.0; }

private:
    void refresh() {
        collider->enabled = !isOpen();
        visible = !isOpen();
    }

    WorldState* state = nullptr;
};
