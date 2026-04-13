#pragma once

#include "Components.hpp"
#include "GameObject.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Scene2D {
public:
    GameObject& createObject(const std::string& name = "") {
        GameObject object{};
        object.id = nextId++;
        object.name = name;
        objects.push_back(object);
        return objects.back();
    }

    GameObject* findObject(GameObjectId id) {
        for (auto& object : objects) {
            if (object.id == id) {
                return &object;
            }
        }
        return nullptr;
    }

    const GameObject* findObject(GameObjectId id) const {
        for (const auto& object : objects) {
            if (object.id == id) {
                return &object;
            }
        }
        return nullptr;
    }

    bool hasSprite(GameObjectId id) const {
        return sprites.find(id) != sprites.end();
    }

    bool hasPlayer(GameObjectId id) const {
        return players.find(id) != players.end();
    }

    bool hasCollider(GameObjectId id) const {
        return colliders.find(id) != colliders.end();
    }

    bool hasTrigger(GameObjectId id) const {
        return triggers.find(id) != triggers.end();
    }

public:
    std::vector<GameObject> objects;
    std::unordered_map<GameObjectId, SpriteComponent> sprites;
    std::unordered_map<GameObjectId, PlayerComponent> players;
    std::unordered_map<GameObjectId, ColliderComponent> colliders;
    std::unordered_map<GameObjectId, TriggerComponent> triggers;
    std::unordered_map<GameObjectId, PlayerStateMachineComponent> playerStateMachines;

private:
    GameObjectId nextId = 1;
};