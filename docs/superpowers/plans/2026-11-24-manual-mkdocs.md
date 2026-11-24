# Manual MkDocs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Site local MkDocs Material em `manual/` com a documentação e o manual da Mush2D, pesquisável, com casos de uso.

**Architecture:** Conteúdo em `.md` dentro de `manual/docs/`, configurado por `manual/mkdocs.yml`. O texto de `docs/GUIA.md` e das seções de uso do `README.md` é movido para o manual (não copiado); README e GUIA passam a apontar para ele. Páginas novas de objetos e sistemas são escritas lendo o código atual.

**Tech Stack:** Python 3.12, `mkdocs-material==9.5.49`.

**Spec:** `docs/superpowers/specs/2026-11-24-manual-mkdocs-design.md`

## Global Constraints

- Todo texto em português.
- Nada descrito que o código atual não faça; cada afirmação vem de leitura de `src/`, `assets/prefabs/` ou `shaders/`.
- Sem duas cópias do mesmo texto: o que vai para o manual sai do README/GUIA.
- `manual/site/` no `.gitignore`.
- Páginas de referência (objetos e sistemas) seguem as seções: "O que faz", "Tipos principais" (ou "Atributos" para objetos), "Casos de uso", "Arquivos".
- Specs, plans e ROADMAP ficam fora do manual.

## Review Focus

- Link quebrado ou página fora do `nav`: `mkdocs build --strict` deve falhar; cada task roda ele.
- Arquivos com acentos lidos com encoding errado (o README aparece com mojibake no PowerShell): todo `.md` salvo em UTF-8 sem BOM; conferir "ação" renderizado no navegador na Task 6.
- Busca em português: buscar "rampa" e "alavanca" deve achar as páginas certas (Task 6).
- Texto movido do README/GUIA perdido no caminho: a Task 2 e a Task 3 listam cada seção de origem e o destino; conferir com `git diff --stat` e leitura.
- Atributo de objeto inventado: na Task 4, cada atributo listado deve existir como `MUSH2D_EXPORT` no `.hpp` do objeto ou em `assets/prefabs/*.prefab.json`.

## Estrutura de arquivos

```
manual/
  mkdocs.yml
  requirements.txt
  docs/
    index.md
    primeiros-passos.md
    desempenho.md
    conceitos/gamemaker.md, ciclo-do-jogo.md, cpp-minimo.md
    fazendo-o-jogo/mundo-e-salas.md, colisao-e-rampas.md, terreno.md,
                   prefabs.md, objeto-novo.md, comunicacao.md, inspetor.md
    objetos.md
    sistemas/core.md, platform.md, world.md, graficos-e-assets.md,
             render.md, gpu.md, tools.md, reflect.md
```

---

### Task 1: Esqueleto do site

**Files:**
- Create: `manual/requirements.txt`, `manual/mkdocs.yml`, `manual/docs/index.md`
- Modify: `.gitignore` (adicionar `manual/site/`)

**Interfaces:**
- Produces: `nav` em `mkdocs.yml` com todos os caminhos da estrutura acima; as tasks seguintes só criam os arquivos.

- [ ] **Step 1:** `requirements.txt` com `mkdocs-material==9.5.49`. Instalar: `py -m pip install -r manual/requirements.txt`.
- [ ] **Step 2:** `mkdocs.yml`: `site_name: Manual Mush2D`, `theme: material` com `language: pt`, paleta clara/escura com toggle, features `navigation.sections`, `navigation.top`, `toc.follow`, `search.suggest`, `search.highlight`, `content.code.copy`; plugin `search` com `lang: pt`; extensões `admonition`, `pymdownx.details`, `pymdownx.superfences`, `pymdownx.highlight`, `pymdownx.tabbed` (`alternate_style: true`), `tables`, `toc` (`permalink: true`). `nav` completo conforme a estrutura.
- [ ] **Step 3:** `index.md`: o que é a engine (C++20/Vulkan 2D, jogo `IkigaiGame`), como o manual está dividido, como abrir (`mkdocs serve -f manual/mkdocs.yml`).
- [ ] **Step 4:** Criar os outros arquivos do `nav` com só o título `# ...` (as tasks seguintes preenchem).
- [ ] **Step 5:** Run `py -m mkdocs build --strict -f manual/mkdocs.yml`. Expected: termina sem `WARNING`.
- [ ] **Step 6:** Commit `docs: manual MkDocs skeleton`.

### Task 2: Primeiros passos e Desempenho (do README)

**Files:**
- Modify: `manual/docs/primeiros-passos.md`, `manual/docs/desempenho.md`, `README.md`

- [ ] **Step 1:** `primeiros-passos.md` recebe do README: Prerequisites, Build (incluindo a nota do imgui no Release), Controles, Variáveis de ambiente. Mais um caso de uso por variável (ex.: `MUSH2D_SCREENSHOT` para capturar uma tela) e como rodar os testes (`ctest --test-dir build/debug`).
- [ ] **Step 2:** `desempenho.md` recebe a seção Desempenho e um passo a passo de medir com `MUSH2D_PERF_LOG`.
- [ ] **Step 3:** README fica só com título, frase de abertura, Prerequisites, Build e o link: "Manual completo: `manual/` (`mkdocs serve -f manual/mkdocs.yml`)". As seções Controles, Mundo, Prefabs, Variáveis de ambiente e Desempenho saem (Mundo e Prefabs vão na Task 3).
- [ ] **Step 4:** `mkdocs build --strict`. Expected: sem `WARNING`.
- [ ] **Step 5:** Commit `docs: move README usage sections to the manual` (inclui Task 3 se preferir um commit só para o README, mas Mundo/Prefabs devem estar no manual antes de saírem do README).

### Task 3: Conceitos e Fazendo o jogo (do GUIA e README)

**Files:**
- Modify: `manual/docs/conceitos/*.md`, `manual/docs/fazendo-o-jogo/*.md`, `docs/GUIA.md`

Mapa de origem para destino:

| Origem | Destino |
|---|---|
| GUIA Parte 1 (tabela GameMaker, "como uma entidade vira objeto") | `conceitos/gamemaker.md` |
| GUIA Parte 1 "O que acontece quando o jogo abre" | `conceitos/ciclo-do-jogo.md` |
| GUIA Parte 2 | `conceitos/cpp-minimo.md` |
| README Mundo + GUIA Parte 5 "Uma sala nova" | `fazendo-o-jogo/mundo-e-salas.md` |
| README Colisão no LDtk (IntGrid) + Tiles de rampa | `fazendo-o-jogo/colisao-e-rampas.md` |
| README entidade `Terrain` | `fazendo-o-jogo/terreno.md` |
| README Prefabs + GUIA Parte 5 "Uma variação de um objeto" | `fazendo-o-jogo/prefabs.md` |
| GUIA Parte 3 (alavanca linha a linha) + Parte 5 "Um objeto novo" e "Herdar de outro objeto" | `fazendo-o-jogo/objeto-novo.md` |
| GUIA Parte 5 "Comunicação entre objetos" | `fazendo-o-jogo/comunicacao.md` |
| README itens do `F1`/"Cena (F1)"/"Salvar" | `fazendo-o-jogo/inspetor.md` |
| GUIA Parte 4 | base das páginas `sistemas/` (Task 5) |
| GUIA Parte 6 | `index.md`, seção "Limitações atuais" |

- [ ] **Step 1:** Mover cada trecho conforme a tabela, ajustando títulos e trocando referências "Parte N" por links entre páginas. Usar admonitions (`!!! tip`, `!!! warning`) para avisos já existentes no texto (ex.: Release sobrescreve região salva).
- [ ] **Step 2:** Cada página de `fazendo-o-jogo/` termina com "Casos de uso": 1 a 3 receitas curtas verificadas no código/assets.
- [ ] **Step 3:** `docs/GUIA.md` vira uma linha apontando para `manual/`.
- [ ] **Step 4:** `mkdocs build --strict`. Expected: sem `WARNING`.
- [ ] **Step 5:** Commit `docs: move guide into the manual`.

### Task 4: Objetos do jogo

**Files:**
- Modify: `manual/docs/objetos.md`
- Read: `src/game/objects/*.hpp`, `assets/prefabs/*.prefab.json`, `src/game/spawners/Spawn.cpp`

- [ ] **Step 1:** Uma seção `##` por objeto: Player, Slime (e `slime_fast`), Door, Lever, Item, Checkpoint, Critter, Prop, Crystal (e `crystal_halo`), Teleport, Wall; mais `PlayerStart`, `Light`, `Terrain` como "entidades com fábrica em código". Cada uma: "O que faz", "Atributos" (tabela nome, tipo, padrão, efeito — só `MUSH2D_EXPORT`/props reais), "Casos de uso" (ex.: ligar alavanca a porta no LDtk), "Arquivos".
- [ ] **Step 2:** Conferir cada atributo listado contra o `.hpp`/prefab (grep pelo nome).
- [ ] **Step 3:** `mkdocs build --strict`. Expected: sem `WARNING`.
- [ ] **Step 4:** Commit `docs: game objects reference`.

### Task 5: Referência de sistemas

**Files:**
- Modify: `manual/docs/sistemas/*.md`
- Read: `src/engine/<pasta>/`, `shaders/`, GUIA Parte 4 (já movida como base)

- [ ] **Step 1:** Uma página por pasta: `core` (Engine, IGame, FileWatcher, Paths), `platform` (GlfwInput, InputState), `world` (GameObject, Component, Scene2D, World, WorldState, CollisionWorld, CharacterController, SpatialGrid, EventBus, Ldtk, LevelLoader, Prefab, PrefabSave, Region, RegionSet, TerrainMesh, SpriteAnimator, PersistentComponent), `graficos-e-assets` (Camera2D, catálogos, AssetRegistry, AtlasLoader, TextureAtlas), `render` (SceneRenderer, passadas gbuffer/light/resolve/additive/bloom/composite, SpriteBatch, Materials, LightComponent, PixelScale, GpuTimers, shaders), `gpu` (GpuContext, Swapchain, Buffer, Image, VMA), `tools` (LookPanel, LookFile, SceneInspector), `reflect` (Reflect, SchemaExport, `MUSH2D_EXPORT`).
- [ ] **Step 2:** Formato fixo: "O que faz", "Tipos principais" (tabela tipo e papel, com as funções públicas mais usadas), "Casos de uso" (trechos curtos tirados do código do jogo em `src/game/`), "Arquivos".
- [ ] **Step 3:** `mkdocs build --strict`. Expected: sem `WARNING`.
- [ ] **Step 4:** Commit `docs: engine systems reference`.

### Task 6: Verificação no navegador

- [ ] **Step 1:** `py -m mkdocs serve -f manual/mkdocs.yml` em segundo plano; abrir `http://127.0.0.1:8000`.
- [ ] **Step 2:** Conferir: acentos corretos, menu completo, modo escuro, busca "rampa" acha `colisao-e-rampas`, busca "alavanca" acha `objetos` e `objeto-novo`.
- [ ] **Step 3:** Corrigir o que falhar e commitar `docs: manual fixes` se houver mudança.
