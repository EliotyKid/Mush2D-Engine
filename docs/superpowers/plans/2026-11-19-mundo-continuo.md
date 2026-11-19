# Mundo Contínuo — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Fazer da sala única um mundo contínuo de várias salas do LDtk, com streaming de salas, regiões (fundo e look), estado persistente por `iid` e morte que restaura a sala.

**Architecture:** Uma `Scene2D` só. Os objetos levam o `roomIid` da sala de onde vieram. A classe `World` (engine) parseia o projeto inteiro, calcula as salas vizinhas e carrega ou descarrega salas por meio do `LevelLoader`. O `WorldState` guarda o estado por `iid` e é consultado pelas fábricas. As regiões são arquivos JSON com o look e as camadas de parallax repetidas, ancoradas no mundo.

**Tech Stack:** C++20, nlohmann_json, glm, os testes existentes (`TEST`/`CHECK`) e Python + Pillow, só para o script que acrescenta as salas de teste.

**Spec:** [docs/superpowers/specs/2026-11-19-mundo-continuo-design.md](../specs/2026-11-19-mundo-continuo-design.md). As fórmulas, os valores e os nomes de testes estão lá. A seção "Decisões do Claude" faz parte do spec.

## Global Constraints

- `src/engine/**` nunca inclui `src/game/**`.
- Unidade de mundo: 32 px. Conversão `world = ((worldX + px.x)/32, -(worldY + px.y)/32)`.
- Histerese: uma sala só descarrega quando não é vizinha da sala atual **e** o jogador está a mais de 4 unidades da borda dela. A tolerância de vizinhança é de 1 px.
- O padrão de `respawnDistance` é 12. A verificação de respawn roda a cada 0,5 s. Interpolação de look entre regiões: 1,5 s. Fade da morte: 0,2 s para escurecer e 0,2 s para clarear.
- A sala inteira é criada num frame (`// shortcut:` sem fatiamento). Meta: menos de 2 ms na Intel UHD.
- As edições do autor em `assets/levels/showcase.ldtk` são preservadas. Os scripts só **acrescentam** (níveis, defs, campos) ou removem as camadas de parallax do nível `Showcase`, que migram para a região `forest`.
- A aparência da vitrine não muda. Zero erros de validação, que saem em **stdout**. Build sem warnings.
- Os commits terminam com uma linha em branco e `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Ambiente: `. .superpowers/sdd/2026-11-19-mundo-continuo/devshell.ps1`, depois `cmake --preset debug; cmake --build build/debug; ctest --test-dir build/debug`. Captura de tela com `MUSH2D_SCREENSHOT=<png>` e câmera fixa com `MUSH2D_CAMERA_X=<x>`.

## Review Focus

1. **Jogador parado na fronteira entre duas salas por muitos frames:** nenhuma sala carrega e descarrega em loop. Teste `streaming_no_thrash_at_border` (Task 2).
2. **Teleporte ou morte para uma sala distante, fora do conjunto carregado:** a sala de destino e as vizinhas dela estão carregadas antes do primeiro frame visível, e o jogador não cai no vazio. Teste `teleport_streams_target_synchronously` (Task 5).
3. **Sala sem o campo `region`, ou região com o arquivo `.region.json` faltando:** usa `forest` ou o look padrão, com aviso, sem crash. Teste `missing_region_falls_back` (Task 3).
4. **Inimigo `respawn` morto com o `spawnPos` na tela:** só volta depois que o ponto sai da tela. Teste `respawn_waits_until_offscreen` (Task 4).
5. **F5 (recarregar) com estado no mundo:** o F5 recarrega o `.ldtk` do disco, mas o `WorldState` continua. Portas abertas continuam abertas e mortos permanentes continuam mortos. Teste `reload_keeps_world_state` (Task 4).

---

### Task 1: Projeto LDtk multi-sala e coordenadas de mundo

**Files:** Modify `src/engine/world/Ldtk.hpp/.cpp`. Test: `tests/tests.cpp`, `tests/data/world3.ldtk` (novo, escrito à mão: 3 salas, uma delas deslocada e uma sem `region`).

**Interfaces (produz):**
- `LdtkLevel` ganha os campos `std::string iid; glm::ivec2 worldPx{0,0}; std::string region;`. Nível sem `region` recebe `"forest"`.
- `struct LdtkProject { std::filesystem::path baseDir; std::vector<LdtkLevel> levels; };`
- `LdtkProject loadLdtkProject(const std::filesystem::path&)`. Lança `std::runtime_error` em arquivo inválido. `loadLdtkLevel` continua existindo e passa a usar o projeto por dentro.
- `glm::vec2 ldtkToWorld(glm::ivec2 levelWorldPx, glm::vec2 px)`, que aplica a fórmula do spec.
- `std::vector<std::vector<int>> computeNeighbours(const std::vector<LdtkLevel>&, int tolerancePx = 1)`. Os índices de cada lista ficam em ordem crescente.

Passos:
- [ ] Escrever os testes `ldtk_world_coordinates`, `ldtk_project_reads_all_levels` (3 níveis, `iid`, `region`, com o padrão `forest`) e `world_neighbours_by_touching_rects` (casos: encostadas, sobrepostas e com 2 px de distância).
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN. A vitrine continua igual na captura.
- [ ] Commit `world: multi-level LDtk project, world coordinates, room adjacency`.

### Task 2: `World`, salas carregadas e streaming

**Files:**
- Create `src/engine/world/World.hpp/.cpp`.
- Modify `GameObject.hpp` (campos `std::string roomIid; bool persistent = false;`), `LevelLoader.*` (sprites estáticos e colliders indexados por sala; o tile ancora na origem do mundo; remove `levelCentre_`) e `ShowcaseGame.*` (passa a usar `World`).
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `class World`:
  - `void open(const LdtkProject&, LevelLoader&, EngineServices*)` (`services` pode ser nulo nos testes);
  - `void update(glm::vec2 focus)`;
  - `int currentRoom() const`;
  - `const std::vector<int>& loadedRooms() const`;
  - `void loadAround(int room)`, síncrono;
  - `glm::vec4 worldBounds() const` (`minX`, `minY`, `maxX`, `maxY` em unidades).
- `LevelLoader`:
  - `void loadRoom(const LdtkLevel&, EngineServices&, MaterialLibrary&)`;
  - `void unloadRoom(const std::string& roomIid, Scene2D&)`, que destrói os objetos com aquele `roomIid` e `!persistent`;
  - `void appendStatic(SpriteBatch&) const`, que desenha só as salas carregadas.
- Para os testes sem GPU, `World` aceita hooks `std::function<void(int)> onLoad, onUnload`, que substituem o `LevelLoader`.
- A regra da sala atual segue a Decisão 4 do spec.

Passos:
- [ ] Escrever os testes `world_streaming_loads_neighbours_and_unloads_with_hysteresis` (3 salas em linha, foco andando de uma ponta à outra; conferir `loadedRooms()` em cada passo), `streaming_no_thrash_at_border` (foco parado na borda por 200 updates, com no máximo 1 load e 0 unload depois do primeiro) e `world_unload_keeps_persistent_objects`.
- [ ] Ver RED, implementar, ver GREEN.
- [ ] `ShowcaseGame` abre o projeto com `World` e chama `world.update(player pos)` todo frame. A câmera usa `bounds = worldBounds()`. Recarregar (F5/TAB) abre o projeto de novo.
- [ ] Verificar em execução: a captura da vitrine fica igual.
- [ ] Commit `world: room streaming with hysteresis in a single scene`.

### Task 3: Regiões (fundo repetido e look interpolado)

**Files:**
- Create `src/engine/world/Region.hpp/.cpp` (parse e serialização do `.region.json`), `assets/regions/forest.region.json`, `assets/regions/cave.region.json` e `tools/migrate_showcase_parallax.py`.
- Modify `ShowcaseGame.*` (região atual, camadas, interpolação, painel e salvar), `LookPanel` (edita a região atual), `SceneRenderer.*` (nenhuma mudança de shader esperada) e `assets/levels/showcase.ldtk` (só pelo script).
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `struct RegionLayer { std::string image; float depth; glm::vec2 parallax; glm::vec2 offset; bool repeatX; };`
- `struct Region { std::string name; LookFile look; std::vector<RegionLayer> layers; };`
- `std::optional<Region> parseRegion(const std::string& json, const FrameSettings& defaults)`, `std::string serializeRegion(const Region&)` e `Region loadRegionOrDefault(path, defaults)`, que avisa no log se o arquivo faltar.
- `FrameSettings lerpSettings(const FrameSettings& a, const FrameSettings& b, float t)`: interpola os campos float e vec3; os campos int e bool trocam quando `t >= 0.5`.
- `void appendRegionLayers(const Region&, const Camera2D&, MaterialLibrary&, SpriteBatch&)`: emite só as cópias de `repeatX` visíveis na câmera.

Passos:
- [ ] Escrever os testes `region_roundtrip`, `missing_region_falls_back` e `region_look_blend` (`t = 0`, `t = 1` e meio-termo em 0,75 s de 1,5 s).
- [ ] Ver RED, implementar, ver GREEN.
- [ ] Script `migrate_showcase_parallax.py`:
  - lê `showcase.ldtk`;
  - converte as 5 camadas de parallax do nível `Showcase` (`Reeds_d2`, `Shore_d4`, `Lake_d7`, `Trees_d11`, `Forest_d18`) em camadas de `forest.region.json` (mesma imagem, profundidade e parallax; `offset` calculado para que a posição na tela com a câmera no início da sala seja a mesma de hoje; `repeatX = true`);
  - grava o look atual (`showcase.look.json`) dentro da região;
  - remove essas camadas e as defs delas do `.ldtk`;
  - preenche `region = forest` no nível.

  O script roda uma vez e o resultado é commitado. Antes de rodar, confira `git status` de `showcase.look.json` e de `showcase.ldtk`. Se tiverem edições do autor ainda não commitadas, commite-as primeiro num commit separado `assets: author edits`.
- [ ] `cave.region.json`: copia o forest com névoa mais escura e azulada, ambiente mais baixo e só 2 camadas de fundo.
- [ ] Verificar em execução: capturas antes e depois da migração em 2 posições de câmera (`MUSH2D_CAMERA_X`); a vitrine fica visualmente igual. Painel F1 → Salvar grava a região.
- [ ] Commit `world: regions with world-anchored repeating parallax and blended look`.

### Task 4: `WorldState`, persistência e objetos interativos

**Files:**
- Create `src/engine/world/WorldState.hpp/.cpp`, `src/engine/world/PersistentComponent.hpp`, `src/game/objects/DoorObject.hpp`, `LeverObject.hpp` e `ItemObject.hpp`.
- Modify `CheckpointObject.hpp`, `SlimeEnemy.hpp` (stomp), `PlayerObject.hpp` (sinal de stomp, ↓ para interagir), `World.*` (verificação de respawn) e `ShowcaseGame.cpp` (fábricas novas).
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `struct EntityState { bool gone = false; double goneTime = 0; glm::vec2 spawnPos{0}; std::map<std::string, double> values; };`
- `class WorldState`:
  - `bool isGone(const std::string& iid) const`;
  - `void markGone(iid, glm::vec2 spawnPos, double time)`;
  - `void clearGone(iid)`;
  - `double get(iid, const std::string& key, double def) const`;
  - `void set(iid, key, double)`;
  - `std::map<std::string, EntityState> snapshot(const std::vector<std::string>& iids) const`;
  - `void restore(const std::vector<std::string>& iids, const std::map<std::string, EntityState>& snap)`, que apaga as entradas desses `iid` que não estão na foto.
- `enum class Persistence { None, Permanent, Respawn }` e `Persistence parsePersistence(const LdtkEntity&)`, que considera `persistence` e `respawn`.
- `struct PersistentComponent : Component { std::string iid; Persistence policy; float respawnDistance = 12; };`
- Helper do jogo `void removeTracked(Scene2D&, WorldState&, GameObject&, double time)`, que marca o estado e destrói o objeto.
- `World` guarda o `LdtkEntity` original por `iid` persistente e roda `checkRespawns(playerPos, cameraViewRect, time)` a cada 0,5 s.

Passos:
- [ ] Escrever os testes `worldstate_permanent_never_respawns`, `worldstate_respawn_by_distance_offscreen`, `respawn_waits_until_offscreen`, `interactive_state_survives_unload` e `reload_keeps_world_state`. Use a fábrica e o `World` com hooks, sem GPU.
- [ ] Ver RED, implementar, ver GREEN.
- [ ] Objetos, segundo a Decisão 1 do spec:
  - porta: AABB sólido quando fechada;
  - alavanca: trigger mais ↓, alterna a si mesma e a porta pelo campo `door` (`F_EntityRef` → `iid`);
  - item: coletado ao encostar;
  - checkpoint: grava `"activated"`;
  - slime: stomp (o jogador cai, `vy < 0`, e a base do jogador fica acima do centro do slime) mata o slime e dá um quique de `jumpSpeed * 0.6`.
- [ ] Commit `world: persistent world state, respawn rules, door/lever/item/checkpoint, slime stomp`.

### Task 5: Morte, foto da sala, fade e salas de teste

**Files:**
- Create `tools/add_test_rooms.py`.
- Modify `World.*` (foto da sala na entrada, `onPlayerDeath`), `SceneRenderer.*` e `common.glsl`/`composite.frag` (`fade` em FrameSettings/FrameUniforms; manter o std140 e os static_asserts), `ShowcaseGame.cpp` (tecla K de morte só em Debug; respawn no checkpoint ativado ou no `PlayerStart`), `assets/levels/showcase.ldtk` (só pelo script), README (tecla K, salas) e `docs/ROADMAP.md`.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `World::onEnterRoom` tira a foto dos `iid` da sala nova. Isso usa o índice `iid` → sala, montado no `open`.
- `void World::onPlayerDeath(glm::vec2 respawnPoint, std::function<void(glm::vec2)> teleportPlayer)`: restaura a foto, recarrega a sala da morte, faz `loadAround(roomAt(respawnPoint))` e teleporta.
- `FrameSettings::fade` (`float`, 1 = visível).

Passos:
- [ ] Escrever os testes `death_restores_room_snapshot` e `teleport_streams_target_synchronously`.
- [ ] Ver RED, implementar, ver GREEN.
- [ ] Script `add_test_rooms.py`, segundo a seção "Conteúdo de teste" e a Decisão 2 do spec. Ele acrescenta defs e campos que faltem e 2 níveis:
  - `Forest2`, à direita do `Showcase`;
  - `Cave1`, acima à direita, região `cave`.

  O chão e as plataformas usam `grass.png` com IntGrid coerente. Cada sala tem 1 slime com respawn, 1 slime sem respawn, 1 item `permanent`, 1 item `respawn`, alavanca, porta e checkpoint. O script nunca altera camadas nem entidades do nível `Showcase`. Rode-o uma vez e commite o resultado.
- [ ] Verificar em execução. Use hooks temporários de posição do jogador, removidos antes do commit, e capturas:
  - andar da vitrine até `Forest2` e `Cave1` (capturas em 3 pontos, incluindo a troca para a região `cave`);
  - matar o slime sem respawn, sair e voltar: continua morto;
  - abrir a porta, sair e voltar: continua aberta;
  - pressionar K: a sala é restaurada e o jogador reaparece no checkpoint.

  Logar os tempos de carga de sala (`MUSH2D_PERF_LOG`). Zero erros de validação.
- [ ] README: tecla K e as salas novas. ROADMAP: marcar o item de várias salas.
- [ ] Commit `world: death restores room snapshot with fade, test rooms`.
