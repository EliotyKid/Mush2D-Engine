# Mush2D-Engine

C++20 / Vulkan 2D engine. Needs Vulkan 1.3 (`vulkaninfo --summary`, `apiVersion` >= 1.3).

Manual completo (controles, mundo e LDtk, prefabs, objetos, sistemas, variáveis de ambiente, desempenho): pasta `manual/`. Para abrir:

```
py -m pip install -r manual/requirements.txt
py -m mkdocs serve -f manual/mkdocs.yml
```

e depois `http://127.0.0.1:8000`.

## Controles (vitrine)

| Tecla | Ação |
|---|---|
| `←` `→` | Correr. |
| `Z` | Pular (só `Z`: a seta para cima não pula o Iki). |
| `↓` + `Z` | Descer por uma plataforma atravessável. |
| `X` | Golpe de gelo. Até 3 seguidos (o terceiro é mais forte). |
| `C` | Estilhaço de gelo (gasta gelo). |
| `V` (segurar, no chão) | Escudo: bloqueia golpes de frente (gasta gelo). Logo depois de erguer, apara (atordoa e devolve projéteis). |
| `←`/`→` contra uma parede de gelo (no ar) | Cravar as estacas e agarrar a parede (precisa da habilidade). Na parede, `↑`/`↓` sobe e desce; `Z` salta para longe. |
| `↑` sob um teto de gelo (no ar) | Pendurar no teto. Pendurado, `←`/`→` anda; `↓` ou `Z` solta. |
| `J` (só Debug) | 1 de dano no Iki. |
| `L` (só Debug) | Gasta 30 de gelo e 30 de fogo. |
| `F1` | Painel e inspetor (mostra o estado do Iki). |

A lista completa está em `manual/docs/primeiros-passos.md`.

## Mundo

`assets/levels/showcase.ldtk` é um mundo contínuo: `Showcase` (início), `Forest2` e `Cave1` à direita, e `IceTest` à esquerda. Para chegar à `IceTest`, saia da vitrine pela porta na parede esquerda. Lá estão o item das estacas de gelo, uma parede de gelo de 8 u, um teto de gelo de 6 u sobre um vão e uma plataforma alta com checkpoint, que só se alcança com a habilidade. No LDtk, o gelo escalável é o valor `9` (`IceClimb`, azul-gelo) da camada `Collision`, ou o campo `iceClimb` de um `Terrain`. Detalhes em `manual/docs/fazendo-o-jogo/mundo-e-salas.md`.

## Sons

Os sons de `assets/audio/sfx` e as músicas de `assets/audio/music` (só `.wav`) são provisórios, gerados por um script com semente fixa: rodar de novo grava os mesmos bytes. Para gerar de novo, na raiz do projeto: `py tools/gen_placeholder_sfx.py` (Python 3, só a biblioteca padrão). Sem dispositivo de áudio, o jogo roda em silêncio. Detalhes em `manual/docs/sistemas/audio.md`.

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

Use `release` instead of `debug` for an optimized build. The executable is `build/<preset>/Mush2DEngine`.

Se o configure do Release travar baixando o imgui, passe `-DFETCHCONTENT_SOURCE_DIR_IMGUI=<caminho de um checkout do imgui>`; `build/debug/_deps/imgui-src` serve depois de um configure Debug.
