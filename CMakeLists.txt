cmake_minimum_required(VERSION 3.24)
project(Mush2DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED COMPONENTS glslc)
include(cmake/Dependencies.cmake)
include(cmake/Shaders.cmake)
include(cmake/Games.cmake)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp)

add_library(mush2d_engine STATIC ${ENGINE_SOURCES})
# external/ only holds stb_image.h and stb_image_write.h
target_include_directories(mush2d_engine PUBLIC src external)
target_include_directories(mush2d_engine PRIVATE ${miniaudio_SOURCE_DIR})
target_compile_definitions(mush2d_engine PRIVATE MA_NO_FLAC MA_NO_MP3) # audio is .wav only
find_package(Threads REQUIRED)
target_link_libraries(mush2d_engine PRIVATE Threads::Threads ${CMAKE_DL_LIBS}) # miniaudio: threads, and dlopen of the audio backends on Linux
target_compile_definitions(mush2d_engine PUBLIC GLFW_INCLUDE_NONE $<$<CONFIG:Debug>:MUSH2D_SOURCE_DIR="${CMAKE_SOURCE_DIR}">)
target_link_libraries(mush2d_engine PUBLIC glfw glm::glm Vulkan::Vulkan nlohmann_json::nlohmann_json imgui vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator)

# The games built into this program (cmake/Games.cmake). Each one is a static library <id>_game.
mush2d_add_game(iki_demo examples/iki_demo IkiDemoGame iki_demo/IkiDemoGame.hpp)
mush2d_write_games_list() # games_list.cpp: the table of the games above (sets MUSH2D_GAMES_LIST, MUSH2D_GAME_LIBS)

# Mush2D: the development program, which opens any compiled game (--game <id>).
add_executable(Mush2D src/app/main.cpp ${MUSH2D_GAMES_LIST})
target_link_libraries(Mush2D PRIVATE ${MUSH2D_GAME_LIBS})
mush2d_compile_shaders(Mush2D)
mush2d_copy_assets(Mush2D)
mush2d_copy_game_files(Mush2D)

enable_testing()
add_executable(mush2d_tests tests/tests.cpp ${MUSH2D_GAMES_LIST})
target_link_libraries(mush2d_tests PRIVATE ${MUSH2D_GAME_LIBS})
target_compile_definitions(mush2d_tests PRIVATE MUSH2D_TEST_DATA="${CMAKE_SOURCE_DIR}/tests/data" MUSH2D_ENGINE_SRC="${CMAKE_SOURCE_DIR}/src/engine") # engine_has_no_game_content scans the engine sources
if(MSVC)
    target_compile_options(mush2d_tests PRIVATE /bigobj) # one large test file
endif()
add_test(NAME mush2d_tests COMMAND mush2d_tests)
