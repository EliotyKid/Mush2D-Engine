#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// No action. V on the ground raises the shield; C shoots; X slashes (the next of the sequence within comboWindow).
struct NoneAction : State<IkiObject> {
    const char* name() const override { return "none"; }
    const char* update(IkiObject& iki, float) override {
        if (iki.actionsBlocked()) return nullptr;
        if (iki.guardPressed() && iki.canGuard()) return "guard";
        if (iki.shootPressed())
            if (const char* shot = iki.tryShoot()) return shot;
        if (iki.attackPressed()) return iki.startSlash(iki.comboLeft > 0.0f ? iki.comboNext : 1);
        return nullptr;
    }
};
