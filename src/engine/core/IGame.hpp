#pragma once

#include "engine/audio/Audio.hpp"
#include "engine/gpu/GpuContext.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/platform/GlfwInput.hpp"
#include "engine/render/Materials.hpp"
#include "engine/render/SceneRenderer.hpp"
#include "engine/tools/SceneInspector.hpp"
#include "engine/world/Scene2D.hpp"

struct EngineServices {
    GpuContext& gpu;
    SceneRenderer& renderer;
    MaterialLibrary& materials;
    GlfwInput& input;
    Camera2D& camera;
    Scene2D& scene;
    Audio& audio;
    SceneInspector& inspector; // the F1 "Cena" window the engine draws: selected() and pausing() for the game
};

class IGame {
public:
    virtual ~IGame() = default;
    virtual void init(EngineServices&) = 0;
    virtual void update(EngineServices&, float dt) = 0;
    virtual void onHud(EngineServices&) {}   // every frame the F1 panel is hidden, inside an ImGui frame (a HUD)
    // Only while the F1 panel is shown, after the engine's own windows ("Cena", "Projeto"): the game's extra panels.
    virtual void onImGui(EngineServices&) {}
    virtual void shutdown(EngineServices&) {}
};
