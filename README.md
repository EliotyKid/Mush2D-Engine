# Mush2D-Engine

C++20 / Vulkan 2D engine. Needs Vulkan 1.3 (`vulkaninfo --summary`, `apiVersion` >= 1.3).

Para começar: `manual/docs/fazendo-o-jogo/criando-o-seu-jogo.md` (pastas e `--game`) e `projeto-em-dados.md` (o `project.json`, com as teclas e os botões).

Manual completo (controles, mundo e LDtk, prefabs, objetos, sistemas, variáveis de ambiente, desempenho): pasta `manual/`. Para abrir:

```
py -m pip install -r manual/requirements.txt
py -m mkdocs serve -f manual/mkdocs.yml
```

e depois `http://127.0.0.1:8000`.

## Pastas

- `src/engine/`: a engine (nunca cita um jogo).
- `src/app/main.cpp`: o programa `Mush2D`, que abre qualquer jogo compilado.
- `examples/iki_demo/`: o jogo de exemplo (o Iki na sala vitrine), com `project.json`, `assets/` e `src/iki_demo/`.
- `games/ikigai/`: o modelo do Ikigai (sala vazia e um personagem que anda e pula).

Cada jogo é ligado ao build por uma linha `mush2d_add_game(...)` no `CMakeLists.txt` (veja `cmake/Games.cmake`).

## Controles (vitrine)

| Teclado | Gamepad | Ação |
|---|---|---|
| `←` `→` | direcional ou analógico | Correr. |
| `Z` | `A` | Pular (a seta para cima não pula o Iki). |
| `↓` + `Z` | `↓` + `A` | Descer por uma plataforma atravessável. |
| `X` | `X` | Golpe de gelo. Até 3 seguidos (o terceiro é mais forte). |
| `C` | `Y` | Estilhaço de gelo (gasta gelo). |
| `V` (segurar, no chão) | `R1` (segurar) | Escudo: bloqueia golpes de frente (gasta gelo). Logo depois de erguer, apara (atordoa e devolve projéteis). |
| `←`/`→` contra uma parede de gelo (no ar) | direcional | Cravar as estacas e agarrar a parede (precisa da habilidade). Na parede, `↑`/`↓` sobe e desce; o pulo salta para longe. |
| `↑` sob um teto de gelo (no ar) | direcional `↑` | Pendurar no teto. Pendurado, `←`/`→` anda; `↓` ou o pulo solta. |
| `R` | | Morrer e voltar ao checkpoint. |
| `J` (só Debug) | | 1 de dano no Iki. |
| `L` (só Debug) | | Gasta 30 de gelo e 30 de fogo. |
| `F1` | | Painel e inspetor (mostra o estado do Iki). |

As teclas e os botões do jogo ficam em `examples/iki_demo/project.json` (`"actions"`).

A lista completa está em `manual/docs/primeiros-passos.md`.

## Mundo

`examples/iki_demo/assets/levels/showcase.ldtk` é um mundo contínuo: `Showcase` (início), `Forest2` e `Cave1` à direita, e `IceTest` à esquerda. Para chegar à `IceTest`, saia da vitrine pela porta na parede esquerda. Lá estão o item das estacas de gelo, uma parede de gelo de 8 u, um teto de gelo de 6 u sobre um vão e uma plataforma alta com checkpoint, que só se alcança com a habilidade. No chão, à direita do vão, ficam três bonecos de treino: um fraco a gelo (rosa), um resistente (ciano) e, perto da porta, um atirador (magenta) para treinar o escudo e o aparo. No LDtk, o gelo escalável é o valor `9` (`IceClimb`, azul-gelo) da camada `Collision`, ou o campo `iceClimb` de um `Terrain`. Detalhes em `manual/docs/fazendo-o-jogo/mundo-e-salas.md`.

## Sons

Os sons de `examples/iki_demo/assets/audio/sfx` e as músicas de `examples/iki_demo/assets/audio/music` (só `.wav`) são provisórios, gerados por um script com semente fixa: rodar de novo grava os mesmos bytes. Para gerar de novo, na raiz do projeto: `py tools/gen_placeholder_sfx.py` (Python 3, só a biblioteca padrão). Sem dispositivo de áudio, o jogo roda em silêncio. Detalhes em `manual/docs/sistemas/audio.md`.

## Prerequisites

- Arch Linux: `sudo pacman -S vulkan-devel cmake ninja gcc libxrandr libxinerama libxcursor libxi libxkbcommon wayland wayland-protocols shaderc`
- Windows: Vulkan SDK, Visual Studio 2022 or MSVC Build Tools, CMake, Ninja

Other dependencies (GLFW, glm, nlohmann_json, vk-bootstrap, VMA, Dear ImGui, miniaudio) are fetched by CMake on first configure.

## Build

```
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug
```

Use `release` instead of `debug` for an optimized build. The executable is `build/<preset>/Mush2D` (`Mush2D.exe` on Windows).

Run a game by id: `build/debug/Mush2D --game iki_demo` (the demo) or `--game ikigai` (the starter template in `games/ikigai/`: an empty room with a floor and a player that walks and jumps with the engine's `PlatformerMover`). Without `--game` it shows a launcher screen (one button per game, the last one marked, `Enter` opens it; the choice is saved in `mush2d_last_game.json` next to the exe). An unknown id prints the valid ids and shows the launcher.

Se o configure do Release travar baixando o imgui, passe `-DFETCHCONTENT_SOURCE_DIR_IMGUI=<caminho de um checkout do imgui>`; `build/debug/_deps/imgui-src` serve depois de um configure Debug.
