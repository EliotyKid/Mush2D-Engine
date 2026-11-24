# Mush2D-Engine

C++20 / Vulkan 2D engine. Needs Vulkan 1.3 (`vulkaninfo --summary`, `apiVersion` >= 1.3).

Manual completo (controles, mundo e LDtk, prefabs, objetos, sistemas, variáveis de ambiente, desempenho): pasta `manual/`. Para abrir:

```
py -m pip install -r manual/requirements.txt
py -m mkdocs serve -f manual/mkdocs.yml
```

e depois `http://127.0.0.1:8000`.

## Prerequisites

- Arch Linux: `sudo pacman -S vulkan-devel cmake ninja gcc libxrandr libxinerama libxcursor libxi libxkbcommon wayland wayland-protocols shaderc`
- Windows: Vulkan SDK, Visual Studio 2022 or MSVC Build Tools, CMake, Ninja

Other dependencies (GLFW, glm, nlohmann_json, vk-bootstrap, VMA, Dear ImGui) are fetched by CMake on first configure.

## Build

```
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug
```

Use `release` instead of `debug` for an optimized build. The executable is `build/<preset>/Mush2DEngine`.

Se o configure do Release travar baixando o imgui, passe `-DFETCHCONTENT_SOURCE_DIR_IMGUI=<caminho de um checkout do imgui>`; `build/debug/_deps/imgui-src` serve depois de um configure Debug.
