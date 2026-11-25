#pragma once

#include "engine/audio/Audio.hpp"
#include "engine/gpu/GpuContext.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/platform/GlfwInput.hpp"
#include "engine/render/Materials.hpp"
#include "engine/render/SceneRenderer.hpp"
#include "engine/world/Scene2D.hpp"

struct EngineServices {
    GpuContext& gpu;
    SceneRenderer& renderer;
    MaterialLibrary& materials;
    GlfwInput& input;
    Camera2D& camera;
    Scene2D& scene;
    Audio& audio;
};

class IGame {
public:
    virtual ~IGame() = default;
    virtual void init(EngineServices&) = 0;
    virtual void update(EngineServices&, float dt) = 0;
    virtual void onHud(EngineServices&) {}   // every frame the F1 panel is hidden, inside an ImGui frame (a HUD)
    virtual void onImGui(EngineServices&) {} // only while the F1 panel is shown
    virtual void shutdown(EngineServices&) {}
};
