#pragma once

#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"
#include "game/objects/PlayerObject.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"

#include <string>

// Trigger: while the player stands on it, pressing down toggles the lever ("on") and its door ("open").
class LeverObject : public GameObject {
public:
    LeverObject(WorldState& state, std::string iid, std::string doorIid)
        : state(state), iid(std::move(iid)), doorIid(std::move(doorIid)) {}

    void onCreate(Scene2D& scene) override {
        name = "Lever";
        addTag("trigger");
        addTag("lever");
        transform.scale = {0.6f, 0.6f};
        sprite = SpriteData{.layer = 1, .orderInLayer = 2};
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Teleport);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }
        collider = ColliderData{.size = {1.0f, 1.0f}, .isTrigger = true};
        refresh();
    }

    void onStep(Scene2D& scene, float) override {
        auto* player = scene.findObjectAs<PlayerObject>(playerId);
        if (player && player->interactPressed()) toggle();
        refresh();
    }

    void onTriggerEnter(Scene2D&, GameObject& other) override {
        if (dynamic_cast<PlayerObject*>(&other)) playerId = other.id;
    }
    void onTriggerExit(Scene2D&, GameObject& other) override {
        if (other.id == playerId) playerId = InvalidGameObjectId;
    }

    void toggle() {
        state.set(iid, "on", isOn() ? 0.0 : 1.0);
        if (!doorIid.empty()) state.set(doorIid, "open", state.get(doorIid, "open", 0.0) != 0.0 ? 0.0 : 1.0);
    }
    bool isOn() const { return state.get(iid, "on", 0.0) != 0.0; }

private:
    void refresh() { sprite->color = isOn() ? glm::vec4(0.4f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f); }

    WorldState& state;
    std::string iid, doorIid;
    GameObjectId playerId = InvalidGameObjectId;
};
