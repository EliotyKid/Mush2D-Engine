# core

`src/engine/core/` é o coração: o contrato do jogo, o loop principal, os caminhos de arquivo e o vigia de hot reload.

## O que faz

`src/main.cpp` cria o jogo (`IkigaiGame`) e a `Engine`, e chama `engine.run(game)`. Se algo der errado, imprime o erro. Para trocar de jogo, troque o jogo criado ali.

A `Engine` abre a janela, inicializa GPU, renderer, materiais, input e câmera, roda o loop (input → `game.update` → câmera → desenho) e fecha tudo na ordem certa. Ela também cuida do painel F1 (ImGui), da captura (`MUSH2D_SCREENSHOT`), do log de desempenho (`MUSH2D_PERF_LOG`) e de `MUSH2D_SHOW_PANEL`.

## Tipos principais

| Tipo | Papel |
|---|---|
| `IGame` | O contrato de todo jogo: `init(EngineServices&)`, `update(EngineServices&, float dt)`, `onImGui(EngineServices&)` (painel F1, opcional) e `shutdown(EngineServices&)` (opcional). |
| `EngineServices` | A caixa de ferramentas que a engine entrega ao jogo: `gpu`, `renderer`, `materials`, `input`, `camera` e `scene`. |
| `Engine` | `run(IGame&)`: abre tudo, roda o loop até a janela fechar e desliga. |
| `assetPath(rel)` | `<pasta do executável>/rel`. Onde o jogo lê assets copiados pelo build. |
| `sourceAssetPath(rel)` | No Debug, o arquivo dentro do projeto (`MUSH2D_SOURCE_DIR/rel`), para editar e salvar no git. No Release, igual a `assetPath`. |
| `FileWatcher` | Vigia um arquivo e o arquivo mais novo com um sufixo numa pasta (o `.ldtk` e os `.region.json`). `watch(...)`, `poll(dt)` devolve `Change{file, dir}`, `sync()` aceita o estado atual depois de o próprio jogo salvar. |

## Casos de uso

### Um jogo novo

```cpp
class MeuJogo : public IGame {
public:
    void init(EngineServices& s) override { /* carrega assets, cria objetos em s.scene */ }
    void update(EngineServices& s, float dt) override { s.scene.step(dt); s.scene.lateStep(dt); }
};
```

E em `main.cpp`, crie `MeuJogo` no lugar de `IkigaiGame`. Veja o `IkigaiGame` para tudo o que um jogo completo faz por frame ([Ciclo do jogo](../conceitos/ciclo-do-jogo.md)).

### Ler um arquivo de asset

`assetPath("assets/levels/showcase.ldtk")` para ler. Para salvar algo que deve ir para o git, use `sourceAssetPath` (e rode em Debug).

### Hot reload de um arquivo novo

Crie um `FileWatcher`, chame `watch(arquivo, pasta, ".sufixo")` no `init` e `poll(dt)` no `update`; recarregue quando `file` ou `dir` vier `true`. Depois de salvar o arquivo pelo jogo, chame `sync()` para não recarregar a própria gravação.

## Arquivos

`src/main.cpp`, `src/engine/core/IGame.hpp`, `Engine.hpp/.cpp`, `Paths.hpp/.cpp`, `FileWatcher.hpp/.cpp`.
