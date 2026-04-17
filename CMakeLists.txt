cmake_minimum_required(VERSION 3.20)
project(Mush2DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(Mush2DEngine
    src/main.cpp
    src/app/EngineApp.cpp

    src/engine/graphics/Renderer2D.cpp
    src/engine/graphics/Camera2D.cpp
    src/engine/platform/GlfwInput.cpp
    src/engine/world/Collision.cpp
    src/engine/scenes/SceneRegistry.cpp
    src/engine/assets/TextureAtlas.cpp
    src/engine/assets/AtlasLoader.cpp

    src/game/spawners/Spawn.cpp
    src/game/scenes/SandboxScene.cpp
    src/game/scenes/TestScene.cpp
    src/game/assets/GameAssets.cpp
)

target_include_directories(Mush2DEngine PRIVATE
    external
    src
    src/app
    src/engine
    src/engine/platform
    src/engine/graphics
    src/engine/world
    src/game
    src/game/scenes
    src/game/objects
    src/game/spawners
)

target_link_libraries(Mush2DEngine
    PRIVATE
    glfw
    Vulkan::Vulkan
    nlohmann_json::nlohmann_json
)