cmake_minimum_required(VERSION 3.24)
project(Mush2DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED COMPONENTS glslc)
include(cmake/Dependencies.cmake)
include(cmake/Shaders.cmake)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp)

add_library(mush2d_engine STATIC ${ENGINE_SOURCES})
# external/ only holds stb_image.h and stb_image_write.h
target_include_directories(mush2d_engine PUBLIC src external)
target_compile_definitions(mush2d_engine PUBLIC $<$<CONFIG:Debug>:MUSH2D_SOURCE_DIR="${CMAKE_SOURCE_DIR}">)
target_link_libraries(mush2d_engine PUBLIC glfw glm::glm Vulkan::Vulkan nlohmann_json::nlohmann_json imgui vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator)

add_executable(Mush2DEngine src/main.cpp ${GAME_SOURCES})
target_link_libraries(Mush2DEngine PRIVATE mush2d_engine)
mush2d_compile_shaders(Mush2DEngine)
mush2d_copy_assets(Mush2DEngine)

enable_testing()
add_executable(mush2d_tests tests/tests.cpp)
target_link_libraries(mush2d_tests PRIVATE mush2d_engine)
target_compile_definitions(mush2d_tests PRIVATE MUSH2D_TEST_DATA="${CMAKE_SOURCE_DIR}/tests/data")
add_test(NAME mush2d_tests COMMAND mush2d_tests)
