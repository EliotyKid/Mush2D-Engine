#pragma once

#include "engine/reflect/Reflect.hpp"

class GameObject;
class Scene2D;

// Behaviour attached to a GameObject. onStep runs right after the owner's onStep, in insertion order.
// Exportable like objects: see MUSH2D_COMPONENT in Reflect.hpp. The base exports `enabled`.
class Component {
public:
    virtual ~Component() = default;

    // Overridden by MUSH2D_COMPONENT; use typeOf(component).
    virtual const TypeInfo& reflectType() const;

    virtual void onAttach(GameObject&, Scene2D&) {}
    virtual void onStep(GameObject&, Scene2D&, float /*deltaTime*/) {}
    virtual void onDetach(GameObject&, Scene2D&) {}

    bool enabled = true;
    bool removing() const { return removing_; }

private:
    friend class GameObject;
    friend class Scene2D;
    bool removing_ = false; // removeComponent() was called; detached at the next flush
};
