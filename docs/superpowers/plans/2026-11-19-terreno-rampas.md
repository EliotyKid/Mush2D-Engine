# Terreno e Rampas: Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Colisão por segmentos para rampas em grade, terreno livre e plataformas atravessáveis; um controlador de personagem próprio usado pelo player e pelos slimes; visual do terreno gerado a partir da linha; hot reload do `.ldtk`.

**Architecture:** O `CollisionWorld` (engine) gera segmentos por sala a partir do IntGrid e das entidades `Terrain`, e os indexa na `SpatialGrid`. O `CharacterController` (engine) move uma caixa contra esses segmentos e contra AABBs sólidas, em sub-passos. O `TerrainMesh` gera instâncias do `SpriteBatch`: colunas de preenchimento mais pedaços girados de borda.

**Tech Stack:** C++20, glm, a infraestrutura de testes existente e Python com Pillow para os scripts de textura e conteúdo.

**Spec:** [docs/superpowers/specs/2026-11-19-terreno-rampas-design.md](../specs/2026-11-19-terreno-rampas-design.md). A seção "Decisões do Claude" faz parte do spec.

## Global Constraints

- 32 px por unidade. A camada `Terrain` tem grade de 4 px. Um ponto F_Point `{cx,cy}` vira `ldtkToWorld(level.worldPx, {cx*4, cy*4})`.
- Valores do IntGrid `Collision`:
  - 1 = sólido;
  - 2 = plataforma atravessável (aresta de cima);
  - 3/4 = rampa de 45°, subindo para a direita / para a esquerda;
  - 5/6 = rampa de 22,5° baixa, direita / esquerda;
  - 7/8 = rampa de 22,5° alta, direita / esquerda.
- Chão é toda superfície com `normal.y >= cos(50°)`. Snap ao descer: até 0,3 u. Cantos arredondados: raio 0,1 u. Sub-passo: no máximo 0,1 u. Descer pela plataforma (drop-through): 0,25 s.
- Física do player sem mudança: `moveSpeed 4.5`, `gravity -32`, `jumpSpeed 11`, `jumpCutFactor 0.5`, `maxFallSpeed -18`. `dt` continua limitado a 0,1.
- O nível `Showcase` do autor não é alterado. Os scripts só mexem em `Forest2` e `Cave1` e acrescentam definições.
- `src/engine/**` não inclui `src/game/**`. Zero erros de validação (eles saem em stdout). Build sem warnings.
- Todo commit termina com uma linha em branco e `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Ambiente: rode `. .superpowers/sdd/2026-11-19-terreno-rampas/devshell.ps1`, depois `cmake --preset debug; cmake --build build/debug; ctest --test-dir build/debug`.

## Review Focus

1. **Player parado exatamente no vértice entre uma rampa e o plano, por muitos frames.** Esperado: nem escorrega nem treme. Teste `controller_rests_on_slope_vertex` (Task 2).
2. **Slime numa rampa de 22,5° chegando à beira de um degrau.** Esperado: vira, não cai. Teste `slime_turns_at_ledge_on_slope` (Task 3).
3. **Hot reload com o player dentro de geometria nova.** Esperado: é empurrado para fora até 2 u; se não conseguir, reaparece no checkpoint. Teste `hot_reload_depenetrates_or_respawns` (Task 5).
4. **Polígono `Terrain` fechado com pontos em sentido anti-horário e horário.** Esperado: normais sempre para fora. Teste `terrain_orientation_both_windings` (Task 1).
5. **Galho atravessável colado num bloco sólido.** Esperado: o topo do galho segura o player, e o lado do bloco continua sólido. Teste `oneway_next_to_solid` (Task 2).

---

### Task 1: Segmentos de colisão (`CollisionWorld`)

**Files:**
- Create: `src/engine/world/CollisionWorld.hpp/.cpp`.
- Modify: `src/engine/world/Ldtk.*`, para ler arrays de `F_Point` (`__value` é uma lista de `{cx,cy}`) e o `gridSize` da camada da entidade.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `struct Segment { glm::vec2 a, b, normal; bool oneWay; std::string roomIid; };`
- `std::vector<Segment> segmentsFromIntGrid(const LdtkLayer& collision, glm::ivec2 levelWorldPx, glm::ivec2 levelPxSize, const std::string& roomIid)`. Cobre fusão colinear, sem arestas internas, sem faces na borda da sala, e trata `5`+`7` como rampa contínua.
- `std::vector<Segment> segmentsFromTerrain(const LdtkEntity&, glm::ivec2 levelWorldPx, int layerGridPx, const std::string& roomIid)`. Cobre `closed`/`thickness`/`oneWay`, orientação corrigida e duplicata do primeiro ponto descartada.
- `class CollisionWorld { void addRoom(roomIid, std::vector<Segment>); void removeRoom(roomIid); void query(const Collision2D::AABB&, std::vector<const Segment*>& out) const; };`. Por dentro usa uma `SpatialGrid` própria.
- `LdtkEntity` ganha `int layerGridPx`. O valor de um campo F_Point (ou de um array dele) continua em `fields`.

Steps:
- [ ] Testes: `intgrid_segments_outer_edges_only`, `intgrid_slopes_and_oneway` (inclui `5`+`7` e `2` encostado em `1`), `intgrid_no_faces_on_room_border`, `terrain_points_to_world`, `terrain_orientation_both_windings`, `terrain_open_surface_thickness`.
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Commit `world: collision segments from intgrid slopes and terrain entities`.

### Task 2: `CharacterController`

**Files:**
- Create: `src/engine/world/CharacterController.hpp/.cpp`.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `struct Body { glm::vec2 position, halfSize; float dropThroughTimer = 0; const Segment* droppingThrough = nullptr; };`
- `struct MoveResult { glm::vec2 position, velocity; bool grounded, onOneWay, hitCeiling, onSteep; glm::vec2 groundNormal; };`
- `struct CollisionQuery { const CollisionWorld* world; std::vector<Collision2D::AABB> solids; };`. Os sólidos são as AABBs dinâmicas, sem o próprio corpo.
- `MoveResult move(Body&, glm::vec2 velocity, float dt, const CollisionQuery&, bool wasGrounded, bool jumpedThisFrame)`.
- `bool requestDropThrough(Body&, const MoveResult& last)`. Só vale em cima de um segmento `oneWay`.

Regras: seção "Controlador de personagem" e decisões 3, 4 e 5 do spec.

Steps:
- [ ] Testes:
  - `controller_walks_up_and_down_45`, `controller_rests_on_slope`, `controller_rests_on_slope_vertex`;
  - `controller_slides_on_steep`, `controller_no_snag_on_seams`, `controller_oneway_from_below_and_drop`;
  - `oneway_next_to_solid`, `controller_no_tunnel_at_max_fall`, `controller_ceiling_stops_vy`;
  - `controller_speed_on_slope_matches_flat`: a distância horizontal por segundo numa rampa de 22,5° fica a ±5% do plano.
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Commit `world: segment-based character controller with slopes and one-way platforms`.

### Task 3: Integração (salas, player, slime, portas)

**Files:**
- Modify: `LevelLoader.*` e `World.*`. Ao carregar a sala, gera os segmentos e chama `addRoom`. Ao descarregar, chama `removeRoom`. Os colliders do IntGrid deixam de ser objetos: as AABBs viram segmentos. Portas e slimes continuam objetos com AABB.
- Modify: `EngineServices`, que ganha `CollisionWorld& collision`; `Engine.cpp`; `PlayerObject.hpp`, que troca `stepPlatformer` pelo controlador e passa a ter ↓ + pulo; `SlimeEnemy.hpp`, que usa o controlador e vira na beira do degrau; `Collision.*`, onde `collidesAtPosition` passa a considerar também os segmentos, para os usos restantes.
- Test: `tests/tests.cpp`.

Steps:
- [ ] Testes `slime_turns_at_ledge_on_slope` e `player_jump_height_unchanged` (o ápice continua em ~1,89 u ≈ 3,8 células).
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Verificar em execução que a vitrine não mudou: andar, pular, pisar no slime, portas, mesma captura de tela.
- [ ] Commit `world: rooms build collision segments; player and slime use the controller`.

### Task 4: Visual do terreno e normal girado

**Files:**
- Create: `src/engine/world/TerrainMesh.hpp/.cpp` e `tools/gen_terrain_textures.py`, que gera `assets/terrain/{forest,cave}/{edge,side,fill}.png` e os `_n` a partir de `grass.png` (decisão do spec).
- Modify: `shaders/gbuffer.frag`, para girar a normal amostrada por `inst.rotation`; `LevelLoader.*`, que gera a malha por sala e a desenha via `appendStatic`.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `struct TerrainStyle { Material edge, side, fill; glm::vec2 edgeSize, fillTile; };`
- `void buildTerrainMesh(const std::vector<Segment>& polyline, bool closed, float thickness, const TerrainStyle&, std::vector<SpriteInstance>& out)`.

Steps:
- [ ] Testes: `terrain_mesh_covers_columns` (sem buracos, com cobertura até `thickness`) e `terrain_edge_follows_surface` (a quantidade e a rotação dos pedaços de borda batem com o comprimento e o ângulo dos trechos).
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Verificar em execução com um `Terrain` temporário, de preferência criado por hook: capturas da borda em rampa sob luz, com o normal girado. Remover o hook antes do commit.
- [ ] Commit `render: terrain mesh from polylines, rotated normals`.

### Task 5: Conteúdo de teste e hot reload

**Files:**
- Create: `tools/add_terrain_content.py`.
- Modify: `assets/levels/showcase.ldtk` (só pelo script), `ShowcaseGame.cpp` (hot reload), README e ROADMAP.
- Test: `tests/tests.cpp`.

**Ruling do plano:** escrever regras automáticas (auto-rules) do LDtk por script é frágil. Por isso o script coloca os tiles de rampa diretamente como `gridTiles` em `Cave1`, usando `grass_slopes.png`, gerado pelo `gen_terrain_textures.py`. O README explica ao autor como criar as auto-rules de rampa no editor.

Steps:
- [ ] Script `add_terrain_content.py`. Ele recusa rodar se `Forest2` já tiver a camada `Terrain`. Ele:
  - acrescenta à definição a camada de entidades `Terrain` (grade de 4 px) e a entidade `Terrain`, com os campos do spec;
  - em `Forest2`, cria um chão orgânico com 2 morros, uma descida de cerca de 30° e um galho atravessável;
  - em `Cave1`, cria rampas de 45° e de 22,5° (`5`+`7`) e uma plataforma `2`.
- [ ] Hot reload em Debug: verifica a data de modificação a cada 0,5 s e recarrega como o F5. Teste `hot_reload_depenetrates_or_respawns`, com `World` e controlador, sem GPU.
- [ ] Verificar em execução, com hooks temporários de posição e capturas de tela:
  - subir e descer os morros de `Forest2`;
  - parar na rampa sem escorregar;
  - atravessar o galho por baixo e descer com ↓ + pulo;
  - passar pelas rampas de `Cave1`;
  - salvar o `.ldtk` (tocando no arquivo) e ver o mundo recarregar.

  Nenhum erro de validação.
- [ ] README: valores do IntGrid, entidade `Terrain`, ↓ + pulo e como criar auto-rules de rampa. ROADMAP: marcar a Fase 1.2.
- [ ] Commit `world: terrain and slope test content, ldtk hot reload`.
