#include "engine/world/Resources.hpp"

#include "engine/world/Scene2D.hpp"

#include <algorithm>

int Resources::health() const { return std::max(0, maxHealth - lost_); }

int Resources::indexOf(std::string_view bar) const {
    for (std::size_t i = 0; i < bars_.size(); ++i)
        if (bars_[i].name == bar) return int(i);
    return -1;
}

float Resources::maxOf(std::string_view bar) const {
    const int i = indexOf(bar);
    return i < 0 ? 0.0f : bars_[i].max;
}

float Resources::energy(std::string_view bar) const {
    const int i = indexOf(bar);
    return i < 0 ? 0.0f : std::max(0.0f, bars_[i].max - used_[i]);
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
    std::fill(used_.begin(), used_.end(), 0.0f);
    emitHealth();
    for (int i = 0; i < int(bars_.size()); ++i) emitEnergy(i);
}

bool Resources::spend(std::string_view bar, float amount) {
    const int i = indexOf(bar);
    if (i < 0 || amount < 0.0f) return false;
    if (amount == 0.0f) return true; // nothing spent: no event, the regen delay keeps running
    if (energy(bar) < amount) return false;
    used_[i] += amount;
    sinceSpend_[i] = 0.0f;
    emitEnergy(i);
    return true;
}

bool Resources::spendAll(std::initializer_list<std::pair<std::string_view, float>> costs) {
    for (const auto& [bar, amount] : costs) // first pass: check everything, change nothing
        if (indexOf(bar) < 0 || amount < 0.0f || energy(bar) < amount) return false;
    for (const auto& [bar, amount] : costs) spend(bar, amount); // second pass: it cannot fail now
    return true;
}

void Resources::update(Scene2D&, float dt) {
    for (int i = 0; i < int(bars_.size()); ++i) {
        sinceSpend_[i] += dt;
        if (sinceSpend_[i] < bars_[i].regenDelay || used_[i] <= 0.0f) continue;
        used_[i] = std::max(0.0f, used_[i] - bars_[i].regenPerSecond * dt);
        emitEnergy(i);
    }
}

void Resources::onAttach(GameObject& owner, Scene2D& scene) {
    owner_ = owner.id;
    scene_ = &scene;
    bars_ = activeProject().resources; // every object starts with the project's bars, full
    used_.assign(bars_.size(), 0.0f);
    sinceSpend_.assign(bars_.size(), 1e9f); // 1e9: no recent spend, so regen is not held back
}

void Resources::emitHealth() {
    if (scene_) scene_->events().emit(HealthChanged{owner_, health(), maxHealth});
}

void Resources::emitEnergy(int index) {
    if (scene_) scene_->events().emit(EnergyChanged{owner_, bars_[index].name, energy(bars_[index].name), bars_[index].max});
}
