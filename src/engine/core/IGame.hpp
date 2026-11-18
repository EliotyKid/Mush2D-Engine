#pragma once

#include "engine/gpu/GpuContext.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/platform/GlfwInput.hpp"
#include "engine/world/Scene2D.hpp"

// Task 5 adds `SceneRenderer& renderer`.
struct EngineServices {
    GpuContext& gpu;
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
