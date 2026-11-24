# world

`src/engine/world/` é o mundo dos objetos: a cena, os objetos e componentes, a colisão, a física de andar, o LDtk, o mundo contínuo, a memória do mundo, as regiões e os prefabs.

## O que faz

- Guarda todas as instâncias numa `Scene2D` e roda os eventos delas.
- Lê o `.ldtk`, transforma cada sala em tiles, linhas de colisão e objetos (`LevelLoader`), e carrega e descarrega salas conforme o player anda (`World`).
- Lembra o estado das entidades entre salas (`WorldState`).
- Move personagens sobre o cenário com rampas e plataformas (`CharacterController`).
- Cria objetos a partir de prefabs (`PrefabLibrary`) e salva edições do inspetor (`PrefabSave`).

## Tipos principais

### Objetos e cena

| Tipo | Papel |
|---|---|
| `GameObject` | Base de todo objeto: `id`, `name`, `active` (false = pausado), `visible`, `updateOrder`, `persistent`, `entityIid`, `prefab`, `transform`, `sprite`, `collider`, `health`, `trigger`, tags, `extra()`. Eventos: `onCreate`, `onPrefabApplied`, `onStep`, `onLateStep`, `onTriggerEnter`, `onTriggerExit`, `onCollision`, `onDestroy`. |
| `GameObjectId` | Id com geração: um id velho (objeto destruído, vaga reusada) não acha o objeto novo. `InvalidGameObjectId` = nenhum. |
| `Transform2D` | `position`, `scale`, `rotation` (radianos). Relativo ao pai quando o objeto tem pai. |
| `SpriteData` | Atlas, frame, cor, `layer`, `orderInLayer`, `depth` (0 = gameplay, > 0 fundo com névoa, < 0 frente), `additive`. |
| `ColliderData` | `size`, `offset`, `isTrigger`, `enabled`. |
| `HealthData`, `TriggerData` | Vida (`current`, `max`) e dados de trigger (tipo, `consumed`, alvo de teleporte). |
| `Component` | Comportamento anexável: `addComponent<T>()`, `getComponent<T>()`. Eventos `onAttach`, `onStep` (logo depois do `onStep` do dono), `onDetach`. Atributo `enabled`. |
| `Scene2D` | A cena. `createObject<T>()`, `destroy(id)`, `find(id)`, `findObjectAs<T>(id)`, `findFirstObjectOfType<T>()`, `findAllObjectsOfType<T>()`, `findObjectsByTag(tag)`, `setParent(filho, pai)`, `forEach(fn)`, `step(dt)`, `lateStep(dt)`, `events()`, `service<T>()` / `setService<T>()`. Criar e destruir valem no fim da etapa (`flush`). |
| `EventBus` | Eventos tipados e adiados: `subscribe<E>(fn)` devolve uma `Subscription` (guarde-a), `emit(E{...})`. Veja [Comunicação](../fazendo-o-jogo/comunicacao.md). |

### Colisão e movimento

| Tipo | Papel |
|---|---|
| `Collision2D` (`Collision.hpp`) | Colisão entre caixas de objetos: `buildAABB`, `intersects`, `queryAABB(scene, caixa)`, `solidBoxes(...)`. |
| `SpatialGrid` | Grade que acha rápido quais caixas se sobrepõem a uma caixa. |
| `CollisionWorld` | Linhas de colisão do cenário por sala, geradas do IntGrid `Collision` e das entidades `Terrain`. `query(caixa, out)`. |
| `CharacterController` | `move(body, velocity, dt, query, wasGrounded, jumped)` anda e resolve colisões: chão até 50°, segue rampas, gruda no chão até 0,3 u abaixo, desliza no íngreme. `requestDropThrough` (↓ + pulo em plataforma), `depenetrate` (empurra para fora do chão novo no hot reload, até 2 u). |
| `TerrainMesh` | Gera o visual do terreno livre (preenchimento, borda e lados) como sprites comuns. |

### LDtk, salas e mundo

| Tipo | Papel |
|---|---|
| `Ldtk` | Lê o `.ldtk`: `loadLdtkProject`, salas, camadas, tiles, entidades. Leitores de campo: `numberField`, `stringField`, `colorField` (o valor padrão volta quando o campo falta ou tem outro tipo). Os outros campos ficam em `entity.fields` (JSON). `ldtkToWorld` converte pixels em unidades (Y para cima). |
| `LevelLoader` | `loadRoom` (tiles viram sprites estáticos, colisão vira linhas), `spawnEntity` (prefab ou fábrica), `unloadRoom`, `registerEntity(nome, fábrica)`. |
| `World` | O mundo contínuo: mantém a sala atual e as vizinhas carregadas, descarrega as distantes, faz os respawns (`checkRespawns`) e a morte (`onPlayerDeath`: a sala volta ao estado de quando o player entrou). |
| `WorldState` | Memória do mundo por id do LDtk: `isGone`, `markGone`, `clearGone`, `get(iid, chave, padrão)`, `set(iid, chave, valor)`, `snapshot`, `restore`. Base do save futuro. |
| `PersistentComponent` | Política (`None`, `Permanent`, `Respawn`) e `respawnDistance` de um objeto vindo do LDtk. |
| `Region`, `RegionSet` | Regiões (`forest`, `cave`): fundo em parallax e look, com transição suave (`lerpSettings`). Arquivos `assets/regions/<nome>.region.json`. |

### Prefabs e animação

| Tipo | Papel |
|---|---|
| `PrefabLibrary` | Carrega e mescla prefabs (`resolved`), cria objetos (`instantiate(scene, nome, overrides, entityIid)`), `clear()` no hot reload. Serviço da cena. |
| `PrefabSave` | `diffAgainstBase`, `savePrefab`, usados pelo inspetor para "Salvar no prefab" e "Salvar como novo prefab". |
| `SpriteAnimator` | Componente que toca a animação do estado atual (`setState("walk")`), usando o mapa `animations` do prefab, com o pivô do frame na posição do objeto. |

## Casos de uso

### Criar um objeto em código

```cpp
SlimeEnemy& s = scene.createObject<SlimeEnemy>();
s.transform.position = {10.0f, -4.0f};
```

Para criar a partir de um prefab (com os valores dele): `scene.service<PrefabLibrary>()->instantiate(scene, "slime_fast")`.

### Fazer um objeto filho seguir o pai

`scene.setParent(filho.id, pai.id);`. A posição do filho passa a ser relativa ao pai.

### Guardar um valor que sobrevive à troca de sala

```cpp
WorldState* ws = scene.service<WorldState>();
ws->set(entityIid, "baú_aberto", 1.0);
bool aberto = ws->get(entityIid, "baú_aberto", 0.0) != 0.0;
```

### Um componente reaproveitável

```cpp
struct Flutuar : Component {
    MUSH2D_COMPONENT(Flutuar)
    MUSH2D_EXPORT(float, amplitude, 0.2f)
    float t = 0.0f;
    void onStep(GameObject& o, Scene2D&, float dt) override { t += dt; o.transform.position.y += std::sin(t * 3.0f) * amplitude * dt; }
};
```

Com `MUSH2D_COMPONENT`, ele pode ser listado em `"components"` de um prefab.

### Ler um campo novo de uma entidade numa fábrica

`float raio = numberField(e, "raio", 4.0f);` dentro de uma fábrica registrada com `levelLoader.registerEntity("MinhaEntidade", ...)`. Para entidades com prefab, não precisa: um campo com o nome do atributo já o preenche.

## Arquivos

`src/engine/world/`: `GameObject.hpp`, `ObjectData.hpp`, `Component.hpp`, `Scene2D.*`, `EventBus.*`, `Collision.*`, `CollisionTypes.hpp`, `SpatialGrid.*`, `CollisionWorld.*`, `CharacterController.*`, `TerrainMesh.*`, `Ldtk.*`, `LevelLoader.*`, `World.*`, `WorldState.*`, `PersistentComponent.hpp`, `Region.*`, `RegionSet.*`, `Prefab.*`, `PrefabSave.*`, `SpriteAnimator.*`.
