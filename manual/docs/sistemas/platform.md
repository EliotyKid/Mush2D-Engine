# platform

`src/engine/platform/` liga o teclado, o mouse e o gamepad (GLFW) ao que o jogo entende.

## O que faz

Há duas partes:

- **Ações com nome**, do jogo. O `project.json` declara cada ação (`"jump"`, `"attack"`...) com as teclas e os controles do gamepad que a disparam. O jogo lê as ações pelo nome; a engine nunca conhece os nomes.
- **Controles fixos de ferramenta**, da engine: câmera livre, zoom, seguir o player, arrastar com o mouse e a roda. São iguais em todo jogo e ficam no `InputState`.

A cada frame, o `GlfwInput::poll` lê os aparelhos num `RawInput` e o `ActionMap` calcula o `ActionFrame` do frame. Cada ação tem três estados, como no GameMaker:

| Campo | GameMaker | Significado |
|---|---|---|
| `down("jump")` | `keyboard_check` | está segurando |
| `pressed("jump")` | `keyboard_check_pressed` | apertou neste frame |
| `released("jump")` | `keyboard_check_released` | soltou neste frame |

Teclado e gamepad na mesma ação valem juntos: um só `pressed`, e o `released` só quando os dois soltam. Um nome que o projeto não declara dá uma ação parada e um aviso (uma vez por nome).

Quando o painel F1 (ImGui) está com o foco do teclado ou do mouse, o jogo vê as teclas como soltas (`setBlocked`).

## Tipos principais

| Tipo | Papel |
|---|---|
| `ActionMap` | As ações do projeto. A engine chama `configure` ao abrir o jogo e depois de um hot reload do `project.json`, e `update` todo frame. Fica registrado na cena: `scene.service<ActionMap>()`. `bufferWindow("jump")` dá o `buffer` da ação, em segundos. |
| `ActionFrame` | As ações de um frame: `get`, `down`, `pressed`, `released`, `stick()` (o analógico esquerdo depois da zona morta) e `lastDevice()`. `keepEdges(anterior)` guarda os apertos de um frame congelado pelo hit stop. `set(nome, estado)` monta um frame à mão (testes). |
| `InputBuffer` (`src/engine/world/InputBuffer.hpp`) | Guarda um aperto por `window` segundos: `step(apertou, dt)` a cada passo da cena, `active()`, `consume()`. Conta no tempo do passo, então não anda durante o hit stop. O `JumpAssist` usa um para o buffer de pulo. |
| `InputState` | Os controles fixos de ferramenta: `cameraLeft/Right/Up/Down`, `zoomIn`, `zoomOut`, `toggleFollow`, `dragCamera`, o mouse e a roda. Cada um é um `ButtonState` (`down`, `pressed`, `released`). |
| `GlfwInput` | `actions()` devolve o `ActionMap`; `getState()` devolve o `InputState`; `isKeyDown(GLFW_KEY_...)` lê uma tecla direto (o jogo usa para `TAB`, `F5` e as teclas de debug); `setBlocked(teclado, mouse)`. |

O player recebe o frame em `IPlayer::applyInput(const ActionFrame&)`, antes do passo da cena.

## Controles fixos da engine

| Controle | Tecla |
|---|---|
| `cameraLeft` / `Right` / `Up` / `Down` | `A` / `D` / `W` / `S` |
| `zoomOut` / `zoomIn` | `Q` / `E` (e a roda do mouse) |
| `toggleFollow` | `F` |
| `dragCamera` | botão do meio do mouse |

## Ações do iki_demo

Declaradas em `examples/iki_demo/project.json`:

| Ação | Teclado | Gamepad |
|---|---|---|
| `moveLeft` / `moveRight` / `moveUp` / `moveDown` | setas | direcional ou analógico esquerdo |
| `jump` (buffer 0,12 s) | `Z` | `A` |
| `attack` | `X` | `X` |
| `shoot` | `C` | `Y` |
| `guard` | `V` | `R1` |
| `dash` | `Espaço` | `B` |
| `interact` (alavancas) | `↓` | direcional ou analógico para baixo |
| `dropThrough` (descer da plataforma) | `↓` | direcional ou analógico para baixo |
| `respawn` | `R` | |
| `hit` (só Debug, Sandbox e Test) | `H` | |

Os nomes do gamepad seguem o mapeamento padrão do GLFW (o layout do Xbox: `A` embaixo, `B` à direita, `X` à esquerda, `Y` em cima).

## Casos de uso

### Trocar a tecla de pulo para `X`

No `project.json` do jogo, em `"jump"`, troque `"keys": ["Z"]` por `"keys": ["X"]`. Com o jogo rodando em Debug, salvar o arquivo já vale.

### Uma ação nova (por exemplo, `"map"` na tecla `M` e no `Select`)

1. No `project.json`, em `"actions"`: `"map": { "keys": ["M"], "gamepad": ["Back"] }`.
2. No código do player, em `applyInput`: `if (input.pressed("map")) { ... }`.

### Um buffer de toque para uma ação

Dê um `"buffer"` à ação no `project.json` e guarde um `InputBuffer` no objeto. A cada passo: `buffer.window = actions->bufferWindow("nome")` (o `actions` vem de `scene.service<ActionMap>()`), depois `buffer.step(input.pressed("nome"), dt)`. Use com `active()` e `consume()`.

### Uma tecla de depuração só no jogo

No `update` do jogo: `if (s.input.isKeyDown(GLFW_KEY_F6)) { ... }`. Guarde o estado do frame anterior se quiser só o instante em que apertou.

## Arquivos

`src/engine/platform/Actions.hpp/.cpp`, `InputState.hpp`, `GlfwInput.hpp/.cpp`, `src/engine/world/InputBuffer.hpp`.
