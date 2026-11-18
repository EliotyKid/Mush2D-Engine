#pragma once

#include "engine/render/SceneRenderer.hpp"
#include "engine/world/Component.hpp"

class Scene2D;

// A light attached to a GameObject. light.posHeightRadius.xy is LOCAL to the owner.
class LightComponent : public Component {
public:
    GpuLight light{};
};

// Adds every enabled LightComponent of every active owner, with its position moved to world space
// (owner world transform: scale, then rotation, then translation). Call each frame after the level lights.
void collectComponentLights(const Scene2D& scene, LightList& lights);
