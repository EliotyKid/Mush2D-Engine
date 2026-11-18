# Plano de Implementação: Modelo de Entidades

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Deixar o modelo de entidades OO (`GameObject`/`Scene2D`) robusto e completo, com handles seguros, criação e destruição diferidas, hierarquia, componentes, eventos, ordem de atualização e broadphase.

**Architecture:** A herança continua sendo o modelo principal. `Scene2D` vira um *slot map* com flush em pontos fixos do frame. Por cima disso entram hierarquia (transform local/mundo), componentes anexáveis, `EventBus` e `SpatialGrid`.

**Tech Stack:** C++20, glm, harness de testes existente (`tests/tests.cpp`, `TEST`/`CHECK`).

**Spec:** [docs/superpowers/specs/2026-11-19-modelo-entidades-design.md](../specs/2026-11-19-modelo-entidades-design.md). Os valores exatos, as fórmulas e os nomes dos testes estão lá.

## Global Constraints

- `src/engine/**` nunca inclui `src/game/**`.
- A sala vitrine, Sandbox e Test continuam iguais. Conferir com captura de tela (`MUSH2D_SCREENSHOT`) e zero erros de validação. As mensagens de validação saem em **stdout**.
- Mudanças no código de jogo são mecânicas.
- Build com zero warnings e todos os testes passando.
- Commits terminam com uma linha em branco e `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Ambiente: `. .superpowers/sdd/2026-11-19-modelo-entidades/devshell.ps1` (cópia do anterior). Depois: `cmake --preset debug; cmake --build build/debug; ctest --test-dir build/debug`.

## Review Focus

1. **Callbacks reentrantes:** um `onDestroy` que cria ou destrói outros objetos. Esperado: sem crash, e o efeito aplicado no flush seguinte. Teste na Task 1.
2. **Handle guardado pelo jogo depois de um F5/TAB** (`clear`): resolve para `nullptr`, sem crash. Teste `clear_invalidates_old_handles`, Task 1.
3. **Objeto com pai destruído no mesmo frame em que o filho chama `setParent`:** o resultado fica consistente, sem filho órfão apontando para slot livre. Teste na Task 2.
4. **Inscrição em evento cujo dono foi destruído:** não é chamada depois da destruição, graças à `Subscription` RAII. Teste na Task 3.
5. **Collider que muda de tamanho ou é desabilitado em runtime:** a grade reflete a mudança no próximo `step`. Teste na Task 4.

---

### Task 1: Handles, slot map, ciclo de vida diferido, ordem de atualização

**Files:**
- Modify: `src/engine/world/GameObject.hpp`, `src/engine/world/Scene2D.hpp` (extrair para `Scene2D.cpp` se o arquivo crescer) e todos os usuários de `GameObjectId`. `Collision.cpp`, `Camera2D.*`, `SpriteBatch.*`, `LevelLoader.cpp`, `ISceneDefinition.hpp` e `src/game/**` só recebem ajustes mecânicos.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `struct GameObjectId { uint32_t index = 0; uint32_t generation = 0; explicit operator bool() const; bool operator==(const GameObjectId&) const = default; };` mais `std::hash<GameObjectId>` e `InvalidGameObjectId = GameObjectId{}`.
- `Scene2D`:
  - `createObject<T>(args...) -> T&` (o objeto fica pendente; `onCreate` roda imediatamente);
  - `void destroy(GameObjectId)`;
  - `GameObject* find(GameObjectId)` e `const GameObject* find(GameObjectId) const`. Manter `findObject` como alias para não quebrar o jogo;
  - `void flush()`;
  - `void step(float)` e `void lateStep(float)` (fazem flush antes e depois);
  - `template<F> void forEach(F&&)`, que percorre os objetos vivos na ordem de atualização;
  - `size_t aliveCount() const`.
- `GameObject`: `int updateOrder = 0;` e `bool pendingDestroy() const`.
- `getObjects()` é removido. Os usuários passam a usar `forEach`.

- [ ] Escrever os testes `spawn_during_step_is_safe`, `destroy_during_step_is_deferred`, `stale_handle_returns_null`, `clear_invalidates_old_handles` e `update_order_respected`, mais o teste do Review Focus 1, `destroy_inside_onDestroy_is_safe`. Usar classes de teste derivadas de `GameObject` no próprio `tests.cpp`.
- [ ] Rodar e ver RED.
- [ ] Implementar conforme o spec, seções "Identidade e armazenamento", "Ciclo de vida e flush" e "Ordem de atualização".
- [ ] Ajustar mecanicamente todos os usuários.
- [ ] Rodar e ver GREEN.
- [ ] Verificar em execução: captura da vitrine igual, TAB/F5 funcionando, zero erros de validação.
- [ ] Commit `world: slot-map entity handles, deferred create/destroy, update order`.

### Task 2: Hierarquia pai/filho e transform de mundo

**Files:**
- Modify: `GameObject.hpp`, `Scene2D.*`; `SpriteBatch.cpp` (`appendSceneSprites`), `Collision.cpp` e `Camera2D.cpp`, que passam a usar o transform de mundo.
- Test: `tests/tests.cpp`.

**Interfaces (consome a Task 1; produz):**
- `Scene2D::setParent(GameObjectId child, GameObjectId parent, bool keepWorld = true) -> bool` (retorna `false` se formar ciclo).
- `GameObjectId GameObject::parent() const` e `const std::vector<GameObjectId>& GameObject::children() const`.
- `const Transform2D& Scene2D::worldTransform(GameObjectId)` e `Transform2D GameObject::world() const`, este válido depois de `scene.updateTransforms()`.
- `void Scene2D::updateTransforms()`, chamado no início de `step` e no flush.

- [ ] Escrever os testes `child_world_transform`, `destroy_parent_destroys_children`, `reparent_cycle_rejected` e o do Review Focus 3, `setParent_to_dying_parent_is_consistent`.
- [ ] Ver RED, implementar conforme a seção "Hierarquia e transforms" do spec e ver GREEN.
- [ ] Verificar em execução que nada mudou visualmente.
- [ ] Commit `world: parent/child hierarchy with world transforms`.

### Task 3: Componentes, `LightComponent` e `EventBus`

**Files:**
- Create: `src/engine/world/Component.hpp`, `src/engine/world/EventBus.hpp` (e `.cpp` se precisar), `src/engine/render/LightComponent.hpp`.
- Modify: `GameObject.hpp`, `Scene2D.*` e `SceneRenderer.*` (coleta das luzes de componentes). Na `ShowcaseGame`, não mudar as luzes do LDtk.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `Component`, como no spec.
- `GameObject::addComponent<T>(args...) -> T&`, `getComponent<T>() -> T*` e `removeComponent<T>()` (diferido).
- `EventBus::subscribe<E>(std::function<void(const E&)>) -> Subscription` (RAII, move-only), `emit<E>(E)` e `dispatch()`.
- `Scene2D::events() -> EventBus&` e o evento `ObjectDestroyed { GameObjectId id; }`.
- `LightComponent { GpuLight light; /* posição local em light.posHeightRadius.xy */ }`.
- `void collectComponentLights(const Scene2D&, LightList&)`. Engine/ShowcaseGame chama isso todo frame depois de preencher as luzes do nível.

- [ ] Escrever os testes `component_lifecycle_and_step_order`, `event_bus_deferred_dispatch_and_unsubscribe`, o do Review Focus 4 `subscription_dies_with_owner`, e `light_component_follows_owner` (pura: chama `collectComponentLights` numa `LightList` sem GPU).
- [ ] Ver RED, implementar conforme as seções "Componentes" e "Eventos" do spec e ver GREEN.
- [ ] Verificar em execução. Adicionar temporariamente um `LightComponent` a um crystal ou critter para provar que a luz segue o dono, tirar uma captura e remover antes do commit.
- [ ] Commit `world: components, LightComponent, deferred event bus`.

### Task 4: Broadphase em grade

**Files:**
- Create: `src/engine/world/SpatialGrid.hpp/.cpp`.
- Modify: `Scene2D.*` (manter a grade sincronizada), `Collision.*` (`collidesAtPosition` via grade, mais `queryAABB`) e a varredura de triggers em `ShowcaseGame.cpp`.
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `class SpatialGrid { explicit SpatialGrid(float cell = 2.0f); void insert(GameObjectId, AABB); void update(GameObjectId, AABB); void remove(GameObjectId); void query(const AABB&, std::vector<GameObjectId>& out) const; };`
- `std::vector<GameObjectId> Collision2D::queryAABB(const Scene2D&, const AABB&)`.

- [ ] Escrever os testes `spatial_grid_matches_bruteforce` (500 AABBs com seed fixa e 200 consultas) e o do Review Focus 5, `grid_tracks_collider_changes`.
- [ ] Ver RED, implementar conforme a seção "Colisão" do spec e ver GREEN.
- [ ] Verificar em execução: o player anda, pula e colide igual a antes; checkpoints e teleports do Sandbox ainda disparam.
- [ ] Commit `world: spatial grid broadphase for collision and triggers`.
