# world

`src/engine/world/` é o mundo dos objetos: a cena, os objetos e componentes, a colisão, a física de andar, o LDtk, o mundo contínuo, a memória do mundo, as regiões e os prefabs.

## O que faz

- Guarda todas as instâncias numa `Scene2D` e roda os eventos delas.
- Lê o `.ldtk`, transforma cada sala em tiles, linhas de colisão e objetos (`LevelLoader`), e carrega e descarrega salas conforme o player anda (`World`).
- Lembra o estado das entidades entre salas (`WorldState`).
- Move personagens sobre o cenário com rampas e plataformas (`CharacterController`).
- Organiza o comportamento em estados com nome (`StateMachine`).
- Cria objetos a partir de prefabs (`PrefabLibrary`) e salva edições do inspetor (`PrefabSave`).
- Dá vida aos sprites: partículas, squash e stretch, piscar, inclinação, eventos por quadro e efeitos reaproveitáveis ligados pelo prefab (veja [Game juice](../fazendo-o-jogo/game-juice.md)).

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
| `Scene2D` | A cena. `createObject<T>()`, `destroy(id)`, `find(id)`, `findObject(id)` (sem os que vão ser destruídos), `findObjectAs<T>(id)`, `findFirstObjectOfType<T>()`, `findAllObjectsOfType<T>()`, `findObjectsByTag(tag)`, `setParent(filho, pai)`, `forEach(fn)`, `step(dt)`, `lateStep(dt)`, `hitstop(segundos)` / `timeScale()` (veja abaixo), `events()`, `service<T>()` / `setService<T>()`. Criar e destruir valem no fim da etapa (`flush`). |
| `StateMachine<Owner>` | Estados com nome (`State<Owner>`: `name`, `enter`, `exit`, `update` devolve o próximo nome ou `nullptr` para ficar). `add`, `start(dono, nome)`, `update(dono, dt)` (no máximo uma transição por chamada), `change(dono, nome)` (forçada: dano, morte), `current()`, `timeInState()`, `onChange(de, para)` (o `start` chama com `de` = `""`, para o animador pegar o primeiro estado). Nome desconhecido: aviso uma vez em stderr, e o estado fica. Um objeto pode ter várias máquinas (`"move"`, `"action"`). `shared<Base, Derivado>(estado)` roda um `State<Base>` numa máquina do derivado. |
| `StateMachineBase` | Vista sem template para ferramentas: `current()`, `timeInState()`. `GameObject::registerStateMachine("move", &maquina)` faz o inspetor F1 mostrar "move: fall (0.32 s)", só leitura, e faz a máquina emitir `StateChanged{object, label, from, to}` a cada mudança (uma máquina já iniciada emite o estado atual ao ser registrada). |
| `IPlayer` (`IPlayer.hpp`) | O que os objetos pedem ao player, seja qual for o tipo dele: `applyInput(input)`, `respawnAt(posição)` (teleporte, sem velocidade nem chão), `downPressedThisFrame()`, `setCheckpoint(posição)`, `respawnAtCheckpoint()`. O player é o objeto com a tag `player`; `dynamic_cast<IPlayer*>(&objeto)` chega nesses métodos. `findPlayer(scene)` devolve o primeiro objeto com a tag, ou `nullptr`. |
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
| `Region`, `RegionSet` | Regiões (`forest`, `cave`): fundo em parallax, look, com transição suave (`lerpSettings`), e música (`music`, pedida ao `Audio` quando a região muda). Arquivos `assets/regions/<nome>.region.json`. |

### Prefabs e animação

| Tipo | Papel |
|---|---|
| `PrefabLibrary` | Carrega e mescla prefabs (`resolved`), cria objetos (`instantiate(scene, nome, overrides, entityIid)`), `clear()` no hot reload. Serviço da cena. |
| `PrefabSave` | `diffAgainstBase`, `savePrefab`, usados pelo inspetor para "Salvar no prefab" e "Salvar como novo prefab". |
| `SpriteAnimator` | Componente que toca a animação do estado atual (`setState("walk")`), usando o mapa `animations` do prefab, com o pivô do frame na posição do objeto. `animationEvents` (animação → {quadro → nome}) emite `AnimationEvent{object, animation, name}` no `scene.events()` cada vez que o quadro entra: a cada volta do loop e também nos quadros pulados por um `dt` grande. |

### Recursos e dano

| Tipo | Papel |
|---|---|
| `Resources` (`Resources.hpp`) | Componente de vida e energias de gelo e fogo. Começa cheio. `health()`, `energy(Element)`, `damage(n)` (ignorado depois da morte), `heal(n)`, `refill()` (tudo ao máximo, no renascimento), `spend(Element, quantia)` (falso e nada gasto sem energia suficiente), `spendBoth(gelo, fogo)` (os dois ou nenhum). Cada energia recarrega `regenPerSecond` (10/s) depois de `regenDelay` (1 s) sem gastar aquele elemento; o `onStep` chama `update(scene, dt)`. Atributos: `maxHealth` 5, `maxIce` 100, `maxFire` 100, `regenPerSecond`, `regenDelay`. |
| Eventos | `HealthChanged{id, health, max}`, `EnergyChanged{id, element, value, max}`, `Died{id}` (vida chegou a 0), no `scene.events()`. |
| `Element` (`Damage.hpp`) | `None`, `Ice`, `Fire` (`MUSH2D_ENUM`). |
| `DamageInfo` | Um golpe: `amount` (1), `direction` (para onde empurra o alvo: do atacante ao alvo, ou a velocidade do projétil), `element` (`None`), `knockback` (0), `source` (quem conta como autor: o atirador de um projétil), `parryable` (verdadeiro), `projectile` (falso). |

### Combate

Guia: [Combate](../fazendo-o-jogo/combate.md).

| Tipo | Papel |
|---|---|
| `CombatWorld` (`Combat.hpp`) | Serviço da cena com as hurtboxes numa `SpatialGrid` própria (a grade de colisão não muda). `add(hurtbox)`, `remove(hurtbox)`, `query(caixa, saída)` (só hurtboxes `enabled` de objetos ativos, testadas com a caixa atual), `size()`. Uma hurtbox por objeto. |
| `Hurtbox` | Componente: `size`, `offset`, `team` (`"enemy"`), `enabled`. Entra no `CombatWorld` ao ser anexada e sai ao ser desanexada; isso cobre `destroy`, descarregamento de sala, `clear()` (morte, hot reload) e `removeComponent`. A caixa é atualizada a cada passo. |
| `Hitbox` | Componente: `size`, `offset`, `damage` (1), `element` (`Ice`), `knockback` (0), `team` (`"player"`), `parryable`, `activeStart` (0,05) e `activeEnd` (0,15) (a janela do golpe para quem o controla). `activate()`, `deactivate()`, `active()`, `source` (vazio = o objeto raiz do dono). Ativa, acerta uma vez por ativação cada hurtbox de outro `team` e emite `HitEvent`. O espelho segue o `flipX` da raiz (`combatBox`, `combatRoot`); um filho espelha em volta dela. A direção vai da raiz ao centro da hurtbox. |
| `Damageable` | Componente: `invulnerableTime` (0,2), `parryStun` (1), `multipliers` (elemento → multiplicador; seção de prefab `"multipliers"`), `guard` (callback, devolve `HitResult`; `Parried` com `parryable` falso vira `Blocked`). Ouve os `HitEvent` do dono. `finalAmount(info)`: inteiro mais próximo, mínimo 1 com multiplicador maior que 0, 0 = imune. |
| `Stunned` | Componente: `start(segundos)` (fica com o maior), `active()`. |
| Eventos | `HitEvent{attacker, target, info}`, `DamageTaken{target, info, finalAmount}`; `enum class HitResult { Hit, Blocked, Parried }`. |
| `Projectile` (`Projectile.hpp`) | Componente: `speed` (10), `range` (8), `team`, `damage` (1), `element` (`Ice`), `parryable`, `size` (0,25 × 0,25). `direction`, `source`, `reflected`, `launch(...)`, `reflect(novaOrigem, novoTime)` (uma vez só), `flying()`. Anda em linha reta pelo `CollisionWorld` (segmentos de mão única deixam passar), quebra na parede ou no primeiro acerto com `move:break` e volta ao pool. |
| `ProjectilePool` | Serviço da cena (`scene` precisa ser preenchido): `spawn(prefab, posição, direção, time, origem)` reaproveita um projétil parado do mesmo prefab ou cria um pelo `PrefabLibrary`. Ids destruídos (sala, `clear()`) são esquecidos no próximo `spawn`. `pooled(prefab)`. |

### Juice

| Tipo | Papel |
|---|---|
| `ParticleEmitter` (`Particles.hpp`) | Partículas na CPU, em coordenadas de mundo. `burst(config, posição, direção)` (direção `-1` espelha os ângulos), `alive()`, `maxParticles` (128; acima disso a mais velha sai). Desenhadas como sprites pelo `appendSceneSprites`: sem `frame`, um pixel branco tingido. Sem luz e sem emissão. |
| `ParticleConfig` | `count`, `speed`, `angle` (graus) e `lifetime` como [min, max], `gravity`, `drag`, `size` [início, fim], `colorStart`/`colorEnd`, `frame` (do atlas do sprite do dono), `additive`, `layer`. `parseParticleConfig(json, config, erro)` lê o formato JSON (`"color": [início, fim]`). |
| `SquashStretch` | Molas em x e y que voltam a {1, 1}: `kick({0.8, 1.25})`, `current()`, `apply(sprite)` (multiplica `drawSize` mantendo a base do sprite; sem acumular). Aplica-se sozinho a cada passo, e o `SpriteAnimator` o aplica depois de cada quadro. `stiffness` 300, `damping` 18. Nunca mexe em `transform.scale`. |
| `Blink` | `start(segundos)` alterna `visible` `rate` vezes por segundo (16) e devolve a visibilidade do começo ao terminar. `active()`. |
| `Flash` (`Effects.hpp`) | `start(cor rgb, segundos, dono)` põe `sprite->flash` = (cor, 1) e baixa a quantidade em linha reta até 0 no fim. Segundos <= 0 não faz nada. `active()`. Usado pela ação de efeito `flash`. |
| `VelocityLean` | Inclina `transform.rotation` pela velocidade horizontal medida pela posição: até `maxDegrees` (4) a partir de `fullSpeed` (5 u/s), suavizado por `smoothing` (12/s); um salto maior que `teleportDistance` (1 u) num passo é ignorado. O `SpriteBatch` gira o sprite em torno do pivô (os pés). |
| `EffectLibrary` (`Effects.hpp`) | Serviço da cena com os efeitos de `assets/effects/<nome>.effect.json`: `scan(pasta)`, `find(nome)` (nome desconhecido avisa uma vez), `reload(arquivo)` (arquivo inválido mantém a versão anterior). Hot reload no Debug. |
| `playEffect` | `playEffect(scene, obj, item, força, posição, direção)`: `item` é um nome de efeito ou uma ação inline (`particles`, `squash`, `blink`, `hitstop`, `shake` e `flash`, estas cinco com `scaleByStrength`, `effect` e `sound`; qualquer uma com `delay`). Cria os componentes que faltam. |
| `EffectTriggers` | Componente da seção `effects` do prefab: `triggers` (gatilho → itens). Ouve `StateChanged` (`state:<label>:<estado>`), `AnimationEvent` (`anim:<nome>`) e `MovementCue` (`move:<nome>`) do dono. O `Damageable` dispara os gatilhos `hit:taken`, `hit:dealt` (força = dano final), `hit:immune`, `hit:blocked` e `hit:parried` (força 1). |
| `MovementCue` | `{object, name, strength, position, direction}`. `emitMovementCue(scene, obj, nome, força, posição)`; `emitMovementCues(scene, obj, anterior, atual, velocidade, maxFallSpeed, turnThreshold = 2)` (`CharacterController.hpp`) emite `landed`, `left_ground` e `turned`. |

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

Um `Item` com `kind` começando por `ability.` faz isso ao ser pego (veja [Item](../objetos.md#item)).

### Gastar energia e tomar dano

```cpp
auto& r = obj.addComponent<Resources>();
if (r.spend(Element::Ice, 30.0f)) { /* lança a estaca */ }
r.damage(1);
auto sub = scene.events().subscribe<Died>([](const Died& e) { /* morte de e.id */ });
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

### Congelar a cena num acerto (hit stop)

`scene.hitstop(0.06f);` (ou a ação de efeito `{"hitstop": 0.06}`). Enquanto dura, `timeScale()` é 0 e `step(dt)` só desconta `dt` do hit stop: objetos, componentes, física, partículas, esperas de efeito e `Flash` ficam parados, e o `lateStep` desse quadro também. Pedidos ficam com o maior, sem somar; `clear()` zera. O tremor da câmera, a interface e a leitura das teclas seguem em tempo real. Depois de um passo congelado, `lastStepFrozen()` é verdadeiro. O `IkiObject::applyInput` usa isso para guardar os apertos (`keepEdges`, em `InputState.hpp`) até um passo lê-los, então o buffer de pulo atravessa o congelamento. Um botão novo no `InputState` precisa entrar no `keepEdges`.

### Dar um ataque a um objeto

No prefab: um filho (ou o próprio objeto) com `Hitbox`, e o alvo com `Hurtbox`, `Damageable` e `Resources`. No código do ataque: `obj.getComponent<Hitbox>()->activate();` no começo do golpe e `deactivate()` no fim. Um tiro: `scene.service<ProjectilePool>()->spawn("dummy_bolt", pos, dir, "enemy", obj.id);`. Detalhes em [Combate](../fazendo-o-jogo/combate.md).

## Arquivos

`src/engine/world/`: `GameObject.hpp`, `GameObjectId.hpp`, `ObjectData.hpp`, `Component.hpp`, `Scene2D.*`, `EventBus.*`, `Collision.*`, `CollisionTypes.hpp`, `SpatialGrid.*`, `CollisionWorld.*`, `CharacterController.*`, `StateMachine.hpp`, `JumpAssist.hpp`, `Resources.*`, `Damage.hpp`, `Combat.*`, `Projectile.*`, `IPlayer.hpp`, `TerrainMesh.*`, `Ldtk.*`, `LevelLoader.*`, `World.*`, `WorldState.*`, `PersistentComponent.hpp`, `Region.*`, `RegionSet.*`, `Prefab.*`, `PrefabSave.*`, `SpriteAnimator.*`, `Particles.*`, `SquashStretch.*`, `Blink.hpp`, `VelocityLean.hpp`, `Effects.*`.
