#pragma once

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
};

class IGame {
public:
    virtual ~IGame() = default;
    virtual void init(EngineServices&) = 0;
    virtual void update(EngineServices&, float dt) = 0;
    virtual void onImGui(EngineServices&) {}
    virtual void shutdown(EngineServices&) {}
};
