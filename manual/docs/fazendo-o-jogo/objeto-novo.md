# Objeto novo

Um objeto é uma classe C++ em `examples/iki_demo/src/iki_demo/objects/` mais um prefab em `assets/prefabs/`. A alavanca (`LeverObject.hpp`) é o modelo para quase todo objeto.

## A alavanca, linha por linha

```cpp
#pragma once
```
Garante que este arquivo é incluído só uma vez por compilação.

```cpp
#include "game/assets/GameAssets.hpp"
#include "game/assets/GameSpriteId.hpp"

#include "engine/world/GameObject.hpp"
#include "engine/world/IPlayer.hpp"
#include "engine/world/Scene2D.hpp"
#include "engine/world/WorldState.hpp"
```
"Vou usar coisas definidas nestes arquivos": os sprites do jogo, o player, a base `GameObject`, a cena e a memória do mundo.

```cpp
class LeverObject : public GameObject {
```
Cria o "objeto" `LeverObject`, filho de `GameObject`. Ele herda tudo o que o pai tem: posição, sprite, colisor, eventos.

```cpp
    MUSH2D_OBJECT(LeverObject, GameObject)
public:
    MUSH2D_EXPORT(EntityRef, door, "")
```
`MUSH2D_OBJECT` registra a classe pelo nome: é assim que o prefab (`"type": "LeverObject"`) sabe qual classe criar. `MUSH2D_EXPORT` declara uma variável da instância (`door`, vazia no começo) que o prefab, o campo `door` da entidade no LDtk e o inspetor podem mudar. `EntityRef` é um texto com o id de outra entidade do LDtk: a porta ligada a esta alavanca.

O objeto não tem construtor com argumentos: quem o cria é o prefab. O id desta alavanca no LDtk já vem pronto em `entityIid`, uma variável de todo `GameObject`.

```cpp
    void onCreate(Scene2D& scene) override {
        state = scene.service<WorldState>();
        name = "Lever";
```
Create Event. Primeiro pega a memória do mundo (`WorldState`), que o jogo registrou como **serviço** da cena: ela lembra se a alavanca está ligada mesmo depois de você sair da sala. Depois dá um nome à instância, útil para depurar e para o filtro do inspetor.

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
Escolhe a imagem. Pega a lista de sprites do jogo (`gameAssetsOf`) e usa o sprite registrado como `Teleport`. Hoje é um quadrado provisório. Para usar outro sprite, troque `Teleport` por outro id de `GameSpriteId.hpp`, ou ponha `"sprite": { "atlas": ..., "frame": ... }` no prefab.

```cpp
        collider = ColliderData{.size = {1.0f, 1.0f}, .isTrigger = true};
```
Máscara de colisão de 1 × 1 unidade. `isTrigger = true` faz dela um sensor: não bloqueia ninguém, só avisa quando algo entra ou sai.

```cpp
        refresh();
    }

    void onPrefabApplied(Scene2D&) override { refresh(); }
```
Pinta a alavanca de vermelho ou verde. `onPrefabApplied` roda depois que o prefab e os campos do LDtk foram aplicados, então a cor reflete os valores finais.

```cpp
    void onStep(Scene2D& scene, float) override {
        auto* player = scene.findObjectAs<IPlayer>(playerId);
        if (player && player->downPressedThisFrame()) toggle();
        refresh();
    }
```
Step Event, a cada frame:

1. procura o player pelo id guardado (`findObjectAs` devolve `nullptr` se ele não existir mais);
2. se o player está ali **e** apertou ↓ neste frame, alterna a alavanca;
3. atualiza a cor.

```cpp
    void onTriggerEnter(Scene2D&, GameObject& other) override {
        if (other.hasTag("player")) playerId = other.id;
    }
```
"Algo entrou na minha máscara." `hasTag("player")` pergunta "esse `other` é o player?", seja qual for o tipo dele. Se for, guarda o id dele. No `onStep`, `findObjectAs<IPlayer>` chega na interface `IPlayer`, que tem o `downPressedThisFrame()`.

```cpp
    void onTriggerExit(Scene2D&, GameObject& other) override {
        if (other.id == playerId) playerId = InvalidGameObjectId;
    }
```
"O player saiu": esquece o id. `InvalidGameObjectId` é o "noone" do GameMaker.

```cpp
    void toggle() {
        if (!state) return;
        state->set(entityIid, "on", isOn() ? 0.0 : 1.0);
        if (!door.empty()) state->set(door, "open", state->get(door, "open", 0.0) != 0.0 ? 0.0 : 1.0);
    }
```
Alterna a alavanca **e** a porta. O estado fica no `WorldState`, indexado pelo id do LDtk. Por isso ele sobrevive quando a sala é descarregada, e a porta lê o próprio estado de lá.

```cpp
    bool isOn() const { return state && state->get(entityIid, "on", 0.0) != 0.0; }
```
"Estou ligada?" Lê da memória do mundo, com 0 como padrão.

```cpp
private:
    void refresh() { sprite->color = isOn() ? glm::vec4(0.4f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.4f, 0.4f, 1.0f); }
```
Pinta o sprite de verde se estiver ligada, ou de vermelho se não. A cor é RGBA de 0 a 1 e multiplica a imagem, como o `image_blend`.

```cpp
    WorldState* state = nullptr;
    GameObjectId playerId = InvalidGameObjectId;
};
```
As variáveis da instância, que só a classe enxerga. `state` fica `nullptr` numa cena sem `WorldState` (os testes, por exemplo), e por isso as funções acima testam `state` antes de usar.

**Como a alavanca nasce:** quando a sala carrega, cada entidade `Lever` do LDtk cria o prefab `assets/prefabs/lever.prefab.json`, que diz `"type": "LeverObject"`. O campo `door` da entidade preenche o atributo `door`. Nenhuma linha de código registra a alavanca.

## Casos de uso

### Uma moeda que gira e some ao ser tocada

1. Crie `examples/iki_demo/src/iki_demo/objects/CoinObject.hpp`:

    ```cpp
    #pragma once

    #include "game/objects/Tracked.hpp"
    #include "engine/world/GameObject.hpp"
    #include "engine/world/Scene2D.hpp"
    #include "engine/world/WorldState.hpp"

    class CoinObject : public GameObject {
        MUSH2D_OBJECT(CoinObject, GameObject)
    public:
        MUSH2D_EXPORT(float, spin, 3.0f)

        void onCreate(Scene2D&) override {
            name = "Coin";
            collider = ColliderData{.size = {0.6f, 0.6f}, .isTrigger = true};
        }
        void onStep(Scene2D&, float dt) override { transform.rotation += spin * dt; }
        void onTriggerEnter(Scene2D& scene, GameObject& other) override {
            if (!other.hasTag("player")) return;
            // removeTracked grava a coleta no WorldState (se houver política) e destrói
            if (WorldState* state = scene.service<WorldState>()) removeTracked(scene, *state, *this, state->clock);
            else scene.destroy(id);
        }
    };
    ```

2. Crie `assets/prefabs/coin.prefab.json`:

    ```json
    { "version": 1, "type": "CoinObject",
      "props": { "spin": 4.0 },
      "sprite": { "atlas": "environment", "frame": "teleport_0", "layer": 1 } }
    ```

3. No LDtk, crie a entidade `Coin`. Um campo `spin` nela sobrescreve o valor do prefab.
4. Compile e rode. Nenhuma fábrica: a entidade `Coin` cria o prefab `coin` sozinha.

!!! note "Registro automático"
    Toda classe de `examples/iki_demo/src/iki_demo/objects/*.hpp` é registrada só por existir: o CMake gera um `iki_demo_objects.cpp` que inclui cada um desses headers (`CONFIGURE_DEPENDS`, então o próximo `cmake --build` já acha o arquivo novo). Um objeto fora dessa pasta não é registrado assim.

Para a moeda não voltar depois de pega, dê a ela uma política de persistência (veja [Mundo e salas](mundo-e-salas.md#estado-do-mundo)). Isso só funciona porque ela sai por `removeTracked`: um `scene.destroy(id)` sozinho não grava nada no `WorldState`, e a moeda voltaria toda vez que a sala recarregasse.

!!! warning "Triggers só com o player"
    `onTriggerEnter` / `onTriggerExit` só disparam entre o **player** e um objeto com `isTrigger`. Quem verifica é o jogo (`IkiDemoGame::processTriggerOverlaps`), não a cena. Dois objetos que não são o player não recebem esses eventos.

### Herdar de outro objeto em C++

```cpp
class SlimeBoss : public SlimeEnemy {          // filho do slime
    MUSH2D_OBJECT(SlimeBoss, SlimeEnemy)
public:
    void onCreate(Scene2D& scene) override {
        SlimeEnemy::onCreate(scene);           // = event_inherited()
        // mudanças aqui
    }
};
```

Se a diferença for só de valores (velocidade, cor), não precisa de C++: use um prefab com `"base"` (veja [Prefabs](prefabs.md#uma-variacao-de-um-objeto-sem-recompilar)).

### Um objeto com luz

O `LightComponent` ainda não é exportado para prefabs: adicione-o em C++, como o cristal faz no `onPrefabApplied`:

```cpp
LightComponent* light = getComponent<LightComponent>();
if (!light) light = &addComponent<LightComponent>();
light->light = GpuLight{{0.0f, 0.0f, 0.6f, 3.0f}, {glow, 1.4f}, {0.0f, 0.06f, 2.0f, 0.0f}};
```

O significado de cada número do `GpuLight` está em [render](../sistemas/render.md). Para uma luz parada no cenário, sem objeto, use a entidade `Light` do LDtk (veja [Objetos do jogo](../objetos.md#light)).
