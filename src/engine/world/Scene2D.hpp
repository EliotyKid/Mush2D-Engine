#pragma once

#include "GameObject.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

class Scene2D {
public:
    template<typename TObject, typename... Args>
    TObject& createObject(Args&&... args) {
        static_assert(std::is_base_of_v<GameObject, TObject>,
                      "TObject precisa herdar de GameObject.");

        auto object = std::make_unique<TObject>(std::forward<Args>(args)...);
        object->id = nextId++;

        TObject& ref = *object;
        objects.push_back(std::move(object));
        ref.onCreate(*this);
        return ref;
    }

    void clear() {
        for (auto& object : objects) {
            if (object) {
                object->onDestroy(*this);
            }
        }

        objects.clear();
        nextId = 1;
    }

    GameObject* findObject(GameObjectId id) {
        for (auto& object : objects) {
            if (object && object->id == id) {
                return object.get();
            }
        }
        return nullptr;
    }

    const GameObject* findObject(GameObjectId id) const {
        for (const auto& object : objects) {
            if (object && object->id == id) {
                return object.get();
            }
        }
        return nullptr;
    }

    template<typename TObject>
    TObject* findObjectAs(GameObjectId id) {
        GameObject* object = findObject(id);
        return object ? dynamic_cast<TObject*>(object) : nullptr;
    }

    template<typename TObject>
    const TObject* findObjectAs(GameObjectId id) const {
        const GameObject* object = findObject(id);
        return object ? dynamic_cast<const TObject*>(object) : nullptr;
    }

    template<typename TObject>
    TObject* findFirstObjectOfType() {
        for (auto& object : objects) {
            if (!object) {
                continue;
            }

            if (auto* casted = dynamic_cast<TObject*>(object.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename TObject>
    const TObject* findFirstObjectOfType() const {
        for (const auto& object : objects) {
            if (!object) {
                continue;
            }

            if (auto* casted = dynamic_cast<const TObject*>(object.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename TObject>
    std::vector<TObject*> findAllObjectsOfType() {
        std::vector<TObject*> result;

        for (auto& object : objects) {
            if (!object) {
                continue;
            }

            if (auto* casted = dynamic_cast<TObject*>(object.get())) {
                result.push_back(casted);
            }
        }

        return result;
    }

    std::vector<GameObject*> findObjectsByTag(const std::string& tag) {
        std::vector<GameObject*> result;

        for (auto& object : objects) {
            if (object && object->hasTag(tag)) {
                result.push_back(object.get());
            }
        }

        return result;
    }

    std::vector<const GameObject*> findObjectsByTag(const std::string& tag) const {
        std::vector<const GameObject*> result;

        for (const auto& object : objects) {
            if (object && object->hasTag(tag)) {
                result.push_back(object.get());
            }
        }

        return result;
    }

    void step(float deltaTime) {
        for (auto& object : objects) {
            if (object && object->active) {
                object->onStep(*this, deltaTime);
            }
        }
    }

    void lateStep(float deltaTime) {
        for (auto& object : objects) {
            if (object && object->active) {
                object->onLateStep(*this, deltaTime);
            }
        }
    }

    std::vector<std::unique_ptr<GameObject>>& getObjects() {
        return objects;
    }

    const std::vector<std::unique_ptr<GameObject>>& getObjects() const {
        return objects;
    }

private:
    GameObjectId nextId = 1;
    std::vector<std::unique_ptr<GameObject>> objects;
};