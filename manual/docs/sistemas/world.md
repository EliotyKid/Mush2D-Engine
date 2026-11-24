# world

`src/engine/world/` é o mundo dos objetos: a cena, os objetos e componentes, a colisão, a física de andar, o LDtk, o mundo contínuo, a memória do mundo, as regiões e os prefabs.

## O que faz

- Guarda todas as instâncias numa `Scene2D` e roda os eventos delas.
- Lê o `.ldtk`, transforma cada sala em tiles, linhas de colisão e objetos (`LevelLoader`), e carrega e descarrega salas conforme o player anda (`World`).
- Lembra o estado das entidades entre salas (`WorldState`).
- Move personagens sobre o cenário com rampas e plataformas (`CharacterController`).
- Organiza o comportamento em estados com nome (`StateMachine`).
- Cria objetos a partir de prefabs (`PrefabLibrary`) e salva edições do inspetor (`PrefabSave`).
- Dá vida aos sprites: partículas, squash e stretch, piscar, inclinação e eventos por quadro (veja [Game juice](../fazendo-o-jogo/game-juice.md)).

## Tipos principais

### Objetos e cena

| Tipo | Papel |
|---|---|
| `GameObject` | Base de todo objeto: `id`, `name`, `active` (false = pausado), `visible`, `updateOrder`, `persistent`, `entityIid`, `prefab`, `transform`, `sprite`, `collider`, `health`, `trigger`, tags, `extra()`. Eventos: `onCreate`, `onPrefabApplied`, `onStep`, `onLateStep`, `onTriggerEnter`, `onTriggerExit` (só entre o player e um trigger, chamados pelo jogo), `onDestroy`. `onCollision` existe mas nada o chama hoje. |
| `GameObjectId` | Id com geração: um id velho (objeto destruído, vaga reusada) não acha o objeto novo. `InvalidGameObjectId` = nenhum. |
| `Transform2D` | `position`, `scale`, `rotation` (radianos). Relativo ao pai quando o objeto tem pai. |
| `SpriteData` | Atlas, frame, cor, `layer`, `orderInLayer`, `depth` (0 = gameplay, > 0 fundo com névoa, < 0 frente), `additive`. |
| `ColliderData` | `size`, `offset`, `isTrigger`, `enabled`. |
| `HealthData`, `TriggerData` | Vida (`current`, `max`) e dados de trigger (tipo, `consumed`, alvo de teleporte). |
| `Component` | Comportamento anexável: `addComponent<T>()`, `getComponent<T>()`. Eventos `onAttach`, `onStep` (logo depois do `onStep` do dono), `onDetach`. Atributo `enabled`. |
| `Scene2D` | A cena. `createObject<T>()`, `destroy(id)`, `find(id)`, `findObjectAs<T>(id)`, `findFirstObjectOfType<T>()`, `findAllObjectsOfType<T>()`, `findObjectsByTag(tag)`, `setParent(filho, pai)`, `forEach(fn)`, `step(dt)`, `lateStep(dt)`, `events()`, `service<T>()` / `setService<T>()`. Criar e destruir valem no fim da etapa (`flush`). |
| `StateMachine<Owner>` | Estados com nome (`State<Owner>`: `name`, `enter`, `exit`, `update` devolve o próximo nome ou `nullptr` para ficar). `add`, `start(dono, nome)`, `update(dono, dt)` (no máximo uma transição por chamada), `change(dono, nome)` (forçada: dano, morte), `current()`, `timeInState()`, `onChange(de, para)` (o `start` chama com `de` = `""`, para o animador pegar o primeiro estado). Nome desconhecido: aviso uma vez em stderr, e o estado fica. Um objeto pode ter várias máquinas (`"move"`, `"action"`). `shared<Base, Derivado>(estado)` roda um `State<Base>` numa máquina do derivado. |
| `StateMachineBase` | Vista sem template para ferramentas: `current()`, `timeInState()`. `GameObject::registerStateMachine("move", &maquina)` faz o inspetor F1 mostrar "move: fall (0.32 s)", só leitura. |
| `EventBus` | Eventos tipados e adiados: `subscribe<E>(fn)` devolve uma `Subscription` (guarde-a), `emit(E{...})`. Veja [Comunicação](../fazendo-o-jogo/comunicacao.md). |

### Colisão e movimento

| Tipo | Papel |
|---|---|
| `Collision2D` (`Collision.hpp`) | Colisão entre caixas de objetos: `buildAABB`, `intersects`, `queryAABB(scene, caixa)`, `solidBoxes(...)`. |
| `SpatialGrid` | Grade que acha rápido quais caixas se sobrepõem a uma caixa. |
| `CollisionWorld` | Linhas de colisão do cenário por sala, geradas do IntGrid `Collision` e das entidades `Terrain`. `query(caixa, out)`. Cada `Segment` tem uma `surface`: `Surface::Default` ou `Surface::IceClimb` (IntGrid `9`, campo `iceClimb` do `Terrain`). |
| `CharacterController` | `move(body, velocity, dt, query, wasGrounded, jumped)` anda e resolve colisões: chão até 50°, segue rampas, gruda no chão até 0,3 u abaixo, desliza no íngreme. `requestDropThrough` (↓ + pulo em plataforma), `depenetrate` (empurra para fora do chão novo no hot reload, até 2 u). `probeWall(body, lado, query)` e `probeCeiling(body, query)` devolvem um `SurfaceHit` (`hit`, `surface`) da parede ou do teto a até 0,05 u, mesmo com o corpo parado; `IceClimb` vence se as duas encostam. |
| `JumpAssist` | Tempo do pulo, sem cena: `coyoteTime` (0,1 s) e `jumpBuffer` (0,12 s). `tick(dt, noChão, apertou)` por quadro (`apertou` = apertado neste quadro, não segurado; no quadro depois do pulo, não passe `noChão` verdadeiro, senão o coyote recarrega), `consumeJump()` (verdadeiro com aperto guardado e chão ou coyote; limpa os dois), `hasBufferedJump`, `clearBuffer`, `startCoyote` (coyote ao sair da parede). |
| `TerrainMesh` | Gera o visual do terreno livre (preenchimento, borda e lados) como sprites comuns. |

### LDtk, salas e mundo

| Tipo | Papel |
|---|---|
| `Ldtk` | Lê o `.ldtk`: `loadLdtkProject`, salas, camadas, tiles, entidades. Leitores de campo: `numberField`, `stringField`, `colorField` (o valor padrão volta quando o campo falta ou tem outro tipo). Os outros campos ficam em `entity.fields` (JSON). `ldtkToWorld` converte pixels em unidades (Y para cima). |
| `LevelLoader` | `loadRoom` (tiles viram sprites estáticos, colisão vira linhas), `spawnEntity` (prefab ou fábrica), `unloadRoom`, `registerEntity(nome, fábrica)`. |
| `World` | O mundo contínuo: mantém a sala atual e as vizinhas carregadas, descarrega as distantes, faz os respawns (`checkRespawns`) e a morte (`onPlayerDeath`: a sala volta ao estado de quando o player entrou). |
| `WorldState` | Memória do mundo por id do LDtk: `isGone`, `markGone`, `clearGone`, `get(iid, chave, padrão)`, `set(iid, chave, valor)`, `snapshot`, `restore`. Flags globais `flag(nome)` e `setFlag(nome, ligado)`, guardadas no iid reservado `"_global"` (`WorldState::GlobalIid`): a morte restaura só os iids da sala, então a flag fica. Base do save futuro. |
| `PersistentComponent` | Política (`None`, `Permanent`, `Respawn`) e `respawnDistance` de um objeto vindo do LDtk. |
| `Region`, `RegionSet` | Regiões (`forest`, `cave`): fundo em parallax e look, com transição suave (`lerpSettings`). Arquivos `assets/regions/<nome>.region.json`. |

### Prefabs e animação

| Tipo | Papel |
|---|---|
| `PrefabLibrary` | Carrega e mescla prefabs (`resolved`), cria objetos (`instantiate(scene, nome, overrides, entityIid)`), `clear()` no hot reload. Serviço da cena. |
| `PrefabSave` | `diffAgainstBase`, `savePrefab`, usados pelo inspetor para "Salvar no prefab" e "Salvar como novo prefab". |
| `SpriteAnimator` | Componente que toca a animação do estado atual (`setState("walk")`), usando o mapa `animations` do prefab, com o pivô do frame na posição do objeto. `animationEvents` (animação → {quadro → nome}) emite `AnimationEvent{object, animation, name}` no `scene.events()` cada vez que o quadro entra: a cada volta do loop e também nos quadros pulados por um `dt` grande. |

### Juice

| Tipo | Papel |
|---|---|
| `ParticleEmitter` (`Particles.hpp`) | Partículas na CPU, em coordenadas de mundo. `burst(config, posição, direção)` (direção `-1` espelha os ângulos), `alive()`, `maxParticles` (128; acima disso a mais velha sai). Desenhadas como sprites pelo `appendSceneSprites`: sem `frame`, um pixel branco tingido. Sem luz e sem emissão. |
| `ParticleConfig` | `count`, `speed`, `angle` (graus) e `lifetime` como [min, max], `gravity`, `drag`, `size` [início, fim], `colorStart`/`colorEnd`, `frame` (do atlas do sprite do dono), `additive`, `layer`. `parseParticleConfig(json, config, erro)` lê o formato JSON (`"color": [início, fim]`). |
| `SquashStretch` | Molas em x e y que voltam a {1, 1}: `kick({0.8, 1.25})`, `current()`, `apply(sprite)` (multiplica `drawSize` mantendo a base do sprite; chame depois do `SpriteAnimator`). `stiffness` 300, `damping` 18. Nunca mexe em `transform.scale`. |
| `Blink` | `start(segundos)` alterna `visible` `rate` vezes por segundo (16) e devolve a visibilidade do começo ao terminar. `active()`. |
| `VelocityLean` | Inclina `transform.rotation` pela velocidade horizontal medida pela posição: até `maxDegrees` (4) a partir de `fullSpeed` (5 u/s). O `SpriteBatch` gira o sprite em torno do pivô (os pés). |

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

### Desbloquear uma habilidade

```cpp
ws->setFlag("ability.ice.stakes", true);
if (ws->flag("ability.ice.stakes")) { /* pode usar */ }
```

### Estados de um objeto

```cpp
struct Idle : State<MeuObjeto> {
    const char* name() const override { return "idle"; }
    const char* update(MeuObjeto& o, float) override { return o.velocity.x != 0.0f ? "run" : nullptr; }
};
// no objeto: StateMachine<MeuObjeto> move;
move.add(std::make_unique<Idle>());
move.add(std::make_unique<Run>());
move.start(*this, "idle");
registerStateMachine("move", &move); // aparece no inspetor F1
// em onStep: move.update(*this, dt);
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

`src/engine/world/`: `GameObject.hpp`, `ObjectData.hpp`, `Component.hpp`, `Scene2D.*`, `EventBus.*`, `Collision.*`, `CollisionTypes.hpp`, `SpatialGrid.*`, `CollisionWorld.*`, `CharacterController.*`, `StateMachine.hpp`, `JumpAssist.hpp`, `TerrainMesh.*`, `Ldtk.*`, `LevelLoader.*`, `World.*`, `WorldState.*`, `PersistentComponent.hpp`, `Region.*`, `RegionSet.*`, `Prefab.*`, `PrefabSave.*`, `SpriteAnimator.*`, `Particles.*`, `SquashStretch.*`, `Blink.hpp`, `VelocityLean.hpp`.
