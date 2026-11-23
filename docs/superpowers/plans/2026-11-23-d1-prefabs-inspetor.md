# D1: Prefabs e Inspetor — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Fazer os objetos nascerem de prefabs (`.prefab.json`), com atributos exportados no estilo Godot, registro automático, ligação com o LDtk, atlas lidos da pasta, inspetor em árvore no F1 e hot reload de prefabs.

**Architecture:** Um registro de tipos próprio fica em `src/engine/reflect/` e é preenchido por `MUSH2D_OBJECT`, `MUSH2D_COMPONENT`, `MUSH2D_EXPORT` e `MUSH2D_ENUM`. O carregador de prefabs (engine) monta árvores de `GameObject` a partir desse registro. O `LevelLoader` passa a criar entidades por prefab. O jogo vira a biblioteca `mush2d_game`, com o registro gerado pelo CMake.

**Tech Stack:** C++20, `nlohmann_json`, Dear ImGui, CMake (`file(GLOB ... CONFIGURE_DEPENDS)` + `configure_file`).

**Spec:** [docs/superpowers/specs/2026-11-23-d1-prefabs-inspetor-design.md](../specs/2026-11-23-d1-prefabs-inspetor-design.md). Inclui a seção "Decisões do Claude".

## Global Constraints

- Prefabs em `assets/prefabs/<nome>.prefab.json`, com `"version": 1`.
- Ordem de aplicação: padrões do C++ → prefab-base → prefab → campos do LDtk → edições ao vivo.
- Identificador LDtk `NomeEntidade` vira `nome_entidade.prefab.json`. O campo `prefab` da entidade, se existir, tem prioridade.
- Fábricas em código só para `Terrain`, `Light` e `PlayerStart`.
- O registro de tipos é um `static` local de função. `mush2dRegisterGameTypes()` é chamada explicitamente.
- `src/engine/**` nunca inclui `src/game/**`.
- A vitrine fica visualmente igual na captura `MUSH2D_SCREENSHOT`, exceto a cintilação.
- Zero erros de validação. Eles saem em stdout.
- Commits terminam com uma linha em branco e `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Ambiente: `. .superpowers/sdd/2026-11-23-d1/devshell.ps1`, depois `cmake --preset debug; cmake --build build/debug; ctest --test-dir build/debug`.
- O conteúdo de `assets/levels/showcase.ldtk` não é alterado.

## Review Focus

1. **Prefab com JSON inválido** (por exemplo, salvo pela metade): aviso com o nome do arquivo e o objeto com valores padrão, ou mantido como estava no hot reload. Sem crash. Teste `prefab_invalid_json_warns` (Task 2).
2. **Atributo com tipo errado** (`"speed": "rápido"`): aviso com arquivo e campo, e o objeto fica com o valor padrão. Teste `prefab_wrong_type_warns` (Task 2).
3. **Entidade do LDtk sem prefab e sem fábrica:** aviso uma vez por identificador, depois ignorada. Teste `ldtk_entity_without_prefab_warns_once` (Task 2).
4. **"Salvar no prefab" num objeto sem prefab de origem** (criado em código): o botão fica desabilitado e só "Salvar como novo prefab" está disponível. Teste no nível da função de serialização (Task 4).
5. **Dois objetos do mesmo prefab editados de forma diferente:** cada um salva a própria diferença no mesmo arquivo, e a última gravação vence. Isso fica documentado no aviso do botão. Sem teste, só comportamento documentado.

---

### Task 1: Reflexão, registro, serviços e biblioteca do jogo

**Files:**
- Create: `src/engine/reflect/Reflect.hpp/.cpp` (registro, macros, tipos e conversão JSON ↔ valor) e `src/engine/reflect/SchemaExport.cpp`.
- Modify: `GameObject.hpp` (`entityIid`, `prefab` e as exportações dos dados-base, incluindo os atalhos `spriteAtlas`, `spriteFrame`, `flipX` e `depth`), `Component.hpp` (base exportável), `Scene2D.hpp` (`setService`/`service`), `ObjectData.hpp` (`SpriteData::additive`) e `CMakeLists.txt` (biblioteca `mush2d_game` + `all_objects.cpp` gerado + `mush2dRegisterGameTypes`).
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `enum class PropType { Float, Int, Bool, String, Vec2, Color, Enum, PrefabRef, EntityRef, AssetRef };`
- `struct PropertyInfo { std::string name; PropType type; std::string enumName; nlohmann::json defaultValue; std::function<nlohmann::json(const void*)> get; std::function<bool(void*, const nlohmann::json&, std::string& error)> set; };`
- `struct TypeInfo { std::string name, base; bool isComponent; std::function<std::unique_ptr<GameObject>()> createObject; std::function<std::unique_ptr<Component>()> createComponent; std::vector<PropertyInfo> props; };`
- `TypeRegistry& typeRegistry();`, com `const TypeInfo* find(name)`, `std::vector<const PropertyInfo*> allProps(name)` (base primeiro) e `const std::vector<std::string>* enumValues(name)`.
- Macros `MUSH2D_OBJECT(Class, Base)`, `MUSH2D_COMPONENT(Class)`, `MUSH2D_EXPORT(type, name, default)` e `MUSH2D_ENUM(Enum, values...)`.
- `const TypeInfo& typeOf(const GameObject&)` (virtual via a macro).
- `nlohmann::json exportSchema()`; em Debug, o jogo grava o resultado em `assets/prefabs/prefab_schema.json`.
- `template<class T> void Scene2D::setService(T*)` e `T* Scene2D::service() const`.
- `void mush2dRegisterGameTypes();`, gerada pelo CMake.

Steps:
- [ ] Escrever os testes `reflect_registers_class_and_props` (classes de teste com um export de cada tipo, base e herança) e `reflect_set_wrong_type_reports_error`, `scene_services`, `schema_export_lists_types`.
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Mover os objetos do jogo para a biblioteca `mush2d_game` com o `all_objects.cpp` gerado. O executável e os testes passam a linkar essa biblioteca e chamar `mush2dRegisterGameTypes()`. Nenhuma mudança de comportamento.
- [ ] Commit `reflect: type registry, exported properties, scene services, game library`.

### Task 2: Prefabs, assets pela pasta, `SpriteAnimator` e LDtk

**Files:**
- Create: `src/engine/world/Prefab.hpp/.cpp` (carga, herança e aplicação), `src/engine/assets/AssetRegistry.hpp/.cpp` (varredura de atlas) e `src/engine/world/SpriteAnimator.hpp/.cpp` (componente).
- Modify: `LevelLoader.*` (entidade → prefab, com fallback para as fábricas), `FileWatcher` ou o uso dele (observar `assets/prefabs/`) e `GameAssets.*` (passa a usar o `AssetRegistry`, mantendo os atalhos por id).
- Test: `tests/tests.cpp` e `tests/data/prefabs/*.prefab.json`.

**Interfaces (produz):**
- `class PrefabLibrary { const nlohmann::json* resolved(name, std::string& error); GameObject* instantiate(Scene2D&, name, const nlohmann::json& overrides = {}); void clear(); };`. `resolved` devolve a mesclagem final já com a base, e guarda o resultado em cache.
- `std::vector<std::string> applyProps(void* obj, const std::string& typeName, const nlohmann::json& props)`. Retorna os avisos.
- `class AssetRegistry { void scan(const std::filesystem::path& root); const TextureAtlas* atlas(name); std::optional<std::pair<std::string,std::string>> resolve(const std::string& ref); };`. `ref` é `"atlas/frame"` ou `"atlas/anim"`.
- `class SpriteAnimator : Component { MUSH2D_EXPORT(std::string, state, "idle") ... void setState(const std::string&); std::map<std::string,std::string> animations; };`. Faz o posicionamento pelo pivô, que hoje está em `PivotSprite`.
- `GameObject` ganha `const nlohmann::json& extra() const` e `std::set<std::string> ldtkOverridden`.

Steps:
- [ ] Escrever os testes listados na seção "Testes" do spec que tratam de prefab, LDtk e assets, além dos testes da Review Focus 1–3.
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Commit `world: prefabs with inheritance, asset registry, sprite animator, ldtk prefab spawn`.

### Task 3: Migração dos objetos e `IkigaiGame`

**Files:**
- Modify:
  - `src/game/objects/*`:
    - Slime, Door, Lever, Item e Checkpoint passam a usar `MUSH2D_EXPORT`, sem `Config` e sem argumentos no construtor, e pegam o `WorldState` por `scene.service`.
    - Player exporta os atributos de movimento.
    - Critter vira prefab `GameObject` + `SpriteAnimator`.
  - `src/game/spawners/Spawn.*`, `src/game/scenes/*`.
  - `ShowcaseGame.*` → `IkigaiGame.*`: remove as fábricas migradas, registra os serviços e tira os cristais do desenho manual.
  - `src/main.cpp`, `README.md`, `docs/GUIA.md`.
- Create:
  - `src/game/objects/CrystalObject.hpp`, com um filho halo aditivo.
  - `assets/prefabs/{slime,door,lever,item,checkpoint,critter,prop,crystal,crystal_halo}.prefab.json`.
  - `assets/prefabs/slime_fast.prefab.json`, que herda do slime e serve ao critério 3.
- Test: `tests/tests.cpp`. Os testes existentes de slime, porta, alavanca e checkpoint continuam passando, adaptados só na forma de criar o objeto.

Steps:
- [ ] Tirar a captura de referência `MUSH2D_SCREENSHOT` antes de qualquer mudança: `.superpowers/sdd/2026-11-23-d1/before.png`.
- [ ] Migrar objeto por objeto, com a suíte de testes verde a cada commit.
- [ ] Renomear `ShowcaseGame` para `IkigaiGame`.
- [ ] Colocar um `SlimeFast` no LDtk **por cópia temporária**. O arquivo do autor não muda: usar uma cópia e restaurar depois, ou um hook temporário.
- [ ] Verificação em execução:
  - captura `after.png` igual, exceto a cintilação;
  - TAB e F5 funcionam;
  - porta, alavanca, item, pulo no slime e checkpoint funcionam;
  - zero erros de validação.
- [ ] Commits por objeto e um commit `game: IkigaiGame, objects from prefabs`.

### Task 4: Inspetor em árvore e Salvar

**Files:**
- Create: `src/engine/tools/SceneInspector.hpp/.cpp` e `src/engine/world/PrefabSave.hpp/.cpp` (cálculo da diferença e gravação).
- Modify: `IkigaiGame.cpp`, para incluir a aba "Cena" no `onImGui` e o desenho do contorno de seleção (sprite de destaque ou linhas).
- Test: `tests/tests.cpp`.

**Interfaces (produz):**
- `nlohmann::json diffAgainstBase(const GameObject&, const nlohmann::json& resolvedBase)`. Exclui `ldtkOverridden`.
- `bool savePrefab(path, const nlohmann::json& doc, std::string& error)`. Mantém `version`, `base`, `type`, `children` e `components` do arquivo original e atualiza só os valores alterados.
- `void SceneInspector::draw(Scene2D&, PrefabLibrary&, bool& pause)`.

Steps:
- [ ] Testes: `inspector_save_writes_only_diff`, `inspector_save_excludes_ldtk_values`, `save_as_new_prefab_sets_base` e `save_disabled_without_source_prefab`.
- [ ] Rodar e ver RED. Implementar. Rodar e ver GREEN.
- [ ] Verificação em execução, com um hook temporário para abrir o painel, selecionar um slime e mudar `moveSpeed`:
  - o efeito aparece ao vivo;
  - salvar grava só esse campo (o hash muda apenas no prefab do slime e é restaurado depois);
  - o hot reload do prefab atualiza o jogo.
- [ ] Commit `tools: scene inspector tree with prefab save`.
