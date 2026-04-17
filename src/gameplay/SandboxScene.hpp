#pragma once

#include "ISceneDefinition.hpp"

class SandboxScene : public ISceneDefinition {
public:
    BuildResult build(Scene2D& scene) override;
};