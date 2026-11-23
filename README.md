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

- Setas: mover. `Z` ou seta para cima: pular. `↓` + pulo sobre uma plataforma atravessável: descer por ela (em chão sólido é um pulo normal).
- `TAB`: próxima cena.
- `F5`: recarregar a cena atual. No Debug, salvar o `.ldtk` (ou um `.region.json`) recarrega a vitrine sozinho em até 0,5 s: o estado do mundo fica, o player fica onde estava (empurrado para fora do chão novo até 2 u; senão volta ao checkpoint). Um arquivo inválido mantém a cena e avisa no stderr.
- `Espaço`: dash. `H`: golpe. Só nas cenas Sandbox e Test, não na sala vitrine.
- `R`: voltar ao checkpoint.
- `K` (só Debug, na vitrine): morrer. A tela escurece, a sala da morte volta ao estado de quando o jogador entrou nela, e o jogador reaparece no último `Checkpoint` ativado (ou no `PlayerStart`).
- `↓` sobre uma alavanca: abre ou fecha a porta ligada a ela. Pular em cima de um slime o mata.
- `F`: liga/desliga a câmera que segue o player. Com ela desligada, `W` `A` `S` `D` movem a câmera. Botão do meio do mouse arrastando também move a câmera.
- `Q` `E` (e a roda do mouse): zoom da câmera. Funciona nos modos pixel art (suave) (limitado a 1x-8x) e HD. Sem efeito no modo pixel art (nítido), que usa escala inteira fixa.
- `F1`: painel de visual e desempenho (ImGui).
- `F1` também abre a janela "Cena (F1)": a árvore de objetos ao vivo (objeto, filhos, componentes). Os atributos editados valem na hora; alguns (atlas, frame, variante, vida máxima, política de persistência) só aparecem depois de salvar e recarregar. "Salvar no prefab" grava só os atributos editados no `.prefab.json` de origem; "Salvar como novo prefab" cria um prefab com `base` no original. Valores vindos do LDtk nunca vão para o prefab. Salvar ou editar um prefab recarrega os objetos dele, e edições não salvas se perdem.
- "Salvar" grava a região atual (`assets/regions/<nome>.region.json`: look e camadas de fundo). No Release, grava ao lado do executável, e a cópia de assets do próximo build sobrescreve esse arquivo. Para persistir, use o Debug (grava no código-fonte).

## Mundo

`assets/levels/showcase.ldtk` é um mundo contínuo: as salas são carregadas e descarregadas conforme o jogador anda, sem cortes.

- `Showcase`: a sala vitrine (região `forest`).
- `Forest2`: à direita da vitrine (região `forest`), pela passagem aberta na parede direita da vitrine. Tem um chão orgânico (`Terrain`) com dois morros, uma descida de ~30° e um galho atravessável sobre a descida.
- `Cave1`: acima e à direita de `Forest2` (região `cave`, mais escura). O chão dela continua a borda alta da direita de `Forest2`. Tem uma mesa com rampas de 45°, um morro com rampas de 22,5° e uma plataforma atravessável (com um slime em cima).

As duas salas de teste foram geradas uma vez por `tools/add_test_rooms.py`, e o terreno e as rampas por `tools/add_terrain_content.py`. Cada uma tem chão, um slime que volta (`respawn` ligado), um que não volta, um item `permanent`, um item `respawn`, uma alavanca que abre uma porta e um `Checkpoint`. Edite tudo livremente no LDtk.

### Colisão no LDtk

IntGrid `Collision` (células de 16 px):

| Valor | Significado |
|---|---|
| `1` | sólido |
| `2` | plataforma atravessável: só o topo colide, e só de cima |
| `3` / `4` | rampa de 45° subindo para a direita / esquerda |
| `5` / `6` | rampa de 22,5°, metade baixa (sobe de 0 a ½ célula), para a direita / esquerda |
| `7` / `8` | rampa de 22,5°, metade alta (sobe de ½ a 1 célula), para a direita / esquerda |

Uma rampa de 22,5° ocupa 2 células: `5` seguida de `7` (subindo para a direita) ou `8` seguida de `6` (descendo). Rampas vizinhas iguais viram um segmento só. Faces verticais na borda da sala não colidem: feche o mundo no level design.

Entidade `Terrain` (camada `Terrain_d0`, grade de 4 px), para chão livre em qualquer ângulo:

- a posição da entidade é o primeiro ponto; `points` (Points) são os seguintes, desenhados como caminho;
- `closed`: polígono fechado; senão é uma superfície preenchida para baixo até `thickness` (unidades, padrão 3; 32 px = 1 u);
- `oneWay`: atravessável (só os trechos virados para cima, como um galho);
- `style`: `forest` ou `cave` (`assets/terrain/<style>/`; um estilo sem texturas usa `forest` e avisa uma vez).

A engine gera a colisão e o visual (borda, lados e preenchimento) a partir da linha.

### Tiles de rampa (auto-rules)

`assets/kaa/tilesets/grass_slopes.png` (gerado por `tools/gen_terrain_textures.py`) é o `grass.png` com mais uma linha (linha 6) com os tiles de rampa dos valores `3` a `8`, nessa ordem. A camada `Ground_d0` de `Cave1` usa esse tileset (override da camada) e tem os tiles colocados à mão pelo script. Para pintar rampas sozinho no LDtk:

1. Camadas `Tiles` como `Ground_d0` não têm regras. Em *Project layers*, crie uma camada *Auto-layer* (por exemplo `Slopes_d0`, logo acima de `Ground_d0`), com tileset `GrassSlopes` e fonte (*Source IntGrid layer*) `Collision`.
2. Crie um grupo de regras. Para cada valor de `3` a `8`, uma regra 1×1: a célula central igual ao valor e, como tile, o da linha 6 na mesma ordem (valor `3` = primeira coluna).
3. Apague os tiles de rampa colocados à mão em `Ground_d0` de `Cave1`: agora a camada nova os pinta. Os tiles do `1` em volta continuam como estão (bordas de grama).
4. Para pintar também o sólido por regra, faça o mesmo com o `1` (9-slice do `grass.png`, nas mesmas posições do `GrassSlopes`).

`tools/add_terrain_content.py` escreve o `.ldtk` no mesmo layout do LDtk (`tools/ldtk_json.py`) e recusa rodar se o arquivo não fizer o caminho de ida e volta byte a byte.

## Prefabs

O jogo (`IkigaiGame`) cria os objetos a partir de prefabs: cada entidade do LDtk cria `assets/prefabs/<nome em snake_case>.prefab.json` (`Slime` → `slime.prefab.json`), ou o prefab do campo `prefab` da entidade. O prefab diz a classe (`"type"`), os atributos (`"props"`), o sprite, o collider, as animações, os componentes e os filhos, e pode herdar de outro com `"base"`. Um campo da entidade com o nome de um atributo sobrescreve o valor. Só `PlayerStart`, `Light` e `Terrain` têm fábrica em código.

Prefabs atuais: `slime`, `slime_fast` (herda do slime: mais rápido, avermelhado), `door`, `lever`, `item`, `checkpoint`, `critter`, `prop`, `crystal` e `crystal_halo` (filho aditivo do cristal). No Debug, `assets/prefabs/prefab_schema.json` lista as classes e os atributos, e salvar um prefab recarrega as salas (o estado do mundo fica).

## Variáveis de ambiente

- `MUSH2D_SCREENSHOT=<arquivo.png>`: salva a imagem do frame 60 e fecha.
- `MUSH2D_SHOW_PANEL=1`: abre com o painel visível.
- `MUSH2D_PERF_LOG=<segundos>`: imprime fps e ms médios por passada de GPU a cada segundo, e o tempo de CPU de cada carga de sala (`room <nome> load_ms=`), e fecha após N segundos (qualquer build).
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
