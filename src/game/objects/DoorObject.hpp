#pragma once

#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

#include <string>

// Solid while closed. Its "open" value lives in WorldState, so a lever can open it while its room is unloaded.
class DoorObject : public GameObject {
    MUSH2D_OBJECT(DoorObject, GameObject)
public:
    DoorObject(WorldState& state, std::string iid, glm::vec2 size) : state(state), iid(std::move(iid)), size(size) {}

    void onCreate(Scene2D& scene) override {
        name = "Door";
        addTag("door");
        transform.scale = size;
        sprite = SpriteData{.layer = 1, .color = glm::vec4(0.55f, 0.4f, 0.3f, 1.0f)};
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Wall);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }
        collider = ColliderData{.size = size};
        refresh();
    }

    void onStep(Scene2D&, float) override { refresh(); }

    bool isOpen() const { return state.get(iid, "open", 0.0) != 0.0; }
    const std::string& entityIid() const { return iid; }

private:
    void refresh() {
        collider->enabled = !isOpen();
        visible = !isOpen();
    }

    WorldState& state;
    std::string iid;
    glm::vec2 size;
};
