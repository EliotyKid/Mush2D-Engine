cmake_minimum_required(VERSION 3.20)
project(Mush2DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(Mush2DEngine
    src/main.cpp
    src/app/EngineApp.cpp
    src/graphics/Renderer2D.cpp
    src/graphics/Camera2D.cpp
    src/platform/GlfwInput.cpp
    src/world/Collision.cpp
    src/gameplay/Spawn.cpp
    src/gameplay/SandboxScene.cpp
    src/gameplay/TestScene.cpp
)

target_include_directories(Mush2DEngine PRIVATE
    external
    src
    src/app
    src/graphics
    src/world
    src/gameplay
    src/platform
)

target_link_libraries(Mush2DEngine
    PRIVATE
    glfw
    Vulkan::Vulkan
)