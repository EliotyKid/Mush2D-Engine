#include "engine/render/LightComponent.hpp"

#include "engine/world/Scene2D.hpp"

#include <cmath>

void collectComponentLights(const Scene2D& scene, LightList& lights) {
    scene.forEach([&](const GameObject& owner) {
        if (!owner.active) return;
        const Transform2D world = owner.world();
        const float c = std::cos(world.rotation), s = std::sin(world.rotation);
        for (const auto& component : owner.components()) {
            const auto* lc = dynamic_cast<const LightComponent*>(component.get());
            if (!lc || !lc->enabled || lc->removing()) continue;
            GpuLight l = lc->light;
            const glm::vec2 scaled = world.scale * glm::vec2(l.posHeightRadius);
            l.posHeightRadius.x = world.position.x + c * scaled.x - s * scaled.y;
            l.posHeightRadius.y = world.position.y + s * scaled.x + c * scaled.y;
            lights.add(l);
        }
    });
}
