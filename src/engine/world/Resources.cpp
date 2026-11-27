#include "engine/world/Resources.hpp"

#include "engine/world/Scene2D.hpp"

#include <algorithm>

namespace {
int slot(Energy element) { return element == Energy::Ice ? 0 : 1; }
}

int Resources::health() const { return std::max(0, maxHealth - lost_); }

float Resources::max(Energy element) const {
    return element == Energy::Ice ? maxIce : element == Energy::Fire ? maxFire : 0.0f;
}

float Resources::energy(Energy element) const {
    if (element == Energy::None) return 0.0f;
    return std::max(0.0f, max(element) - used_[slot(element)]);
}

void Resources::damage(int amount) {
    if (amount <= 0 || health() == 0) return;
    lost_ = std::min(maxHealth, lost_ + amount);
    emitHealth();
    if (health() == 0 && scene_) scene_->events().emit(Died{owner_});
}

void Resources::heal(int amount) {
    if (amount <= 0 || lost_ == 0) return;
    lost_ = std::max(0, lost_ - amount);
    emitHealth();
}

void Resources::refill() {
    lost_ = 0;
    used_[0] = used_[1] = 0.0f;
    emitHealth();
    emitEnergy(Energy::Ice);
    emitEnergy(Energy::Fire);
}

bool Resources::spend(Energy element, float amount) {
    if (amount == 0.0f) return true; // nothing spent: no event, the regen delay keeps running
    if (amount < 0.0f || element == Energy::None || energy(element) < amount) return false;
    used_[slot(element)] += amount;
    sinceSpend_[slot(element)] = 0.0f;
    emitEnergy(element);
    return true;
}

bool Resources::spendBoth(float ice, float fire) {
    if (ice < 0.0f || fire < 0.0f || energy(Energy::Ice) < ice || energy(Energy::Fire) < fire) return false;
    spend(Energy::Ice, ice);
    spend(Energy::Fire, fire);
    return true;
}

void Resources::update(Scene2D&, float dt) {
    for (Energy element : {Energy::Ice, Energy::Fire}) {
        const int i = slot(element);
        sinceSpend_[i] += dt;
        if (sinceSpend_[i] < regenDelay || used_[i] <= 0.0f) continue;
        used_[i] = std::max(0.0f, used_[i] - regenPerSecond * dt);
        emitEnergy(element);
    }
}

void Resources::onAttach(GameObject& owner, Scene2D& scene) {
    owner_ = owner.id;
    scene_ = &scene;
}

void Resources::emitHealth() {
    if (scene_) scene_->events().emit(HealthChanged{owner_, health(), maxHealth});
}

void Resources::emitEnergy(Energy element) {
    if (scene_) scene_->events().emit(EnergyChanged{owner_, element, energy(element), max(element)});
}
