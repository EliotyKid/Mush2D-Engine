#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// A block without the ice: no shield nor attack for guardBreakStun (the hit itself passed: "hurt" meanwhile).
struct GuardBreakAction : State<IkiObject> {
    const char* name() const override { return "guard_break"; }
    const char* update(IkiObject& iki, float) override {
        return iki.actions().timeInState() >= iki.guardBreakStun ? "none" : nullptr;
    }
};
