# Objetos do jogo

Os objetos ficam em `examples/iki_demo/src/iki_demo/objects/`. Cada entidade do LDtk cria o prefab de mesmo nome em `snake_case` (veja [Prefabs](fazendo-o-jogo/prefabs.md)). Um campo da entidade com o nome de um atributo muda o valor só naquela instância.

A lista completa de classes e atributos, com tipos e padrões, é gerada no Debug em `assets/prefabs/prefab_schema.json`.

## Atributos de todo objeto

Todo objeto herda estes atributos de `GameObject`:

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `name` | String | `""` | Nome da instância (aparece no inspetor). |
| `position` | Vec2 | `[0, 0]` | Posição em unidades (1 u = 32 px). |
| `scale` | Vec2 | `[1, 1]` | Escala; `x` negativo espelha. |
| `rotation` | Float | `0` | Rotação em radianos. |
| `visible` | Bool | `true` | Desenha ou não. |
| `updateOrder` | Int | `0` | Ordem do `onStep`: menor roda antes. |
| `persistent` | Bool | `false` | Sobrevive quando a sala dele é descarregada. |
| `spriteAtlas` | AssetRef | `""` | Atlas do sprite. |
| `spriteFrame` | String | `""` | Frame do atlas. |
| `spriteColor` | Color | `[1, 1, 1, 1]` | Cor que multiplica a imagem. |
| `spriteLayer` | Int | `0` | Camada de desenho. |
| `depth` | Float | `0` | Profundidade: positivo = fundo, negativo = frente. |
| `additive` | Bool | `false` | Desenha na passada aditiva (brilho). |
| `flipX` | Bool | `false` | Espelha o sprite colocado pelo pivô. |
| `colliderSize` | Vec2 | `[1, 1]` | Tamanho da máscara de colisão. |
| `colliderOffset` | Vec2 | `[0, 0]` | Deslocamento da máscara. |
| `isTrigger` | Bool | `false` | A máscara só avisa, não bloqueia. |

E todo objeto feito de uma entidade do LDtk aceita os campos de persistência `respawn`, `persistence` e `respawnDistance` (veja [Mundo e salas](fazendo-o-jogo/mundo-e-salas.md#estado-do-mundo)).

---

## Iki

**O que faz:** o personagem jogável da vitrine (`IkiObject`). Corre com aceleração, pula só com `Z` (coyote time, buffer, altura variável, topo flutuante, queda rápida, correção de quina), leva dano, morre e renasce cheio. Tem a máquina de estados `move` (`idle`, `run`, `jump`, `fall`, `hurt`, `dead`, `wall` agarrado numa parede de gelo, `ceiling` pendurado num teto de gelo), vida e energias (`Resources`) e todo o juice pelo prefab. Tags: `player`.

**Atributos:** os números de movimento e de dano estão na página [Iki: estados e habilidades](fazendo-o-jogo/iki.md#parametros).

**Como nasce:** pela entidade `PlayerStart` (fábrica em código em `IkiDemoGame.cpp`), que instancia `assets/prefabs/iki.prefab.json` uma vez só, com os pés na base da entidade. O Iki é `persistent` e não pertence a nenhuma sala.

**Casos de uso:**

- Ajustar o pulo ao vivo: `F1` → "Cena (F1)" → Iki → `jumpSpeed`, `apexGravityFactor`, `fallGravityFactor`.
- Causar dano em código: emita `HitEvent{atacante, iki->id, DamageInfo{1, {-1.0f, 0.0f}}}` (quantidade, sentido do empurrão); o `Damageable` do Iki tira a vida e chama `hurt`, que é só a reação. No Debug, `J` faz isso.
- Mudar a poeira, o squash ou o piscar: edite os arquivos em `assets/effects/` ou a seção `effects` do prefab.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/IkiObject.hpp/.cpp`, `examples/iki_demo/src/iki_demo/iki/states/`, `assets/prefabs/iki.prefab.json`.

## Player

**O que faz:** o personagem antigo, só nas cenas Sandbox e Test (a vitrine usa o [Iki](#iki)). Anda, pula (com pulo curto ao soltar o botão), dash e golpe, tem checkpoint e pode ser teleportado. Tags: `player`.

**Atributos** (`PlayerObject`):

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `moveSpeed` | Float | `2.5` | Velocidade horizontal (u/s). |
| `dashSpeed` | Float | `6.5` | Velocidade do dash. |
| `dashDuration` | Float | `0.18` | Duração do dash (s). |
| `hitDuration` | Float | `0.35` | Duração do golpe (s). |
| `movementMode` | Enum | `TopDown` | `TopDown` ou `Platformer`. |
| `gravity` | Float | `-30` | Só `Platformer`. Gravidade (u/s²). |
| `jumpSpeed` | Float | `11` | Só `Platformer`. Velocidade inicial do pulo. |
| `jumpCutFactor` | Float | `0.5` | Só `Platformer`. Ao soltar o pulo subindo, `velocity.y` é multiplicado por isto. |
| `maxFallSpeed` | Float | `-18` | Velocidade máxima de queda. |

**Como nasce:** em código, pelas cenas Sandbox e Test (`Spawn::player`), não por prefab.

**Casos de uso:**

- Ajustar a altura do pulo ao vivo: `F1` → "Cena (F1)" → player → `jumpSpeed`. O ápice é `jumpSpeed² / (2 × |gravity|)` unidades.
- Mandar o player para outro lugar em código: `player->teleport(posicao)`, ou `respawnAt(posicao)` pela interface `IPlayer`.
- Saber se o player apertou a ação de interagir neste frame: `interactPressed()` (`IPlayer`).
- Os outros objetos acham o player pela tag `player` e falam com ele pela interface `IPlayer`, nunca pelo tipo `PlayerObject`. Veja [Comunicação](fazendo-o-jogo/comunicacao.md).

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/PlayerObject.hpp`, `examples/iki_demo/src/iki_demo/spawners/Spawn.cpp`.

## Slime

**O que faz:** inimigo que patrulha em volta de onde nasceu, virando em paredes, superfícies íngremes e beiradas. Morre quando o player pula em cima (a morte é registrada no `WorldState`). Tags: `enemy`, `slime`.

**Atributos** (`SlimeEnemy`):

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `moveSpeed` | Float | `1.0` | Velocidade da patrulha. `0` = parado (animação idle). |
| `patrolDistance` | Float | `2.0` | Quanto anda para cada lado do ponto de nascimento. |
| `maxHealth` | Int | `10` | Vida máxima. |
| `gravity` | Float | `-32` | Gravidade. `0` em cenas top-down. |
| `maxFallSpeed` | Float | `-18` | Velocidade máxima de queda. |

**Prefabs:** `slime` (`moveSpeed` 1, `patrolDistance` 2) e `slime_fast` (herda de `slime`: `moveSpeed` 3, `patrolDistance` 3, avermelhado).

**Casos de uso:**

- Um slime parado de sentinela: campo `moveSpeed` = `0` na entidade.
- Um slime que volta depois de morto: campo `respawn` ligado.
- Um slime mais lento num corredor curto: campos `moveSpeed` = `0.5` e `patrolDistance` = `1`.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/SlimeEnemy.hpp`, `assets/prefabs/slime.prefab.json`, `assets/prefabs/slime_fast.prefab.json`.

## Door

**O que faz:** sólida e visível enquanto fechada; aberta, some e deixa passar. O estado `open` fica no `WorldState`, então uma alavanca pode abri-la mesmo com a sala da porta descarregada. Do LDtk, ela toma o tamanho da caixa da entidade. Tag: `door`.

**Atributos:** só os de todo objeto.

**Casos de uso:**

- Uma porta de 1 × 3: estique a caixa da entidade `Door` no LDtk.
- Abrir por código: `state->set(porta.entityIid, "open", 1.0)` no `WorldState`.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/DoorObject.hpp`, `assets/prefabs/door.prefab.json`.

## Lever

**O que faz:** com o player em cima, ↓ alterna a alavanca (`on`) e a porta ligada (`open`) no `WorldState`. Vermelha desligada, verde ligada. Tags: `trigger`, `lever`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `door` | EntityRef | `""` | A porta que ela abre (referência a uma entidade do LDtk). |

**Casos de uso:**

- Ligar alavanca e porta: na entidade `Lever`, campo `door` (Entity ref) apontando para a entidade `Door`.
- Duas alavancas para a mesma porta: as duas com `door` na mesma porta; cada uma inverte o estado.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/LeverObject.hpp`, `assets/prefabs/lever.prefab.json`. Explicada linha por linha em [Objeto novo](fazendo-o-jogo/objeto-novo.md).

## Item

**O que faz:** coletado ao ser tocado pelo player. Ainda não há inventário: coletar só registra o estado (com a política de persistência) no `WorldState`. Tags: `trigger`, `item`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `kind` | String | `""` (prefab: `"coin"`) | Tipo do item. Começando por `ability.` (ex.: `ability.ice.stakes`), pegar o item liga a flag global de mesmo nome no `WorldState` e desbloqueia a habilidade. Fora isso, só informativo. |

**Casos de uso:**

- Uma moeda que nunca volta: campo `persistence` = `permanent`.
- Uma vida que volta depois de um tempo: campo `respawn` ligado.
- Um item de habilidade: `kind` = `ability.ice.stakes`. A flag não volta atrás na morte; o componente da habilidade lê `flag(kind)`.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/ItemObject.hpp`, `assets/prefabs/item.prefab.json`.

## Checkpoint

**O que faz:** ao ser tocado, vira o ponto de reaparecimento do player e registra a hora da ativação no `WorldState`. O último ativado é o ponto de respawn. Tags: `trigger`, `checkpoint`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `oneShot` | Bool | `false` | Só funciona na primeira vez. |

**Casos de uso:**

- Um checkpoint antes de um trecho difícil: ponha a entidade `Checkpoint` no começo do trecho.
- Um checkpoint que não pode ser reativado depois de passar por outro: `oneShot` ligado.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/CheckpointObject.hpp`, `assets/prefabs/checkpoint.prefab.json`.

## Critter

**O que faz:** animal de decoração. Um sprite com `SpriteAnimator`, com os pés na base da caixa da entidade.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `anim` | AssetRef | `""` | Animação "idle" (nome no atlas do sprite). Vazio = a do prefab (`fox_idle`). |

Use `flipX` para virar o animal.

**Casos de uso:** uma raposa olhando para a esquerda: entidade `Critter` com `flipX` ligado.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/CritterObject.hpp`, `assets/prefabs/critter.prefab.json` (atlas `kaa_animals`).

## Prop

**O que faz:** decoração (árvore, arbusto, cipó) com o pivô do frame na base da caixa da entidade. Props na frente (`depth` < 0) usam `foregroundColor`: silhuetas escuras, como em Ori.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `frame` | AssetRef | `""` | Frame do atlas do sprite (`kaa_props`). |
| `foregroundColor` | Color | `[0.32, 0.36, 0.42, 1]` | Cor dos props na frente. |

**Casos de uso:**

- Uma árvore no fundo: `frame` = o frame da árvore, `depth` positivo (fica atrás e recebe névoa).
- Uma silhueta na frente da câmera: `depth` negativo.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/PropObject.hpp`, `assets/prefabs/prop.prefab.json`.

## Crystal

**O que faz:** cristal brilhante: sprite emissivo, um filho `Halo` desenhado na passada aditiva (`crystal_halo`) e uma luz que tremula, tudo na cor da `variant`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `variant` | Int | `0` | `0` ciano, `1` magenta, `2` ciano claro, `3` rosa. |

**Casos de uso:** iluminar uma caverna: espalhe entidades `Crystal` com variantes diferentes. Cada uma traz a própria luz.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/CrystalObject.hpp`, `assets/prefabs/crystal.prefab.json`, `assets/prefabs/crystal_halo.prefab.json`.

## Bonecos de treino

**O que fazem:** alvos para testar o combate, na sala `IceTest`. Cada boneco é um `GameObject` montado só pelo prefab, com estes componentes:

- `Hurtbox` (time `enemy`, 1 × 1,2 u);
- `Damageable` com `invulnerableTime` 0, para a sequência de cortes acertar três vezes seguidas;
- `Resources` com vida 10;
- `TrainingDummy`.

Com vida 0, o boneco **quebra**:

- some e solta estilhaços (gatilho `move:break`, efeito `dummy_break`);
- a `Hurtbox` é desligada e sai da grade de combate;
- depois de `respawnTime` segundos de tempo de jogo, volta com a vida cheia.

Nada vai para o `WorldState`. Cada acerto pisca o boneco de branco (`hit:taken` → `dummy_hit`). O hit stop, o tremor e o som vêm do golpe do Iki.

**Prefabs** (sprites provisórios do atlas `crystals`):

| Prefab | Entidade do LDtk | Base | Multiplicadores | Aparência | Dano da sequência 1, 1, 2 |
|---|---|---|---|---|---|
| `training_dummy` | — | — | nenhum | cristal ciano | 1, 1, 2 |
| `training_dummy_weak` | `TrainingDummyWeak` | `training_dummy` | `{"Ice": 2.0}` | cristal rosa | 2, 2, 4 |
| `training_dummy_resist` | `TrainingDummyResist` | `training_dummy` | `{"Ice": 0.5}` | cristal ciano claro | 1, 1, 1 |
| `training_dummy_shooter` | `TrainingDummyShooter` | `training_dummy_weak` | `{"Ice": 2.0}` (herdado) | cristal magenta | 2, 2, 4 |

O resistente arredonda 0,5 para 1: um golpe com multiplicador maior que 0 tira pelo menos 1 (veja [Combate](fazendo-o-jogo/combate.md#fraquezas-e-resistencias)).

**O atirador** tem também `Stunned` e `DummyShooter`. A cada `shootInterval` segundos, ele atira um `dummy_bolt` no Iki, se o Iki estiver ao alcance: a até `range` u, vivo e na mesma sala do atirador. A sala vizinha fica carregada e pode estar a menos de `range` u pela porta, mas o atirador da `IceTest` não atira no Iki que está na `Showcase`.

- o tiro mira do centro da hurtbox do atirador ao centro da hurtbox do player;
- o atirador vira para o lado do tiro e emite o gatilho `move:shoot` (som e flash);
- o `dummy_bolt` é lento (5 u/s), tira 1 de vida, não tem elemento e é aparável.

O atirador fica quieto quando está atordoado (`Stunned`), quebrado ou inativo, e quando o Iki está fora de alcance. Em todos esses casos, ele recomeça a contagem: o primeiro tiro sai um intervalo inteiro depois que o Iki entra no alcance.

O aparo do Iki reflete o tiro de volta e atordoa o atirador por 1 s (o `parryStun` do Iki). Se um boneco estiver na linha, o tiro refletido acerta ele primeiro.

**Atributos:**

| Componente | Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|---|
| `TrainingDummy` | `respawnTime` | Float | `3` | Segundos quebrado antes de voltar. |
| `DummyShooter` | `shootInterval` | Float | `2.5` | Segundos entre tiros. |
| `DummyShooter` | `range` | Float | `10` | Distância máxima até o player (u). |
| `DummyShooter` | `projectile` | String | `"dummy_bolt"` | Prefab do tiro (precisa de um componente `Projectile`). |

**Na `IceTest`:** o script `tools/add_ice_test_room.py` põe os três bonecos no chão, à direita do buraco, longe do caminho de escalada:

- o fraco e o resistente ficam embaixo da plataforma alta;
- o atirador fica perto da porta para a `Showcase`.

As entidades têm 40 × 40 px.

**Casos de uso:**

- Um boneco que demora a voltar: `respawnTime` = `10` no componente `TrainingDummy` do prefab, ou ao vivo no `F1`. Um campo da entidade no LDtk não serve: ele muda atributos do objeto, e `respawnTime` é de um componente.
- Um alvo imune a gelo: um prefab novo com `"base": "training_dummy"` e `"multipliers": {"Ice": 0}`. Os golpes disparam `hit:immune` (flash cinza) e não tiram vida.
- Treinar o aparo com calma: no `F1`, aumente o `shootInterval` do atirador.
- Um atirador de outro projétil: um prefab com `Projectile` e o nome dele em `projectile`.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/TrainingDummy.hpp`, `examples/iki_demo/src/iki_demo/objects/DummyShooter.hpp`, `assets/prefabs/training_dummy*.prefab.json`, `assets/prefabs/dummy_bolt.prefab.json`, `assets/effects/dummy_hit.effect.json`, `assets/effects/dummy_break.effect.json`.

## Teleport e Wall

Usados só nas cenas antigas montadas em código (Sandbox, Test), sem prefab. São configurados por uma `Config` no construtor.

- **Teleport:** trigger que leva o player para `targetPosition` (opção `oneShot`). Tags: `trigger`, `teleport`.
- **Wall:** caixa sólida de tamanho `size`. Tag: `wall`.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/TeleportObject.hpp`, `examples/iki_demo/src/iki_demo/objects/WallObject.hpp`, `examples/iki_demo/src/iki_demo/scenes/`.

---

## Entidades com fábrica em código

### PlayerStart

Cria o Iki pelo prefab `iki` (veja [Iki](#iki)). Se o player já existe (a sala carregou de novo), não faz nada.

### Light

Uma luz parada no cenário. Não é um objeto da cena: fica guardada por sala e some quando a sala descarrega.

| Campo | Padrão | Efeito |
|---|---|---|
| `color` | `(1, 0.76, 0.48)` | Cor. |
| `intensity` | `2` | Intensidade. |
| `radius` | `6` | Raio (u). |
| `height` | `1.5` | Altura sobre o plano (afeta o normal map). |
| `depth` | `0` | Profundidade. |
| `flicker` | `0` | Tremulação. |

Se o arquivo da região da sala tem valores para essa luz (editados no [painel F1](fazendo-o-jogo/inspetor.md), seção "Luzes"), eles vencem os do LDtk.

**Caso de uso:** uma tocha: entidade `Light` com `color` laranja, `radius` 4 e `flicker` 0,3.

### Terrain

Chão livre em qualquer ângulo. Veja [Terreno](fazendo-o-jogo/terreno.md).
