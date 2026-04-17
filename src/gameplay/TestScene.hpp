#pragma once

#include "ISceneDefinition.hpp"

class TestScene : public ISceneDefinition {
public:
    BuildResult build(Scene2D& scene) override;
};