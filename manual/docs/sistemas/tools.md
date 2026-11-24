# tools

`src/engine/tools/` são as ferramentas dentro do jogo, no painel F1 (Dear ImGui).

## O que faz

- **`LookPanel`** desenha o painel de visual e desempenho: ambiente, névoa, bloom, grading, pixel art, fundo da região, camadas, luzes e tempos de GPU.
- **`LookFile`** lê e grava o look (`FrameSettings`) em JSON, a parte `"look"` do `.region.json`.
- **`SceneInspector`** desenha a janela "Cena (F1)": a árvore de objetos ao vivo, os atributos exportados de cada objeto e componente, e os botões de salvar prefab.

O uso está em [Painel F1 e inspetor](../fazendo-o-jogo/inspetor.md).

## Tipos principais

| Tipo | Papel |
|---|---|
| `LookPanel` | Os controles do look e o botão "Salvar" da região. |
| `LookFile` | JSON ↔ `FrameSettings` (campos como `lightHalfRes`, `fogDensity`, `exposure`). |
| `SceneInspector` | Árvore, filtro por nome ou tag, "pausar o jogo", edição de atributos, "Salvar no prefab", "Salvar como novo prefab". Usa `PrefabSave` (veja [world](world.md)). |

O inspetor mostra um widget por tipo de atributo (`Float`, `Int`, `Bool`, `String`, `Vec2`, `Color`, `Enum`, referências). Um tipo novo de atributo precisa de um widget novo aqui (veja [reflect](reflect.md)).

## Casos de uso

- **Achar um objeto pela tag:** digite a tag (por exemplo `enemy`) no filtro da janela "Cena (F1)".
- **Um controle novo no look:** adicione o campo em `FrameSettings` (`SceneRenderer.hpp`), a leitura e gravação em `LookFile.cpp` e o controle em `LookPanel.cpp`.

## Arquivos

`src/engine/tools/LookPanel.*`, `LookFile.*`, `SceneInspector.*`.
