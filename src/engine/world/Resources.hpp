#pragma once

#include "engine/core/Project.hpp"
#include "engine/world/Component.hpp"
#include "engine/world/GameObject.hpp"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Events on scene.events(), delivered at the next flush.
struct HealthChanged { GameObjectId id; int health; int max; };
struct EnergyChanged { GameObjectId id; std::string bar; float value; float max; }; // `bar` is the bar's name
struct Died { GameObjectId id; }; // health reached 0

// Health and the named resource bars the game declares in project.json. Starts full. Each bar regenerates its
// regenPerSecond once its regenDelay has passed since its last spend. onStep calls update(); the owner does not.
// onAttach copies the bars from the active project; a prefab "resources" section can change them per bar.
class Resources : public Component {
    MUSH2D_COMPONENT(Resources)
public:
    MUSH2D_EXPORT(int, maxHealth, 5)

    int health() const;
    float energy(std::string_view bar) const; // what is left in the bar; 0 for an unknown bar
    float maxOf(std::string_view bar) const;  // the bar's max; 0 for an unknown bar

    void damage(int amount); // ignored once dead
    void heal(int amount);
    void refill(); // health and every bar back to the max (respawn)

    // False, spending nothing, when the bar is unknown, the amount is negative or there is not enough energy.
    // An amount of 0 on a known bar is true and does nothing.
    bool spend(std::string_view bar, float amount);
    // Spends every cost, or nothing when any one of them would fail (a 0 cost is left alone).
    bool spendAll(std::initializer_list<std::pair<std::string_view, float>> costs);

    // The bars (name, element, max, regen). The non-const one lets the F1 panel edit them live.
    const std::vector<ResourceBarDef>& bars() const { return bars_; }
    std::vector<ResourceBarDef>& bars() { return bars_; }

    void update(Scene2D& scene, float dt);

    void onAttach(GameObject& owner, Scene2D& scene) override;
    void onStep(GameObject&, Scene2D& scene, float dt) override { update(scene, dt); }

private:
    int indexOf(std::string_view bar) const; // -1 when there is no such bar
    void emitHealth();
    void emitEnergy(int index);

    // Stored as what is missing, so a max set after attaching (prefab, inspector) still starts full.
    int lost_ = 0;
    std::vector<ResourceBarDef> bars_; // copied from the project in onAttach
    std::vector<float> used_;          // per bar: how much is missing
    std::vector<float> sinceSpend_;    // per bar: seconds since its last spend
    Scene2D* scene_ = nullptr;
    GameObjectId owner_ = InvalidGameObjectId;
};
