# tools

`src/engine/tools/` são as ferramentas dentro do jogo, no painel F1 (Dear ImGui).

## O que faz

- **`LookPanel`** desenha o painel de visual e desempenho: ambiente, névoa, bloom, grading, pixel art, fundo da região, camadas, luzes, os números de juice da câmera e do áudio (volumes e transição da música) e tempos de GPU.
- **`LookFile`** lê e grava o look (`FrameSettings`) em JSON, a parte `"look"` do `.region.json`.
- **`SceneInspector`** desenha a janela "Cena (F1)": a árvore de objetos ao vivo, os atributos exportados de cada objeto e componente, e os botões de salvar prefab.
- **`drawInputPanel`** (`InputPanel.hpp`) desenha a janela "Projeto (F1)": os limites do gamepad e o `buffer` de cada ação, ao vivo.

A engine desenha "Cena (F1)" e "Projeto (F1)" para todo jogo, com o painel F1 aberto, antes do `onImGui` do jogo. O jogo recebe o inspetor em `EngineServices::inspector` (`selected()`, `pausing()`). O `LookPanel` é usado pelo jogo, no `onImGui` dele.

O uso está em [Painel F1 e inspetor](../fazendo-o-jogo/inspetor.md).

## Tipos principais

| Tipo | Papel |
|---|---|
| `LookPanel` | Os controles do look e o botão "Salvar" da região. |
| `LookFile` | JSON ↔ `FrameSettings` (campos como `lightHalfRes`, `fogDensity`, `exposure`). |
| `SceneInspector` | Árvore, filtro por nome ou tag, "pausar o jogo" (`pausing()`: a caixa marcada e o painel aberto), edição de atributos, "Salvar no prefab", "Salvar como novo prefab" (só com o serviço `PrefabLibrary` na cena). Usa `PrefabSave` (veja [world](world.md)). |
| `drawInputPanel` | "Projeto · Entrada": `triggerThreshold`, `stickDeadzone` e o `buffer` de cada ação do `ActionMap`. Não salva no `project.json`. |

O inspetor mostra um widget por tipo de atributo (`Float`, `Int`, `Bool`, `String`, `Vec2`, `Color`, `Enum`, referências). Um tipo novo de atributo precisa de um widget novo aqui (veja [reflect](reflect.md)).

## Casos de uso

- **Achar um objeto pela tag:** digite a tag (por exemplo `enemy`) no filtro da janela "Cena (F1)".
- **Um controle novo no look:** adicione o campo em `FrameSettings` (`SceneRenderer.hpp`), a leitura e gravação em `LookFile.cpp` e o controle em `LookPanel.cpp`.

## Arquivos

`src/engine/tools/LookPanel.*`, `LookFile.*`, `SceneInspector.*`, `InputPanel.*`.
