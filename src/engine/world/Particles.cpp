#include "engine/world/Particles.hpp"

#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace {
bool isNumbers(const json& j, std::size_t n) {
    return j.is_array() && j.size() == n && std::all_of(j.begin(), j.end(), [](const json& v) { return v.is_number(); });
}
glm::vec2 vec2Of(const json& j) { return {j[0].get<float>(), j[1].get<float>()}; }
glm::vec4 vec4Of(const json& j) { return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()}; }
}

bool parseParticleConfig(const json& j, ParticleConfig& out, std::string& error) {
    if (!j.is_object()) return error = "particles: expected an object", false;
    ParticleConfig c = out;
    for (const auto& [key, v] : j.items()) {
        auto bad = [&](const char* expected) { error = "particles." + key + ": expected " + expected; return false; };
        if (key == "count") { if (!v.is_number_integer()) return bad("an integer"); c.count = v.get<int>(); }
        else if (key == "layer") { if (!v.is_number_integer()) return bad("an integer"); c.layer = v.get<int>(); }
        else if (key == "gravity") { if (!v.is_number()) return bad("a number"); c.gravity = v.get<float>(); }
        else if (key == "drag") { if (!v.is_number()) return bad("a number"); c.drag = v.get<float>(); }
        else if (key == "additive") { if (!v.is_boolean()) return bad("true or false"); c.additive = v.get<bool>(); }
        else if (key == "frame") { if (!v.is_string()) return bad("a frame name"); c.frame = v.get<std::string>(); }
        else if (key == "speed" || key == "angle" || key == "lifetime" || key == "size") {
            if (!isNumbers(v, 2)) return bad("[a, b]");
            (key == "speed" ? c.speed : key == "angle" ? c.angle : key == "lifetime" ? c.lifetime : c.size) = vec2Of(v);
        } else if (key == "color") {
            if (!v.is_array() || v.size() != 2 || !isNumbers(v[0], 4) || !isNumbers(v[1], 4)) return bad("[[r, g, b, a], [r, g, b, a]]");
            c.colorStart = vec4Of(v[0]);
            c.colorEnd = vec4Of(v[1]);
        } else {
            return error = "particles: unknown key '" + key + "'", false;
        }
    }
    out = c;
    return true;
}

void ParticleEmitter::burst(const ParticleConfig& config, glm::vec2 position, int direction) {
    auto shared = std::make_shared<const ParticleConfig>(config);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    auto pick = [&](glm::vec2 range) { return range.x + (range.y - range.x) * unit(rng_); };
    const std::size_t cap = std::size_t(std::max(maxParticles, 0));
    particles_.reserve(cap);
    for (int i = 0; i < config.count && cap > 0; ++i) {
        while (particles_.size() >= cap) particles_.erase(particles_.begin()); // drop the oldest
        float deg = pick(config.angle);
        if (direction < 0) deg = 180.0f - deg;
        const float speed = pick(config.speed), rad = glm::radians(deg);
        particles_.push_back({position, speed * glm::vec2(std::cos(rad), std::sin(rad)), 0.0f, pick(config.lifetime), shared});
    }
}

void ParticleEmitter::onStep(GameObject&, Scene2D&, float dt) {
    for (Particle& p : particles_) {
        p.age += dt;
        p.velocity.y += p.config->gravity * dt;
        p.velocity *= std::max(0.0f, 1.0f - p.config->drag * dt);
        p.position += p.velocity * dt;
    }
    std::erase_if(particles_, [](const Particle& p) { return p.age >= p.life; });
}
