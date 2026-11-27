#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// Shield up while V is held on the ground (slow, no jump). The parry window counts from entering (each press).
// The blocking itself is IkiObject::guard, the Damageable's callback.
struct GuardAction : State<IkiObject> {
    const char* name() const override { return "guard"; }
    void enter(IkiObject& iki) override { iki.resetCombo(); }
    const char* update(IkiObject& iki, float) override {
        return iki.guardHeld() && iki.canGuard() ? nullptr : "none";
    }
};
