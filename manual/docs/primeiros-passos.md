# Primeiros passos

## Pré-requisitos

A engine precisa de Vulkan 1.3. Confira com `vulkaninfo --summary`: o `apiVersion` deve ser 1.3 ou maior.

=== "Windows"

    Vulkan SDK, Visual Studio 2022 ou MSVC Build Tools, CMake e Ninja.

=== "Arch Linux"

    ```bash
    sudo pacman -S vulkan-devel cmake ninja gcc libxrandr libxinerama libxcursor libxi libxkbcommon wayland wayland-protocols shaderc
    ```

As outras dependências (GLFW, glm, nlohmann_json, vk-bootstrap, VMA, Dear ImGui) são baixadas pelo CMake no primeiro configure.

## Compilar e rodar

```bash
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug
```

Use `release` no lugar de `debug` para um build otimizado. O executável fica em `build/<preset>/Mush2D` (`Mush2D.exe` no Windows).

### Escolher o jogo

O `Mush2D` traz todos os jogos compilados. Para abrir um jogo pelo id:

```bash
build/debug/Mush2D --game iki_demo
```

Sem o `--game`, ele mostra a tela de escolha: um botão por jogo, com o último jogo aberto marcado. `Enter` abre o marcado. A última escolha fica em `mush2d_last_game.json`, ao lado do executável. A tela também avisa, sem botão, das pastas com `project.json` que não têm código compilado.

Com `--game <id>` a tela é pulada. Com um id que não existe (ou sem o id), ele mostra os ids válidos e abre a tela de escolha.

Hoje há um jogo: o `iki_demo`, em `examples/iki_demo/` (o Iki e a sala vitrine). Cada jogo tem a sua pasta, com:

- `project.json`: o id e o nome do jogo;
- `assets/`: salas, sprites, prefabs e sons do jogo;
- `src/<id>/`: o código do jogo.

Os caminhos `assets/...` no código do jogo são relativos à pasta dele. O build copia a pasta para perto do executável (`build/debug/examples/iki_demo/`). Os shaders são da engine e ficam em `build/debug/shaders/`.

No Windows, o terminal precisa do ambiente do Visual Studio. Se o `cmake` não achar o compilador, carregue-o antes no PowerShell:

```powershell
. .superpowers/sdd/2026-11-18-sala-vitrine/devshell.ps1
```

!!! note "Sobre o `devshell.ps1`"
    Esse script fica numa pasta ignorada pelo git. Ele põe o Vulkan SDK e o Python no `Path` e abre o ambiente do Visual Studio (`Launch-VsDevShell.ps1`, x64).

!!! tip "Configure do Release travado no imgui"
    Se o configure do Release travar baixando o imgui, passe `-DFETCHCONTENT_SOURCE_DIR_IMGUI=<caminho de um checkout do imgui>`. Depois de um configure Debug, `build/debug/_deps/imgui-src` serve.

Se der erro de compilação, o compilador mostra `arquivo(linha): error ...`. A linha indicada é onde ele percebeu o problema, que às vezes vem de uma linha acima.

### Testes

Os testes ficam em `tests/tests.cpp`, com dados em `tests/data/` (salas `.ldtk`, atlas e prefabs de teste). Rode com:

```bash
ctest --test-dir build/debug
```

## Controles

| Tecla | Ação |
|---|---|
| Setas | Mover. |
| `Z` | Pular. Nas cenas Sandbox e Test, a seta para cima também pula; o Iki (vitrine) só pula com `Z`. |
| `↓` + pulo sobre plataforma atravessável | Descer por ela. Em chão sólido é um pulo normal. |
| `↓` sobre uma alavanca | Abre ou fecha a porta ligada a ela. |
| `←`/`→`, `↑`/`↓` numa parede ou teto de gelo (Iki, com a habilidade) | Agarrar, subir, descer, andar pendurado. Veja [Estacas de gelo](fazendo-o-jogo/iki.md#estacas-de-gelo). |
| `Espaço` | Dash. Só nas cenas Sandbox e Test, não na sala vitrine. |
| `H` | Golpe. Só nas cenas Sandbox e Test. |
| `R` | Voltar ao checkpoint. Na vitrine é uma morte (escurece e restaura a sala); nas cenas Sandbox e Test só teleporta. |
| `K` (só Debug, na vitrine) | Morrer. Veja [Mundo e salas](fazendo-o-jogo/mundo-e-salas.md#morte-e-respawn). |
| `J` (só Debug, na vitrine) | 1 de dano no Iki (estado `hurt`, pisca). Com a vida em 0, ele morre. |
| `L` (só Debug, na vitrine) | Gasta 30 de gelo e 30 de fogo (as barras do HUD recarregam 1 s depois). |
| `TAB` | Próxima cena. |
| `F5` | Recarregar a cena atual. |
| `F` | Liga/desliga a câmera que segue o player. Desligada, `W` `A` `S` `D` movem a câmera. |
| Botão do meio do mouse (arrastar) | Move a câmera. |
| `Q` `E` e roda do mouse | Zoom da câmera. |
| `F1` | Painel de visual e desempenho e janela "Cena (F1)". Veja [Painel F1 e inspetor](fazendo-o-jogo/inspetor.md). |

Pular em cima de um slime o mata. Na vitrine, o personagem é o Iki: veja [Iki: estados e habilidades](fazendo-o-jogo/iki.md).

O zoom funciona nos modos pixel art (suave), limitado a 1x a 8x, e HD. Não tem efeito no modo pixel art (nítido), que usa escala inteira fixa.

As teclas são ligadas em `src/engine/platform/GlfwInput.cpp`. Veja [platform](sistemas/platform.md) para trocar uma tecla.

### Hot reload

No Debug, salvar o `.ldtk` (ou um `.region.json`) recarrega a vitrine sozinho em até 0,5 s:

- o estado do mundo fica;
- o player fica onde estava, empurrado para fora do chão novo até 2 u; se não der, volta ao checkpoint;
- um arquivo inválido mantém a cena e avisa no stderr.

## Variáveis de ambiente

| Variável | Efeito |
|---|---|
| `MUSH2D_SCREENSHOT=<arquivo.png>` | Salva a imagem do frame 60 e fecha. |
| `MUSH2D_SHOW_PANEL=1` | Abre com o painel F1 visível. |
| `MUSH2D_PERF_LOG=<segundos>` | Imprime fps e ms médios por passada de GPU a cada segundo, e o tempo de CPU de cada carga de sala (`room <nome> load_ms=`). Fecha após N segundos. Funciona em qualquer build. |
| `MUSH2D_DEBUG_ADDITIVE=1` | Sprite aditivo brilhante ao lado do player, para testar aditivo e bloom. |
| `MUSH2D_CAMERA_X=<x do mundo>` | Fixa a câmera nessa posição (capturas de parallax). |

### Casos de uso

Capturar uma imagem da vitrine para comparar antes e depois de mudar o look:

```powershell
$env:MUSH2D_SCREENSHOT = "antes.png"; build/debug/Mush2D.exe; Remove-Item Env:MUSH2D_SCREENSHOT
```

Comparar o parallax de duas versões no mesmo ponto do mundo: rode as duas com o mesmo `MUSH2D_CAMERA_X` e `MUSH2D_SCREENSHOT`.

Medir o desempenho por 30 segundos: veja [Desempenho](desempenho.md).
