# D1: Fluxo de criação (prefabs e inspetor)

Data: 2026-11-23
Status: rascunho para aprovação
Roadmap: [D1](../../ROADMAP.md)

## Intenção

Hoje, criar um inimigo, NPC ou item no Ikigai exige quatro coisas:
1. escrever a classe C++ com os valores fixos no código;
2. registrar uma fábrica à mão no `ShowcaseGame`;
3. registrar o sprite à mão em `GameAssets.cpp`;
4. recompilar.

A D1 troca isso por um fluxo no estilo GameMaker + Godot:
- os objetos são descritos em **prefabs** (arquivos), com atributos editáveis;
- uma **árvore de nós** (filhos e componentes), como a Godot;
- um **inspetor ao vivo** no painel F1.

Criar um inimigo novo vira "copiar um prefab e mudar números", sem recompilar. Todas as fases seguintes (D2–D12) criam o conteúdo delas com esse fluxo.

## Decisões do autor

| Tema | Decisão |
|---|---|
| Atributos | Estilo Godot: `MUSH2D_EXPORT(tipo, nome, padrão)` marca uma variável C++ como editável. Variáveis extras livres no prefab para coisas simples. |
| Implementação | Registro próprio (macro + JSON + tabelas), sem biblioteca de reflexão e sem geração de código. |
| Editor | Por enquanto, o inspetor fica dentro do jogo (F1). IDE própria e fork do LDtk ficam para depois, mas devem ler as mesmas tabelas e arquivos. |

## Objetivos

1. **Reflexão mínima.** `MUSH2D_OBJECT(Classe, Base)` registra a classe pelo nome, e `MUSH2D_EXPORT` registra cada atributo. A herança de C++ é respeitada: um `Slime` mostra os atributos do `GameObject` e da classe-base dele.
2. **Registro automático.** Nenhuma lista à mão: toda classe em `src/game/objects/` é registrada só por existir.
3. **Prefabs** `.prefab.json` com tipo, atributos, extras, sprite, animações por estado, collider, componentes, filhos e herança de prefab.
4. **LDtk → prefab.** Uma entidade do LDtk cria o prefab de mesmo nome. Os campos da entidade sobrescrevem atributos de mesmo nome.
5. **Atlas e animações pela pasta**, sem `GameAssets.cpp` à mão.
6. **Inspetor em árvore** no F1, com edição ao vivo e Salvar no prefab, gravando só o que difere da base.
7. **Hot reload de prefabs:** salvar um `.prefab.json` recarrega as salas carregadas.
8. **`IkigaiGame`:** o jogo padrão, no lugar do `ShowcaseGame`. A vitrine continua como sala de testes.
9. **Esquema exportado.** A engine grava `prefab_schema.json` com todos os tipos e atributos, para a IDE futura e para validar prefabs.
10. **Os objetos atuais migram para o novo fluxo:** Slime, Door, Lever, Item, Checkpoint, Critter, Prop e Crystal. O player migra só os atributos de movimento. Isso prova o sistema no uso real.

## Não-objetivos

- Editor de animações com prévia (fase seguinte).
- Criar objetos do zero dentro do inspetor, sem um prefab-base.
- Undo/redo no inspetor.
- IDE e fork do LDtk.
- Scripts.

## Anti-objetivos

- Mudar como o jogo se comporta ou aparece. A migração é transparente: mesma captura de tela, mesmos testes de jogabilidade.
- Exigir C++ avançado para declarar atributos. Uma classe nova tem de ser tão simples quanto o exemplo da seção "Reflexão".
- Prefabs que quebram em silêncio. Um atributo desconhecido, um tipo errado ou um prefab-base inexistente geram um aviso claro com arquivo e campo, e o objeto usa o valor padrão.

## Desenho

### Reflexão (`src/engine/reflect/`)

```cpp
class Slime : public EnemyBase {
    MUSH2D_OBJECT(Slime, EnemyBase)            // registra a classe e a base
public:
    MUSH2D_EXPORT(float, speed, 1.0f)          // membro normal + entrada na tabela
    MUSH2D_EXPORT(float, patrolDistance, 2.0f)
    MUSH2D_EXPORT(Element, weakness, Element::Fire)

    void onStep(Scene2D& scene, float dt) override { transform.position.x += speed * dt; }
};
```

- `TypeInfo`: nome, base, função de criação (`std::unique_ptr<GameObject>()`) e lista de `PropertyInfo`.
- `PropertyInfo`: nome, tipo, valor padrão e acesso ao membro (ponteiro para membro, sem `offsetof`).
- **Tipos suportados:** `float`, `int`, `bool`, `std::string`, `glm::vec2`, `Color` (`glm::vec4`), enums registrados com `MUSH2D_ENUM(Element, Fire, Ice, Combined, None)`, `PrefabRef` (nome de prefab), `EntityRef` (iid do LDtk) e `AssetRef` (atlas, frame ou animação).
- **Tipo novo:** um conversor JSON ↔ valor e um widget de inspetor, num único lugar.
- **Componentes** usam o mesmo mecanismo: `MUSH2D_COMPONENT(LightComponent)` + `MUSH2D_EXPORT`.
- O **`GameObject`** exporta os próprios dados-base: nome, posição, escala, rotação, `visible`, `updateOrder`, `persistent`, sprite (atlas, frame, cor, camada, profundidade) e collider (tamanho, offset, trigger). Com isso, o inspetor os edita como qualquer atributo.
- **Registro automático:** cada objeto vive num `.hpp`. O CMake gera, a partir de um `glob` de `src/game/objects/*.hpp`, um `all_objects.cpp` que inclui todos os headers. Isso garante que os registros estáticos (`inline static`) existam no executável. Nenhuma lista à mão.

### Dependências dos objetos

Os objetos passam a ter **construtor sem argumentos**, porque o prefab é quem os cria.
- O que hoje vem pelo construtor (`WorldState&`, `iid`, `doorIid`) passa a vir de **serviços da cena**: `scene.service<WorldState>()`, registrado pelo jogo, além de campos que o loader preenche (`entityIid`) e de atributos (`door`, do tipo `EntityRef`).
- `GameObject` ganha `std::string entityIid` (o iid do LDtk, vazio se o objeto nasceu fora do LDtk) e `std::string prefab` (o prefab de origem).

### Prefab (`assets/prefabs/<nome>.prefab.json`)

```json
{
  "version": 1,
  "base": "enemy_base",
  "type": "Slime",
  "props": { "speed": 2.5, "weakness": "Fire" },
  "extra": { "lootChance": 0.3 },
  "sprite": { "atlas": "kaa_animals", "animation": "fox_idle", "layer": 1 },
  "collider": { "size": [0.8, 0.8] },
  "animations": { "idle": "slime_idle", "walk": "slime_walk", "hurt": "slime_hit" },
  "components": [ { "type": "PersistentComponent", "props": { "policy": "Respawn" } } ],
  "children": [
    { "name": "Glow", "prefab": "glow_small", "position": [0, 0.5] },
    { "name": "Hitbox", "type": "GameObject", "collider": { "size": [1, 0.6], "isTrigger": true } }
  ]
}
```

- **Ordem de aplicação:** padrões do C++ → prefab-base (recursivo) → este prefab → campos da entidade do LDtk → edições ao vivo.
- **Herança:**
  - `base` mescla `props`, `extra`, `sprite`, `collider` e `animations` chave por chave.
  - `components` e `children` são mesclados por `type` e `name`, respectivamente.
  - Um filho ou componente com `"remove": true` remove o item herdado.
  - Ciclo de herança é recusado com aviso.
- **Filhos** viram `GameObject`s filhos (o `setParent` que já existe), com `position` local.
- **`animations`** liga um nome de estado ("idle", "walk") a uma animação do atlas. A máquina de estados da D2 vai usar exatamente isso.
- **`version`** permite migrar o formato no futuro sem quebrar prefabs antigos.

### LDtk → prefab

- O identificador da entidade, convertido para `snake_case`, aponta para o prefab: `Slime` → `assets/prefabs/slime.prefab.json`. Um campo `prefab` (texto) na entidade escolhe outro prefab explicitamente.
- Campos da entidade com o mesmo nome de um atributo exportado sobrescrevem o valor, com conversão de tipo. Campos sem atributo correspondente vão para `extra`.
- **Entidades especiais** continuam com fábrica em código, por não serem objetos de jogo: `Terrain`, `Light`, `PlayerStart` e as regiões. Todas as outras usam prefab. As fábricas do `ShowcaseGame` para Slime, Item, Door, Lever, Checkpoint, Critter, Prop e Crystal somem.
- Uma entidade sem prefab e sem fábrica gera aviso e é ignorada, como hoje.

### Atlas e animações pela pasta

- Na abertura, o `AssetRegistry` (engine) varre `assets/**/*.json` com formato de atlas do Aseprite (que tenha `frames` e `meta.image`). O atlas é registrado pelo nome do arquivo (`kaa_animals`), as animações pelas `frameTags` e os frames pelos nomes.
- `AssetRef` aponta para `"atlas/frame"` ou `"atlas/animação"`.
- `GameAtlasId` e `GameSpriteId` continuam como **atalhos** para o código antigo (Sandbox e Test), resolvidos pelo nome. `GameAssets.cpp` deixa de listar atlas à mão.

### Inspetor (F1, aba "Cena")

- **Árvore:** as raízes da cena, agrupadas por sala, depois os filhos e depois os componentes. Clicar seleciona. O objeto selecionado ganha um contorno na tela.
- **Painel do selecionado:**
  - nome, prefab de origem, `entityIid`;
  - os atributos por classe (GameObject → EnemyBase → Slime), cada tipo com o widget certo;
  - os componentes com os atributos deles;
  - as variáveis extras.

  Toda edição vale na hora.
- **"Salvar no prefab"** grava no `.prefab.json` de origem **só os valores que diferem do prefab-base**. Isso resolve a pendência de "salvar grava valores que ninguém editou".
- **"Salvar como novo prefab"** cria `<nome>.prefab.json` com `base` apontando para o prefab atual e grava só a diferença.
- **Filtro** por nome ou tag. Opção de **pausar o jogo** enquanto o painel está aberto.

### Hot reload de prefabs (Debug)

- O `FileWatcher` passa a observar também `assets/prefabs/`.
- Uma mudança em um prefab recarrega as salas carregadas, mantendo o `WorldState`, como o hot reload do LDtk faz.

### `IkigaiGame`

- `ShowcaseGame` passa a se chamar `IkigaiGame` (arquivos e classe), e o `main.cpp` usa o `IkigaiGame`.
- O mundo continua sendo o `showcase.ldtk` até a D12 criar o mapa da demo. As cenas Sandbox e Test continuam acessíveis por TAB.

### Esquema exportado

- Em Debug, a engine grava `assets/prefabs/prefab_schema.json` ao abrir. O arquivo lista os tipos, as bases, os atributos (nome, tipo, padrão) e os enums.
- O loader de prefabs valida contra o registro.
- A IDE futura e o fork do LDtk podem ler o mesmo arquivo.

## Como cresce até o jogo final

- **Tipos de atributo novos** (curva, gradiente, lista): um conversor e um widget novos, sem mexer nos objetos.
- **Editor de animações** e **IDE:** leem e escrevem os mesmos `.prefab.json` e usam o `prefab_schema.json`. Nada no runtime muda.
- **Máquina de estados (D2) e IA (D4):** os estados e as transições entram no prefab como um bloco novo, `"states"`, sem mudar o loader.
- **Combate (D3):** hitbox e hurtbox são filhos com componentes; resistências e fraquezas são atributos.
- **NPCs (D7):** o diálogo é um atributo `AssetRef`; personalidade e relação inicial são atributos.
- **Migração de formato:** `version` mais funções de migração por versão.
- **Scripts (se vierem):** um componente `Script` com um atributo `AssetRef` para o arquivo.

## Testes (`mush2d_tests`)

- `reflect_registers_class_and_props`: uma classe de teste com `MUSH2D_EXPORT` de cada tipo. O registro tem nome, base, tipos e padrões corretos, e um atributo herdado aparece pela base.
- `prefab_applies_props_extra_sprite_collider`.
- `prefab_inheritance_merge_and_remove`: base → filho, com `children` e `components` mesclados por nome e tipo e `remove` funcionando.
- `prefab_inheritance_cycle_rejected`.
- `prefab_unknown_prop_warns_and_keeps_default`.
- `prefab_children_become_scene_children`.
- `ldtk_entity_fields_override_prefab_props`: inclui campo sem atributo indo para `extra` e `EntityRef`.
- `asset_registry_scans_folder`: encontra os atlas, as animações e os frames de uma pasta de teste.
- `inspector_save_writes_only_diff`: altera um atributo e "salva". O JSON resultante só contém aquele atributo e o `base`. A parte de serialização é testável sem GPU.
- `schema_export_lists_types`.

## Critérios de pronto

1. Slime, Door, Lever, Item, Checkpoint, Critter, Prop e Crystal nascem de prefabs. As fábricas deles somem do código do jogo.
2. A vitrine e as salas de teste ficam visual e funcionalmente iguais, na captura de tela e nos testes existentes.
3. Um inimigo novo ("slime rápido") é criado só com um `.prefab.json` herdando do slime, colocado no LDtk, sem recompilar.
4. No F1, a árvore mostra os objetos. Editar a velocidade de um slime funciona ao vivo, e "Salvar no prefab" grava só a diferença.
5. Salvar um `.prefab.json` atualiza o jogo sozinho (Debug).
6. Testes passando, zero erros de validação e build sem warnings novos.

## Abordagens consideradas

1. **Registro próprio com macro + JSON** ← escolhido pelo autor.
2. Biblioteca de reflexão pronta (`refl-cpp`, `rttr`, `meta` do EnTT): mais pesada, mais uma dependência e sintaxe mais difícil para um iniciante em C++.
3. Gerar C++ a partir dos prefabs: um passo extra no build e código gerado misturado ao código escrito à mão.

## Decisões do Claude (após revisão; autor pode corrigir)

1. **Cristal e prop viram objetos de verdade.**
   - O cristal vira a classe `CrystalObject`: sprite com emissão, mais um **filho halo** desenhado no passe aditivo.
   - Para isso, o `SpriteData` ganha `bool additive`, e o `appendSceneSprites` liga a flag de aditivo.
   - A lista `crystals` e o desenho manual saem do jogo.
   - O prop vira um prefab do tipo `GameObject`. O frame vem do campo do LDtk.
   - O `GameObject` exporta, além dos dados-base, os atalhos `spriteAtlas`, `spriteFrame`, `flipX` e `depth`, para que campos do LDtk com esses nomes funcionem.
2. **Os objetos são reescritos com os atributos como `MUSH2D_EXPORT`.**
   - Os nomes e valores canônicos são os **campos atuais** dos `Config` (o exemplo `speed` do documento é só ilustrativo). Os `Config` dos objetos migrados somem.
   - Os `Spawn::*` das cenas Sandbox e Test passam a criar o objeto e ajustar os atributos diretamente.
   - O comportamento é preservado pelos testes existentes.
3. **Animação a partir do prefab.**
   - Um componente de engine, o `SpriteAnimator`, toca a animação do estado atual usando o mapa `animations` do prefab. O estado inicial é `"idle"`, e o código pode mudar o estado com `setState`.
   - O posicionamento pelo pivô (`PivotSprite`) passa para dentro desse componente.
   - O `CritterObject` vira um prefab `GameObject` + `SpriteAnimator`, com a animação vinda do campo do LDtk.
   - O player mantém a própria lógica de animação até a D2.
4. **Registro.**
   - O registro de tipos é um `static` local de função (sem problema de ordem de inicialização).
   - Os objetos do jogo e o `all_objects.cpp` gerado formam uma biblioteca `mush2d_game`, ligada pelo executável e pelos testes.
   - O arquivo gerado define `void mush2dRegisterGameTypes()`, chamada explicitamente pelo jogo e pelos testes. Isso evita que o linker descarte os registros da biblioteca estática.
   - As classes só de teste se registram pelas mesmas macros, dentro de `tests.cpp`.
5. **Serviços.** É um registro genérico por tipo na `Scene2D`: `scene.setService<T>(T*)` e `scene.service<T>()`, que retorna `nullptr` se o serviço não estiver registrado. O jogo registra `WorldState`, `World` e `GameAssets`. O hot reload de prefab mantém o `WorldState`, como o hot reload do LDtk.
6. **Componentes.** Têm construtor sem argumentos e são criados pelo nome (`MUSH2D_COMPONENT`). A `policy` do `PersistentComponent` vira atributo.
7. **Salvar no prefab.**
   - A diferença é calculada contra o resultado da cadeia `base`, ou contra os padrões do C++ quando não há base.
   - Grava sempre no prefab de origem do objeto, o arquivo-folha.
   - Valores vindos de **campos do LDtk não são salvos**. O inspetor marca esses atributos como "do LDtk", e editar um deles avisa que o valor deve ser mudado no LDtk.
8. **Hot reload.** Edições ao vivo que não foram salvas se perdem quando um prefab é recarregado; isso fica documentado. O estado persistente (`WorldState`) se mantém.
9. **Renomear `ShowcaseGame` para `IkigaiGame`.** É um rename simples. Também atualiza as referências em `main.cpp`, nos testes, no README e no `GUIA.md`.
10. **Mesma aparência.** A comparação é por captura `MUSH2D_SCREENSHOT` antes e depois. Diferenças só podem aparecer onde há cintilação (luzes e cristais), como nas fases anteriores.

## Deixado para o construtor

- A implementação exata das macros (por exemplo, `inline static` mais lambda de registro).
- Detalhes de layout do inspetor ImGui.
- Os nomes internos das classes de registro.
