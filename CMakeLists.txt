cmake_minimum_required(VERSION 3.20)
project(Mush2DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(Mush2DEngine
    src/main.cpp
    src/EngineApp.cpp
    src/Renderer2D.cpp
)

target_include_directories(Mush2DEngine PRIVATE
    external
    src
)

target_link_libraries(Mush2DEngine
    PRIVATE
    glfw
    Vulkan::Vulkan
)