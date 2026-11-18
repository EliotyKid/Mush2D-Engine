# Mush2D-Engine

C++20 / Vulkan 2D engine. Needs Vulkan 1.3 (`vulkaninfo --summary`, `apiVersion` >= 1.3).

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

## Controles

- Setas: mover. `Z` ou seta para cima: pular.
- `TAB`: próxima cena.
- `F5`: recarregar a cena atual.
- `Espaço`: dash. `H`: golpe. Só nas cenas Sandbox e Test, não na sala vitrine.
- `R`: voltar ao checkpoint.
- `F`: liga/desliga a câmera que segue o player. Com ela desligada, `W` `A` `S` `D` movem a câmera. Botão do meio do mouse arrastando também move a câmera.
- `Q` `E` (e a roda do mouse): zoom da câmera. Sem efeito no modo pixel art, que usa escala inteira fixa.
- `F1`: painel de visual e desempenho (ImGui).
- No Release, "Salvar" grava `showcase.look.json` ao lado do executável, e a cópia de assets do próximo build sobrescreve esse arquivo. Para persistir, use o Debug (grava no código-fonte).

## Variáveis de ambiente

- `MUSH2D_SCREENSHOT=<arquivo.png>`: salva a imagem do frame 60 e fecha.
- `MUSH2D_SHOW_PANEL=1`: abre com o painel visível.
- `MUSH2D_PERF_LOG=<segundos>`: imprime fps e ms médios por passada de GPU a cada segundo, e fecha após N segundos (qualquer build).
- `MUSH2D_DEBUG_ADDITIVE=1`: sprite aditivo brilhante ao lado do player, para testar aditivo e bloom.
- `MUSH2D_CAMERA_X=<x do mundo>`: fixa a câmera nessa posição (capturas de parallax).

## Desempenho

Release, `MUSH2D_PERF_LOG`, 30 s parado na sala vitrine (média por passada, ms de GPU; vsync liga o fps ao monitor).

| Máquina | gbuffer | light | resolve | additive | bloom | composite | soma | frame | fps |
|---|---|---|---|---|---|---|---|---|---|
| Intel UHD, 1280×720, luz em resolução cheia | 3,16 | 1,66 | 1,30 | 0,09 | 0,56 | 0,41 | 7,19 | 16,67 ms | 60 |
| Intel UHD, 1280×720, luz em meia resolução (`lightHalfRes`) | 3,49 | 0,60 | 0,89 | 0,11 | 0,65 | 0,48 | 6,23 | 16,67 ms | 60 |
| RTX 2060 Super (Arch), 1920×1080 | | | | | | | a medir pelo autor | | |

Orçamento de 60 fps: 16,6 ms. Nos primeiros segundos o fps oscila (~49-59) enquanto a GPU integrada sobe o clock; depois fica em 60. Meta no PC Arch: soma < 3 ms.
