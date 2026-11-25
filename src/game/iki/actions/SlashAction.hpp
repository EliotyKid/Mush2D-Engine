#pragma once

#include "engine/world/StateMachine.hpp"
#include "game/objects/IkiObject.hpp"

// Slash n (1..3), for slashDuration. Its hitbox (child Slash<n>) is active between the animation's hit_on and
// hit_off or, without them, between the hitbox's activeStart and activeEnd. X meanwhile chains the next slash at
// the end; V (shield) and C (shard) cancel the sequence.
struct SlashAction : State<IkiObject> {
    explicit SlashAction(int n) : n(n) {}
    int n;
    const char* name() const override { return n == 1 ? "slash1" : n == 2 ? "slash2" : "slash3"; }
    void enter(IkiObject& iki) override { iki.beginSlash(n); }
    void exit(IkiObject& iki) override { iki.endSlash(n); }
    const char* update(IkiObject& iki, float) override {
        if (iki.actionsBlocked()) return "none";
        if (iki.guardPressed() && iki.canGuard()) return "guard"; // GuardAction resets the sequence
        if (iki.shootPressed())
            if (const char* shot = iki.tryShoot()) return shot;
        if (iki.attackPressed()) iki.attackBuffered = true;
        const float t = iki.actions().timeInState();
        iki.slashWindow(n, t);
        if (t < iki.slashDuration) return nullptr;
        if (iki.attackBuffered && n < 3) return n == 1 ? "slash2" : "slash3";
        iki.openCombo(n < 3 ? n + 1 : 1);
        return "none";
    }
};
