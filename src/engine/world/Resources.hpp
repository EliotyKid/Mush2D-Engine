#pragma once

#include "engine/world/Component.hpp"
#include "engine/world/Damage.hpp"
#include "engine/world/GameObject.hpp"

// Events on scene.events(), delivered at the next flush.
struct HealthChanged { GameObjectId id; int health; int max; };
struct EnergyChanged { GameObjectId id; Element element; float value; float max; };
struct Died { GameObjectId id; }; // health reached 0

// Health and the ice / fire energies. Starts full. Each energy regenerates regenPerSecond once regenDelay has
// passed since that element's last spend. onStep calls update(); the owner does not call it.
class Resources : public Component {
    MUSH2D_COMPONENT(Resources)
public:
    MUSH2D_EXPORT(int, maxHealth, 5)
    MUSH2D_EXPORT(float, maxIce, 100.0f)
    MUSH2D_EXPORT(float, maxFire, 100.0f)
    MUSH2D_EXPORT(float, regenPerSecond, 10.0f)
    MUSH2D_EXPORT(float, regenDelay, 1.0f)

    int health() const;
    float energy(Element element) const; // 0 for Element::None

    void damage(int amount); // ignored once dead
    void heal(int amount);
    void refill(); // health and both energies back to the max (respawn)

    // False, spending nothing, when there is not enough energy or the amount is negative. 0 is true, no effect.
    bool spend(Element element, float amount);
    bool spendBoth(float ice, float fire); // both or neither; a 0 side is left alone

    void update(Scene2D& scene, float dt);

    void onAttach(GameObject& owner, Scene2D& scene) override;
    void onStep(GameObject&, Scene2D& scene, float dt) override { update(scene, dt); }

private:
    float max(Element element) const;
    void emitHealth();
    void emitEnergy(Element element);

    // Stored as what is missing, so a max set after attaching (prefab, inspector) still starts full.
    int lost_ = 0;
    float used_[2] = {0.0f, 0.0f};                    // Ice, Fire
    float sinceSpend_[2] = {1e9f, 1e9f};
    Scene2D* scene_ = nullptr;
    GameObjectId owner_ = InvalidGameObjectId;
};
