#include "engine/world/Resources.hpp"

#include "engine/world/Scene2D.hpp"

#include <algorithm>

namespace {
int slot(Element element) { return element == Element::Ice ? 0 : 1; }
}

int Resources::health() const { return std::max(0, maxHealth - lost_); }

float Resources::max(Element element) const {
    return element == Element::Ice ? maxIce : element == Element::Fire ? maxFire : 0.0f;
}

float Resources::energy(Element element) const {
    if (element == Element::None) return 0.0f;
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
    emitEnergy(Element::Ice);
    emitEnergy(Element::Fire);
}

bool Resources::spend(Element element, float amount) {
    if (element == Element::None || energy(element) < amount) return false;
    used_[slot(element)] += amount;
    sinceSpend_[slot(element)] = 0.0f;
    emitEnergy(element);
    return true;
}

bool Resources::spendBoth(float ice, float fire) {
    if (energy(Element::Ice) < ice || energy(Element::Fire) < fire) return false;
    spend(Element::Ice, ice);
    spend(Element::Fire, fire);
    return true;
}

void Resources::update(Scene2D&, float dt) {
    for (Element element : {Element::Ice, Element::Fire}) {
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

void Resources::emitEnergy(Element element) {
    if (scene_) scene_->events().emit(EnergyChanged{owner_, element, energy(element), max(element)});
}
