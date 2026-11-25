#pragma once

#include "engine/world/Component.hpp"

#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// One burst's look and motion. Angles in degrees (0 = +x, counter-clockwise); speed, angle and lifetime are
// [min, max] picked per particle; size is [start, end] (world units, square) and the colour goes from
// colorStart to colorEnd over the life. frame: a frame of the owner's sprite atlas; empty = a tinted white pixel.
struct ParticleConfig {
    int count = 1;
    glm::vec2 speed{1.0f, 1.0f}, angle{0.0f, 360.0f}, lifetime{0.5f, 0.5f};
    float gravity = 0.0f; // world units / s^2 along y (negative falls)
    float drag = 0.0f;    // velocity lost per second, as a fraction
    glm::vec2 size{0.1f, 0.0f};
    glm::vec4 colorStart{1.0f}, colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
    std::string frame;
    bool additive = false;
    int layer = 0;
};

// Reads {"count", "speed", "angle", "lifetime", "gravity", "drag", "size", "color": [start, end], "frame",
// "additive", "layer"}; missing keys keep the defaults. False with a message on an unknown key or a wrong type.
bool parseParticleConfig(const nlohmann::json& j, ParticleConfig& out, std::string& error);

struct Particle {
    glm::vec2 position{0.0f}, velocity{0.0f};
    float age = 0.0f, life = 1.0f;
    std::shared_ptr<const ParticleConfig> config; // shared by the burst
};

// CPU particles in world space (they stay where they were emitted when the owner moves). Drawn by
// appendParticles as sprites: lit by the scene irradiance, never a light source, no emission. Over maxParticles the oldest are dropped.
class ParticleEmitter : public Component {
    MUSH2D_COMPONENT(ParticleEmitter)
public:
    MUSH2D_EXPORT(int, maxParticles, 128)

    // direction -1 mirrors the angles (the owner faces left).
    void burst(const ParticleConfig& config, glm::vec2 position, int direction = 1);
    std::size_t alive() const { return particles_.size(); }
    const std::vector<Particle>& particles() const { return particles_; } // oldest first

    void onStep(GameObject& owner, Scene2D& scene, float dt) override;

private:
    std::vector<Particle> particles_;
    std::minstd_rand rng_{12345};
};
