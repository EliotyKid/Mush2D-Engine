#include "SceneRegistry.hpp"

#include "../../game/scenes/SandboxScene.hpp"
#include "../../game/scenes/TestScene.hpp"

#include <stdexcept>

std::unique_ptr<ISceneDefinition> SceneRegistry::create(SceneId sceneId) {
    switch (sceneId) {
        case SceneId::Sandbox:
            return std::make_unique<SandboxScene>();

        case SceneId::Test:
            return std::make_unique<TestScene>();
    }

    throw std::runtime_error("SceneId invalido ao criar definicao de cena.");
}