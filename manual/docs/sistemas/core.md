# core

`src/engine/core/` é o coração: o contrato do jogo, o loop principal, os caminhos de arquivo e o vigia de hot reload.

## O que faz

`src/app/main.cpp` lê o `--game <id>`, acha o jogo na tabela dos jogos compilados (`compiledGames()`, em `GameRegistry.hpp`) e chama `engine.run(jogo)`. Sem o `--game`, ou com um id que não existe, abre o primeiro da tabela. Se algo der errado, imprime o erro.

A tabela é gerada pelo CMake: cada `mush2d_add_game(<id> <pasta> <Classe> <header>)` no `CMakeLists.txt` vira uma linha dela (veja `cmake/Games.cmake`). O `Engine::run` aponta os caminhos para a pasta do jogo (`setGameRoot`), cria o jogo e roda.

A `Engine` abre a janela, inicializa GPU, renderer, materiais, input e câmera, roda o loop (input → `game.update` → câmera → desenho) e fecha tudo na ordem certa. Ela também cuida do painel F1 (ImGui), da captura (`MUSH2D_SCREENSHOT`), do log de desempenho (`MUSH2D_PERF_LOG`) e de `MUSH2D_SHOW_PANEL`.

## Tipos principais

| Tipo | Papel |
|---|---|
| `IGame` | O contrato de todo jogo: `init(EngineServices&)`, `update(EngineServices&, float dt)`, `onHud(EngineServices&)` (todo quadro com o painel F1 fechado, dentro de um quadro do ImGui, para um HUD; opcional), `onImGui(EngineServices&)` (só com o painel F1 aberto, opcional) e `shutdown(EngineServices&)` (opcional). |
| `EngineServices` | A caixa de ferramentas que a engine entrega ao jogo: `gpu`, `renderer`, `materials`, `input`, `camera`, `scene` e `audio`. |
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

E no `CMakeLists.txt`, ligue o jogo com `mush2d_add_game(meu_jogo games/meu_jogo MeuJogo meu_jogo/MeuJogo.hpp)`; depois abra com `--game meu_jogo`. Veja o `IkiDemoGame` para tudo o que um jogo completo faz por frame ([Ciclo do jogo](../conceitos/ciclo-do-jogo.md)).

### Ler um arquivo de asset

`assetPath("assets/levels/showcase.ldtk")` para ler. O caminho é relativo à pasta do jogo que está rodando (`gameRoot()`, por exemplo `examples/iki_demo`). Os arquivos da própria engine (os shaders) usam `enginePath`. Para salvar algo que deve ir para o git, use `sourceAssetPath` (e rode em Debug).

### Hot reload de um arquivo novo

Crie um `FileWatcher`, chame `watch(arquivo, pasta, ".sufixo")` no `init` e `poll(dt)` no `update`; recarregue quando `file` ou `dir` vier `true`. Depois de salvar o arquivo pelo jogo, chame `sync()` para não recarregar a própria gravação.

## O jogo: `examples/iki_demo/src/iki_demo/`

O jogo atual fica fora da engine, em `examples/iki_demo/src/iki_demo/`. É ali que você trabalha.

| Arquivo | Papel |
|---|---|
| `IkiDemoGame.hpp/.cpp` | O jogo. Carrega os assets; registra os serviços da cena (`WorldState`, `World`, `GameAssets`, `AssetRegistry`, prefabs) e as fábricas das entidades especiais (`PlayerStart`, `Light`); trata `TAB`, `F5`, `K`, `R` e, no Debug, `J` e `L`; ouve `Died` para a morte; desenha o HUD de teste (`onHud`); liga o mundo e as regiões; verifica os triggers do player. |
| `objects/*.hpp` | Os objetos (veja [Objetos do jogo](../objetos.md)). Toda classe daqui é registrada só por existir. |
| `assets/GameAssets.*`, `Game*Id.hpp` | Os ids de atlas, sprites e animações usados pelo player e pelo código antigo. |
| `scenes/*` | As cenas antigas montadas em código (`SandboxScene`, `TestScene`) e o registro de cenas que o `TAB` percorre. |
| `spawners/Spawn.*` | Funções que criam objetos já configurados (`Spawn::player`, `Spawn::slime`). |

## Pastas de dados e scripts

| Pasta | Conteúdo |
|---|---|
| `assets/atlases/`, `assets/kaa/` | Atlas de sprites (`.png` + `.json`, com normal maps). `assets/kaa/` tem a arte do kaa e os tilesets. |
| `assets/levels/showcase.ldtk` | As salas. |
| `assets/prefabs/` | Os prefabs e o `prefab_schema.json` (gerado no Debug). |
| `assets/regions/` | O look e o fundo de cada região. |
| `assets/terrain/` | Texturas do terreno livre, por estilo. |
| `assets/props/` | Atlas gerados, como o dos cristais (`crystals.png` com `_e` e `_n`). |
| `tools/*.py` | Scripts que geraram arte e conteúdo: `import_kaa.py` importa sprites do GameMaker; `gen_showcase.py` gera a arte que faltava para a vitrine (os cristais); `add_test_rooms.py` e `add_terrain_content.py` escrevem no `.ldtk` (via `ldtk_json.py`); `gen_terrain_textures.py` gera texturas de terreno e o tileset de rampas; `migrate_showcase_parallax.py` (uso único) passou as camadas de parallax da vitrine para `assets/regions/forest.region.json`. |

## Arquivos

`src/app/main.cpp`, `cmake/Games.cmake`, `src/engine/core/IGame.hpp`, `Engine.hpp/.cpp`, `GameRegistry.hpp/.cpp`, `Paths.hpp/.cpp`, `FileWatcher.hpp/.cpp`.
