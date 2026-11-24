# platform

`src/engine/platform/` liga o teclado e o mouse (GLFW) aos "botões" que o jogo entende.

## O que faz

A cada frame, o `GlfwInput` lê as teclas físicas e atualiza o `InputState`. Cada botão tem três estados, como no GameMaker:

| Campo | GameMaker | Significado |
|---|---|---|
| `down` | `keyboard_check` | está segurando |
| `pressed` | `keyboard_check_pressed` | apertou neste frame |
| `released` | `keyboard_check_released` | soltou neste frame |

Quando o painel F1 (ImGui) está com o foco do teclado ou do mouse, o jogo vê os botões como soltos (`setBlocked`).

## Tipos principais

| Tipo | Papel |
|---|---|
| `ButtonState` | `down`, `pressed`, `released`. |
| `InputState` | Os botões: `moveLeft`, `moveRight`, `moveUp`, `moveDown`, `cameraLeft`, `cameraRight`, `cameraUp`, `cameraDown`, `zoomIn`, `zoomOut`, `jump`, `dash`, `hit`, `toggleFollow`, `respawn`, `dragCamera`. |
| `GlfwInput` | `getState()` devolve o `InputState`; `isKeyDown(GLFW_KEY_...)` lê uma tecla direto (o jogo usa para `TAB`, `F5` e `K`); `setBlocked(teclado, mouse)`. |

## Mapa das teclas

| Botão | Tecla |
|---|---|
| `moveLeft` / `moveRight` / `moveUp` / `moveDown` | setas |
| `jump` | `Z` (no modo plataforma, a seta para cima também pula) |
| `dash` | `Espaço` |
| `hit` | `H` |
| `cameraLeft` / `Right` / `Up` / `Down` | `A` / `D` / `W` / `S` |
| `zoomOut` / `zoomIn` | `Q` / `E` |
| `toggleFollow` | `F` |
| `respawn` | `R` |
| `dragCamera` | botão do meio do mouse |

O player usa `moveDown.pressed` como "interagir" (alavancas).

## Casos de uso

### Trocar a tecla de pulo para `X`

Em `GlfwInput.cpp`, na linha do `state.jump`, troque `GLFW_KEY_Z` por `GLFW_KEY_X`.

### Um botão novo (por exemplo, `interact` na tecla `C`)

1. Em `InputState.hpp`, adicione `ButtonState interact;`.
2. Em `GlfwInput.hpp`, adicione `bool previousInteract = false;`.
3. Em `GlfwInput.cpp`, ao lado dos outros: `updateButton(state.interact, !keyboardBlocked && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS, previousInteract);`.
4. Objetos não recebem o input: só o player, em `PlayerObject::applyInput(const InputState&)`. Leia `input.interact.pressed` ali e exponha para os outros objetos como o player já faz com `interactPressed()`.

### Uma tecla de depuração só no jogo

No `update` do jogo: `if (s.input.isKeyDown(GLFW_KEY_F6)) { ... }`. Guarde o estado do frame anterior se quiser só o instante em que apertou.

## Arquivos

`src/engine/platform/InputState.hpp`, `GlfwInput.hpp/.cpp`.
