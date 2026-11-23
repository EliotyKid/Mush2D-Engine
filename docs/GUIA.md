# Guia da Mush2D — como a engine funciona e como fazer um jogo nela

Este guia é para quem conhece o GameMaker e sabe pouco de C++. Ele está organizado em camadas. A **Parte 1** dá a visão geral, a **Parte 2** traz o C++ mínimo para ler o código, a **Parte 3** percorre um objeto linha por linha, a **Parte 4** passa por cada sistema e cada arquivo, e a **Parte 5** mostra como criar coisas novas.

> Sobre "cada linha": a engine tem cerca de 8.000 linhas de C++. Explicar todas de uma vez renderia um livro e se perderia rápido. Este guia explica **toda linha que você vai escrever ou mexer para fazer um jogo** (Parte 3) e o **papel de cada arquivo** (Parte 4). Os arquivos internos (GPU, shaders, física) podem ser explicados linha por linha depois, um por vez, quando você quiser mexer neles.

---

## Parte 1 — Visão geral, comparando com o GameMaker

| GameMaker | Mush2D | Onde fica |
|---|---|---|
| **Object** (obj_player) | uma **classe** que herda de `GameObject` | `src/game/objects/*.hpp` |
| **Instance** | um objeto criado na cena com `scene.createObject<T>()` | — |
| **Create Event** | `onCreate(Scene2D& scene)` | dentro da classe |
| **Step Event** | `onStep(Scene2D& scene, float dt)` | dentro da classe |
| **End Step** | `onLateStep(...)` | dentro da classe |
| **Collision Event** (com trigger) | `onTriggerEnter` / `onTriggerExit` | dentro da classe |
| **Destroy Event** | `onDestroy(...)` | dentro da classe |
| `instance_create_layer` | `scene.createObject<MinhaClasse>(...)` | qualquer lugar |
| `instance_destroy` | `scene.destroy(id)` | qualquer lugar |
| `id` da instância | `GameObjectId id` | campo do objeto |
| `x, y` | `transform.position` (em **unidades**; 1 unidade = 32 px) | campo do objeto |
| `image_xscale` | `transform.scale.x` (negativo = espelhado) | campo do objeto |
| `sprite_index` | `sprite` (atlas + nome do frame) | campo do objeto |
| Máscara de colisão | `collider` (tamanho, `isTrigger`) | campo do objeto |
| **Room** | **sala do LDtk** (nível) | `assets/levels/showcase.ldtk` |
| Room Editor | **LDtk** (editor externo) | — |
| Tile layer / Instance layer | camadas de tiles / de entidades do LDtk | — |
| Variáveis de criação da instância | **campos** da entidade no LDtk | — |
| `persistent` | `persistent = true` | campo do objeto |
| Sprite editor | Aseprite/GameMaker → atlas `.png` + `.json` | `assets/kaa/` |
| Variável global | membro do jogo (`ShowcaseGame`) ou `WorldState` | — |
| Luz | entidade `Light` no LDtk, ou `LightComponent` | — |

**A diferença principal**: no GameMaker, o editor faz a ligação "objeto no room → código" sozinho. Aqui você faz essa ligação **uma vez por tipo**, registrando uma "fábrica" (Parte 5). Isso é um dos pontos para deixar a engine mais fácil (Parte 6).

### O que acontece quando o jogo abre

```
main.cpp ──► cria ShowcaseGame (o "jogo") e Engine (a "máquina")
             engine.run(game)
                 │
                 ├─ abre a janela, liga a GPU (Vulkan), o renderer e o input
                 ├─ game.init(...)          ← o jogo carrega assets e a primeira sala
                 └─ loop até fechar a janela, ~60 vezes por segundo:
                       1. lê teclado e mouse
                       2. game.update(dt)   ← tudo do jogo acontece aqui
                       3. a câmera segue o player
                       4. o renderer desenha o frame (luz, névoa, bloom...)
```

Dentro de `game.update` (o "Step" de todo o jogo), o `ShowcaseGame` faz, nesta ordem:

1. teclas globais (TAB troca de cena, F5 recarrega) e hot reload;
2. passa o input para o player;
3. **`scene.step(dt)`**: chama o `onStep` de **todos** os objetos (como o Step Event de todas as instâncias);
4. verifica os triggers, chamando `onTriggerEnter`/`onTriggerExit`;
5. **`scene.lateStep(dt)`**: o End Step;
6. o mundo carrega e descarrega salas conforme o player anda, checa respawns e cuida da névoa e cor por região e do fade de morte;
7. envia sprites e luzes para o renderer.

---

## Parte 2 — C++ mínimo para ler o código

Você só precisa disto para começar:

```cpp
#include "engine/world/GameObject.hpp"   // "usa o código deste outro arquivo" (como importar)

class Slime : public GameObject {         // "Slime é um GameObject" (herança = objeto filho no GameMaker)
public:                                   // o que vem abaixo pode ser usado de fora da classe
    float speed = 2.0f;                   // variável da instância, com valor inicial. "f" = número float

    void onStep(Scene2D& scene, float dt) override {   // "override" = estou substituindo o evento do pai
        transform.position.x += speed * dt;            // dt = segundos desde o frame anterior
    }
private:                                  // só a própria classe enxerga
    int hp = 3;                           // int = número inteiro
};
```

| Sinal | O que significa |
|---|---|
| `;` | fim de uma instrução (obrigatório) |
| `{ }` | bloco: o corpo de uma função, de um `if` ou de uma classe |
| `//` | comentário, ignorado pelo compilador |
| `.` | acessa algo de um objeto: `transform.position` |
| `->` | o mesmo que `.`, mas através de um **ponteiro** (`player->transform`) |
| `&` no tipo (`Scene2D& scene`) | "referência": é o próprio objeto, não uma cópia |
| `*` no tipo (`Player* p`) | ponteiro: um endereço que pode ser `nullptr` (vazio). Sempre teste com `if (p)` |
| `auto` | "o compilador descobre o tipo sozinho" |
| `std::string`, `std::vector<T>` | texto e lista (array dinâmico) |
| `glm::vec2` | um par (x, y) de floats |
| `template<typename T>` | função que funciona para vários tipos. Exemplo: `createObject<Slime>()` |
| `#pragma once` | "inclua este arquivo uma vez só" (primeira linha de todo `.hpp`) |
| `.hpp` / `.cpp` | declaração ("o que existe") / implementação ("como funciona"). Objetos pequenos ficam só no `.hpp` |

**Compilar** transforma o texto em `.exe`. Use estes comandos:

```bash
. .superpowers/sdd/2026-11-19-terreno-rampas/devshell.ps1
```
```bash
cmake --build build/debug
```
```bash
build/debug/Mush2DEngine.exe
```

Se der erro, o compilador mostra `arquivo(linha): error ...`. A linha indicada é onde ele percebeu o problema, que às vezes vem de uma linha acima.

---

## Parte 3 — Um objeto, linha por linha: a alavanca (`LeverObject.hpp`)

Este arquivo é o modelo para quase todo objeto que você vai criar.

```cpp
#pragma once
```
Garante que este arquivo é incluído só uma vez por compilação.

```cpp
#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"
#include "game/objects/PlayerObject.hpp"
#include "engine/world/GameObject.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"
#include <string>
```
"Vou usar coisas definidas nestes arquivos": os sprites do jogo, o player, a base `GameObject`, a cena, a memória do mundo e o tipo texto (`std::string`).

```cpp
class LeverObject : public GameObject {
```
Cria o "objeto" `LeverObject`, que é filho de `GameObject`. Ele herda tudo o que o pai tem: posição, sprite, colisor, eventos.

```cpp
public:
    LeverObject(WorldState& state, std::string iid, std::string doorIid)
        : state(state), iid(std::move(iid)), doorIid(std::move(doorIid)) {}
```
O **construtor**, que roda ao criar a instância, antes do `onCreate`. Ele recebe três coisas:
- `state`: a memória do mundo, que lembra se a alavanca está ligada mesmo depois de você sair da sala;
- `iid`: o id desta alavanca no LDtk;
- `doorIid`: o id da porta ligada a ela.

A linha que começa com `:` guarda os três dentro do objeto. `std::move` só evita uma cópia do texto.

```cpp
    void onCreate(Scene2D& scene) override {
        name = "Lever";
```
Create Event. Dá um nome à instância, útil para depurar.

```cpp
        addTag("trigger");
        addTag("lever");
```
Etiquetas para encontrar objetos depois (`scene.findObjectsByTag("lever")`).

```cpp
        transform.scale = {0.6f, 0.6f};
```
Tamanho do desenho: 0,6 × 0,6 unidade.

```cpp
        sprite = SpriteData{.layer = 1, .orderInLayer = 2};
```
"Este objeto tem sprite." `layer` e `orderInLayer` definem quem fica na frente, como o depth do GameMaker.

```cpp
        if (GameAssets* assets = gameAssetsOf(scene)) {
            const auto& entry = assets->sprites.get(GameSpriteId::Teleport);
            sprite->atlasTextureIndex = entry.atlasTextureIndex;
            sprite->frameName = entry.frameName;
        }
```
Escolhe a imagem. Pega a lista de sprites do jogo (`gameAssetsOf`) e usa o sprite registrado como `Teleport`. Hoje é um quadrado provisório. Para usar outro sprite, troque `Teleport` por outro id de `GameSpriteId.hpp`.

```cpp
        collider = ColliderData{.size = {1.0f, 1.0f}, .isTrigger = true};
```
Máscara de colisão de 1 × 1 unidade. `isTrigger = true` faz dela um sensor: não bloqueia ninguém, só avisa quando algo entra ou sai.

```cpp
        refresh();
    }
```
Chama a função `refresh` (mais abaixo), que pinta a alavanca de vermelho ou verde.

```cpp
    void onStep(Scene2D& scene, float) override {
        auto* player = scene.findObjectAs<PlayerObject>(playerId);
        if (player && player->interactPressed()) toggle();
        refresh();
    }
```
Step Event, a cada frame:
1. procura o player pelo id guardado (`findObjectAs` devolve `nullptr` se ele não existir mais);
2. se o player está ali **e** apertou ↓ neste frame, alterna a alavanca;
3. atualiza a cor.

```cpp
    void onTriggerEnter(Scene2D&, GameObject& other) override {
        if (dynamic_cast<PlayerObject*>(&other)) playerId = other.id;
    }
```
"Algo entrou na minha máscara." `dynamic_cast<PlayerObject*>` pergunta "esse `other` é um player?" e devolve `nullptr` se não for. É o equivalente a `other.object_index == obj_player`. Se for o player, guarda o id dele.

```cpp
    void onTriggerExit(Scene2D&, GameObject& other) override {
        if (other.id == playerId) playerId = InvalidGameObjectId;
    }
```
"O player saiu": esquece o id. `InvalidGameObjectId` é o "noone" do GameMaker.

```cpp
    void toggle() {
        state.set(iid, "on", isOn() ? 0.0 : 1.0);
        if (!doorIid.empty()) state.set(doorIid, "open", state.get(doorIid, "open", 0.0) != 0.0 ? 0.0 : 1.0);
    }
```
Alterna a alavanca **e** a porta. `a ? b : c` significa "se a, então b, senão c". O estado fica no `WorldState`, a memória do mundo indexada pelo id do LDtk. Por isso ele sobrevive quando a sala é descarregada, e a porta lê o próprio estado de lá.

```cpp
    bool isOn() const { return state.get(iid, "on", 0.0) != 0.0; }
```
"Estou ligada?" Lê da memória do mundo, com 0 como padrão. `const` = esta função não altera nada.

```cpp
private:
    void refresh() { sprite->color = isOn() ? glm::vec4(0.4f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f); }
```
Pinta o sprite de verde se estiver ligada, ou de vermelho se não. A cor é RGBA de 0 a 1 e multiplica a imagem, como o `image_blend` do GameMaker.

```cpp
    WorldState& state;
    std::string iid, doorIid;
    GameObjectId playerId = InvalidGameObjectId;
};
```
As variáveis da instância, que só a classe enxerga.

**Como a alavanca nasce:** no `ShowcaseGame.cpp`, dentro de `registerLevelEntities`, existe uma fábrica `"Lever"`. Quando a sala carrega, cada entidade `Lever` do LDtk vira `scene.createObject<LeverObject>(world.state, e.iid, entityRefField(e, "door"))`.

---

## Parte 4 — Cada sistema e cada arquivo

### `src/main.cpp` — o ponto de partida (14 linhas)
Cria o jogo (`ShowcaseGame`) e a engine, e chama `engine.run(game)`. Se algo der errado, imprime o erro. **Para trocar de jogo, você troca esta linha.**

### `src/engine/core/` — o coração
| Arquivo | Papel |
|---|---|
| `IGame.hpp` | O "contrato" que todo jogo cumpre: `init` (começar), `update` (cada frame), `onImGui` (painel F1), `shutdown` (sair). Também define `EngineServices`, a "caixa de ferramentas" que a engine entrega ao jogo: GPU, renderer, materiais, input, câmera e cena. |
| `Engine.hpp/.cpp` | Abre a janela, inicializa tudo, roda o loop (lê input → `game.update` → câmera → desenha) e fecha tudo na ordem certa. Também liga o painel F1 (ImGui), o modo de captura (`MUSH2D_SCREENSHOT`) e o log de desempenho (`MUSH2D_PERF_LOG`). |
| `Paths.hpp/.cpp` | Acha arquivos. `assetPath` procura ao lado do `.exe`. `sourceAssetPath` procura na pasta do projeto (Debug), para que o que você salva vá para o git. |
| `FileWatcher.hpp/.cpp` | Vigia arquivos (o `.ldtk` e os `.region.json`) para o hot reload. |

### `src/engine/platform/` — teclado e mouse
| Arquivo | Papel |
|---|---|
| `InputState.hpp` | Os "botões" que o jogo entende (`moveLeft`, `jump`, `dash`…). Cada botão tem `down` (segurando), `pressed` (apertou agora) e `released` (soltou agora), como `keyboard_check`, `keyboard_check_pressed` e `keyboard_check_released`. |
| `GlfwInput.hpp/.cpp` | Liga teclas físicas a esses botões. **Para mudar a tecla de pulo, é aqui.** |

### `src/engine/world/` — o mundo dos objetos
| Arquivo | Papel |
|---|---|
| `GameObject.hpp` | A classe-base de todo objeto: id, nome, `active`, `visible`, `transform`, `sprite`, `collider`, tags, eventos, componentes e pai/filho. |
| `ObjectData.hpp` | Os "pedaços" de dados: `SpriteData` (atlas, frame, cor, camada, profundidade, parallax), `ColliderData` (tamanho, offset, trigger), `HealthData`, `TriggerData`. |
| `Scene2D.hpp/.cpp` | A **cena**: guarda todas as instâncias. Funções: `createObject`, `destroy`, `find`/`findObjectAs`, `findFirstObjectOfType`, `findObjectsByTag`, `setParent`, `step`, `lateStep` e `events()`. Criar e destruir são **seguros a qualquer momento**: a mudança vale no fim da etapa, como no GameMaker. |
| `Component.hpp` | Um comportamento anexável a qualquer objeto (`addComponent<T>()`), para reaproveitar código sem herança. |
| `EventBus.hpp/.cpp` | Mensagens entre objetos: `scene.events().subscribe<MeuEvento>(...)` e `emit(MeuEvento{...})`. Equivale a um "user event" global. |
| `Collision.hpp/.cpp`, `CollisionTypes.hpp`, `SpatialGrid.*` | Colisão entre **caixas** de objetos (portas, triggers), com uma grade para ser rápida. |
| `CollisionWorld.*` | A colisão do **cenário**: linhas geradas do IntGrid (chão, rampas, plataformas) e das entidades `Terrain`. |
| `CharacterController.*` | A física de andar e pular sobre essas linhas: rampas, deslizar no íngreme, plataforma atravessável, descer com ↓ + pulo. |
| `TerrainMesh.*` | Gera o **visual** do terreno livre (terra e borda de grama) a partir da linha. |
| `Ldtk.*` | Lê o arquivo `.ldtk`: salas, camadas, tiles, entidades e campos. As funções `numberField`, `stringField`, `boolField`, `colorField` e `entityRefField` leem os campos que você cria no LDtk. |
| `LevelLoader.*` | Transforma uma sala do LDtk em coisas do jogo: tiles viram sprites, a colisão vira linhas e cada entidade chama a sua fábrica. |
| `World.*` | O **mundo contínuo**: carrega as salas vizinhas e descarrega as distantes, cuida da morte (a sala volta ao estado de quando você entrou nela) e dos respawns. |
| `WorldState.*` | A **memória do mundo**: o que morreu, o que foi coletado e o que está aberto, indexado pelo id do LDtk. É a base do save futuro. |
| `PersistentComponent.hpp` | Marca um objeto como "lembrado" (permanente ou com respawn). |
| `Region.*`, `RegionSet.*` | **Regiões** (floresta, caverna): o fundo em parallax e o look (névoa, cor, bloom) de cada área, com transição suave entre elas. |

### `src/engine/graphics/` e `src/engine/assets/` — câmera e imagens
| Arquivo | Papel |
|---|---|
| `Camera2D.*` | A câmera: segue um alvo, respeita limites, faz zoom e tem os modos de pixel art. |
| `TextureAtlas.*`, `AtlasLoader.*`, `AtlasTypes.hpp` | Lê os atlas (`.png` + `.json` no formato do Aseprite): frames, animações e pivô. |
| `SpriteCatalog.hpp`, `AnimationCatalog.hpp` | Dão nomes de jogo (`GameSpriteId::Player`) a frames e animações de um atlas. |

### `src/engine/render/` e `shaders/` — o desenho (avançado)
Você não precisa mexer aqui para fazer um jogo. Resumo do caminho de um frame:
1. **`SpriteBatch`** junta todos os sprites do frame numa lista.
2. **G-buffer** (`gbuffer.frag`): desenha a cor, o normal map, a emissão e a profundidade de cada sprite.
3. **Luz** (`light.frag`): calcula quanta luz chega em cada pixel, usando o normal map.
4. **Resolve** (`resolve.frag`): junta cor × luz + emissão e aplica a névoa por profundidade.
5. **Aditivo** (`additive.frag`): brilhos, como os halos dos cristais.
6. **Bloom** (`bloom_down/up.frag`): o "brilho que vaza", estilo Ori.
7. **Composição** (`composite.frag`): exposição, cor, tonemap, fade e o painel F1 por cima.

Os outros arquivos dessa pasta:
- `Bindless` e `Materials`: carregam as texturas (`.png`, `_n`, `_e`, `_m`).
- `SceneRenderer`: comanda as passadas acima.
- `LightComponent`: uma luz presa a um objeto.
- `GpuTimers`: mede o tempo de cada passada.

### `src/engine/gpu/` — Vulkan (bem avançado)
Conversa direta com a placa de vídeo: criação do dispositivo, troca de imagens da tela e memória. Você não precisa tocar nisso para fazer jogos.

### `src/engine/tools/` — o painel F1
`LookFile.*` lê e salva o look. `LookPanel.*` desenha os controles (névoa, bloom, cor, luzes, camadas).

### `src/game/` — **o seu jogo** (onde você trabalha)
| Arquivo | Papel |
|---|---|
| `ShowcaseGame.hpp/.cpp` | O jogo de exemplo. Carrega os assets, **registra as fábricas das entidades do LDtk** (`registerLevelEntities`), trata TAB, F5 e a tecla K, e liga o mundo e as regiões. |
| `objects/*.hpp` | Os objetos: player, slime, porta, alavanca, item, checkpoint, teleporte, parede e animal de decoração (`CritterObject`). |
| `assets/GameAssets.*`, `Game*Id.hpp` | A lista de atlas, sprites e animações do jogo, com nomes. |
| `scenes/*` | As cenas antigas montadas em código (Sandbox, Test). |
| `spawners/Spawn.*` | Funções que criam objetos já configurados (`Spawn::player`, `Spawn::slime`). |

### `assets/` e `tools/`
- `assets/kaa/`: a arte do kaa, já em atlas, com normal maps.
- `assets/levels/showcase.ldtk`: as salas.
- `assets/regions/*.region.json`: o look de cada região.
- `assets/terrain/`: as texturas do terreno livre.
- `tools/*.py`: scripts que geraram arte e conteúdo. `import_kaa.py` importa sprites do GameMaker.

---

## Parte 5 — Como criar coisas

### Uma sala nova
Abra o `.ldtk` no LDtk, crie um nível encostado nos outros, pinte a camada `Collision` (1 = sólido, 2 = plataforma, 3 a 8 = rampas), pinte os tiles e salve. Com o jogo rodando em Debug, ele recarrega sozinho. Ande até a sala nova.

### Um objeto novo (exemplo: uma moeda que gira e some ao ser tocada)
1. Crie `src/game/objects/CoinObject.hpp` seguindo o modelo da alavanca: `onCreate` com sprite e collider trigger, `onStep` girando com `transform.rotation += 3.0f * dt;`, e `onTriggerEnter` com `if (dynamic_cast<PlayerObject*>(&other)) scene.destroy(id);`.
2. No LDtk, crie a entidade `Coin`, com campos se quiser.
3. Em `ShowcaseGame.cpp`, em `registerLevelEntities`, acrescente a fábrica:
   ```cpp
   levelLoader.registerEntity("Coin", [](const LdtkEntity& e, glm::vec2 pos, EngineServices& s) {
       auto& coin = s.scene.createObject<CoinObject>();
       coin.transform.position = pos;
   });
   ```
   e o `#include "game/objects/CoinObject.hpp"` no topo do arquivo.
4. Compile e rode.

### Herdar de outro objeto
```cpp
class SlimeFast : public SlimeEnemy {          // filho do slime
public:
    using SlimeEnemy::SlimeEnemy;              // usa o construtor do pai
    void onCreate(Scene2D& scene) override {
        SlimeEnemy::onCreate(scene);           // = event_inherited()
        // mudanças aqui
    }
};
```

### Comunicação entre objetos
- Direto: `auto* p = scene.findFirstObjectOfType<PlayerObject>(); if (p) { ... }`
- Por tag: `for (GameObject* g : scene.findObjectsByTag("enemy")) { ... }`
- Por evento: crie `struct PlayerHit { int damage; };`, assine com `scene.events().subscribe<PlayerHit>(...)` e dispare com `scene.events().emit(PlayerHit{1})`.

---

## Parte 6 — O que ainda falta para ficar "simples como o GameMaker"

O que já é fácil: montar salas no LDtk, hot reload, o painel de look, criar e destruir com segurança, e eventos parecidos com os do GameMaker.

O que ainda é trabalhoso, com a solução proposta:
1. **Registrar cada objeto à mão** no `ShowcaseGame`. → Registro automático: a classe se registra com uma linha (`MUSH2D_OBJECT(CoinObject, "Coin")`), e os campos do LDtk preenchem as variáveis sozinhos.
2. **Recompilar e reabrir o jogo para testar código.** → Hot reload de código, ou scripts (Lua) para a lógica de gameplay.
3. **Um jogo novo exige escrever um `ShowcaseGame` próprio.** → Um "jogo padrão" da engine que já faz salas, mundo, regiões, player e câmera. Você só escreve os objetos.
4. **Nenhum inspetor ao vivo.** → No painel F1, uma lista de instâncias com as variáveis editáveis, como o debugger do GameMaker.
5. **Sprites e animações registrados em código** (`GameAssets.cpp`). → Ler tudo automaticamente da pasta de atlas, pelo nome.
