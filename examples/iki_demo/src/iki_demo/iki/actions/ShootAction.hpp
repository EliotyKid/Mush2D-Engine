#pragma once

#include "engine/world/StateMachine.hpp"
#include "iki_demo/objects/IkiObject.hpp"

// The shard left (IkiObject::tryShoot fired it): one step, for the "state:action:shoot" trigger.
struct ShootAction : State<IkiObject> {
    const char* name() const override { return "shoot"; }
    const char* update(IkiObject&, float) override { return "none"; }
};
