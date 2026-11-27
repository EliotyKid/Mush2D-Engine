# Iki: estados e habilidades

O Iki é o personagem jogável de Ikigai (`IkiObject`). Ele nasce do prefab `assets/prefabs/iki.prefab.json` pela entidade `PlayerStart` da vitrine. As cenas Sandbox e Test continuam com o `PlayerObject` antigo.

- Tem a tag `player` e implementa `IPlayer`. Portas, alavancas, itens, checkpoints e teletransportes o encontram por isso, sem saber o tipo dele. Veja [Comunicação](comunicacao.md).
- A posição dele (`transform.position`) são **os pés**. O collider (0,6 × 1,6) fica acima dela (`offset` 0,8). Assim, o pivô do quadro, a inclinação e a poeira ficam no chão.
- Ele anda com o `CharacterController`: rampas, plataformas atravessáveis e descer com `↓` + `Z`. Veja [Colisão e rampas](colisao-e-rampas.md).
- Ele pula num slime e o mata, com a mesma regra do `PlayerObject` (`SlimeEnemy::stomp`).

## Controles

O Iki lê as ações com nome do `examples/iki_demo/project.json` (veja [platform](../sistemas/platform.md)): `moveLeft`, `moveRight`, `moveUp`, `moveDown`, `jump`, `attack`, `shoot`, `guard`, `interact` e `dropThrough`. No gamepad, "direcional" vale também para o analógico esquerdo.

| Teclado | Gamepad | Ação |
|---|---|---|
| `←` `→` | direcional `←` `→` | Correr. |
| `Z` | `A` | Pular. Segurar pula mais alto; soltar subindo corta o pulo. |
| `↓` + `Z` numa plataforma atravessável | `↓` + `A` | Descer por ela (ação `dropThrough`). |
| `↓` | `↓` | Interagir com uma alavanca (ação `interact`). |
| `X` | `X` | Corte (sequência de até 3). |
| `C` | `Y` | Estilhaço de gelo. |
| `V` (segurar, no chão) | `R1` (segurar) | Escudo e aparo. |
| `←`/`→` contra uma parede de gelo, no ar | direcional | Agarrar a parede (estacas de gelo). |
| `↑` / `↓` na parede | direcional | Subir / descer. |
| `Z` na parede | `A` | Salto da parede. |
| `↑` sob um teto de gelo, no ar | direcional `↑` | Pendurar no teto. Pendurado: `←`/`→` anda, `↓` ou pulo solta. |
| `J` (só Debug) | | 1 de dano, empurrando para trás. |
| `L` (só Debug) | | Gasta 30 de gelo e 30 de fogo. |

A seta `↑` **não** pula o Iki: ela sobe paredes e agarra tetos de gelo. As estacas só valem com a habilidade desbloqueada (veja [Habilidades](#habilidades)).

## Estados

O movimento é uma **máquina de estados** chamada `"move"`. Em cada momento o Iki está em exatamente um estado. Cada estado é uma classe pequena em `examples/iki_demo/src/iki_demo/iki/states/`, com três partes:

- `enter`: roda ao entrar no estado;
- `update`: roda a cada passo e devolve o nome do próximo estado, ou `nullptr` para ficar;
- `exit`: roda ao sair.

| Estado | Quando | Sai para | Animação |
|---|---|---|---|
| `idle` | No chão, sem direção. Freia com `groundDecel`. | `run`, `jump`, `fall` | `idle` |
| `run` | No chão, com direção. Acelera até `runSpeed`. | `idle`, `jump`, `fall` | `run` |
| `jump` | No ar, subindo. Soltar `Z` corta o pulo uma vez. | `fall`, `idle`, `run`, `wall`, `ceiling` | `jump` |
| `fall` | No ar, descendo. O coyote time ainda deixa pular. | `idle`, `run`, `jump`, `wall`, `ceiling` | `fall` |
| `hurt` | Levou dano. Sem controle por `hurtDuration`, empurrado por `hurtKnockback`. | o estado de movimento certo | `attack_leave` (provisório) |
| `dead` | Vida em 0. Sem controle. Só `respawn()` tira dele. | `idle` (no renascimento) | `fall` (provisório) |
| `wall` | Agarrado a uma parede de gelo. Sem gravidade. | `jump`, `fall`, `ceiling`, `idle`, `run`, `hurt` | `push` (provisório) |
| `ceiling` | Pendurado num teto de gelo. Sem gravidade. | `fall`, `wall`, `idle`, `run`, `hurt` | `balance_back` (provisório) |

- O nome do estado é também a chave da animação no prefab (`animations`). Ao trocar de estado, o Iki chama `SpriteAnimator::setState`.
- A máquina aparece no inspetor F1, na janela "Cena (F1)", com o Iki selecionado: `move: run (0.50 s)`.
- Toda troca de estado emite `StateChanged`, que os efeitos usam (`state:move:wall` amassa o Iki ao agarrar a parede).

## A ordem de cada passo

Cada passo do Iki segue a mesma ordem:

1. Entrada: o jogo chamou `applyInput` antes do passo.
2. `JumpAssist::tick`: conta o coyote time e o buffer de pulo (um `InputBuffer`, contado no tempo do passo: não anda durante o hit stop).
3. `actions().update`: a máquina `"action"` (golpe, tiro, escudo; veja [Combate do Iki](#combate-do-iki)). Ela roda antes da `"move"`, que lê a ação deste passo (velocidade reduzida, gravidade do golpe no ar).
4. `states().update`: o estado atual move o Iki com os métodos de física. Depois, se o estado for `wall`, `ceiling`, `hurt` ou `dead`, a ação volta para `none`.
5. `emitMovementCues`, no fim do `onStep`: avisa `landed`, `left_ground` e `turned`. Os avisos são eventos: os efeitos tocam no fim do passo (no flush), depois dos componentes.
6. Os componentes rodam logo depois do `onStep`. Primeiro os do `onCreate` (`Resources`, `Hurtbox`, `Damageable`), depois os do prefab na ordem dele: `SpriteAnimator`, `SquashStretch`, `VelocityLean` e `Blink`. O `SquashStretch` funciona em qualquer ordem: o `SpriteAnimator` o aplica de novo depois de trocar o quadro.

Os estados usam estes métodos do `IkiObject`, nesta ordem:

| Método | O que faz |
|---|---|
| `runHorizontal(dt, entrada)` | Leva `velocity_.x` até `entrada * runSpeed`, com aceleração ou desaceleração (chão ou ar). Vira o Iki para a entrada. Virar no chão correndo rápido vira o aviso `turned`. |
| `tryJump()` | Se há um `Z` no buffer e o Iki está no chão ou no coyote time, pula e emite `jumped`. Com `↓` numa plataforma atravessável, desce por ela. |
| `applyGravity(dt)` | Gravidade, mais fraca no topo com `Z` segurado e mais forte na queda, até `maxFallSpeed`. |
| `moveAndCollide(dt)` | Move com o `CharacterController`, faz a correção de quina e o pulo no slime. |

## Parâmetros

Todos são atributos exportados: aparecem no inspetor F1, mudam ao vivo e podem ser salvos no prefab.

| Atributo | Padrão | Efeito |
|---|---|---|
| `runSpeed` | `5` | Velocidade máxima correndo (u/s). |
| `groundAccel` / `groundDecel` | `60` / `70` | Aceleração e freio no chão (u/s²). |
| `airAccel` / `airDecel` | `35` / `20` | Aceleração e freio no ar (u/s²). |
| `gravity` | `-32` | Gravidade (u/s²). |
| `jumpSpeed` | `11` | Velocidade inicial do pulo. |
| `jumpCutFactor` | `0.5` | Soltar `Z` subindo multiplica `vy` por isto. |
| `apexThreshold` | `1.5` | Com `vy` entre `-apexThreshold` e `apexThreshold` e `Z` segurado, é o topo do pulo. |
| `apexGravityFactor` | `0.5` | Gravidade no topo (flutua um instante). |
| `fallGravityFactor` | `1.6` | Gravidade descendo (queda rápida). |
| `maxFallSpeed` | `-18` | Velocidade máxima de queda. |
| `coyoteTime` | `0.1` | Segundos em que o pulo ainda vale depois de sair de uma borda. |
| `cornerCorrection` | `0.2` | Subindo, a cabeça que bate na quina de um teto desliza até isto para o lado e continua subindo. |
| `stompBounceFactor` | `0.6` | Pisar num slime faz o Iki quicar a `jumpSpeed` vezes isto. |
| `turnThreshold` | `2` | Virar mais rápido que isto (u/s) emite `move:turned` (a poeira da virada). |
| `wallClimbSpeed` | `3` | Subir e descer a parede (u/s). |
| `wallJumpVelocity` | `[6, 10]` | Salto da parede: x para longe dela, y para cima. |
| `wallJumpLock` | `0.15` | Segundos em que `←`/`→` é ignorado depois do salto da parede (e do `ledgeHop`), para ele não voltar colado. |
| `ledgeHop` | `[2, 7]` | Impulso ao chegar no topo de uma parede de gelo sem nada acima: x para a beirada, y para cima. |
| `ceilingMoveSpeed` | `3` | Andar pendurado no teto (u/s). |
| `climbParticleInterval` | `0.1` | Segundos entre as farpas (`move:climb`) enquanto ele se move na parede ou no teto. |
| `wallGripHalf` | `0.1` | Meia altura da faixa no meio do corpo que agarra a parede (e meia largura para o teto). |
| `ceilingGrabReach` | `0.3` | Distância até um teto de gelo em que `↑` já o agarra (u). |
| `ceilingStickSpeed` | `8` | Velocidade para cima com que ele encosta no teto ao pendurar (u/s). |
| `hurtDuration` | `0.35` | Segundos sem controle após o dano. |
| `hurtKnockback` | `[6, 6]` | Empurrão do dano: x no sentido do golpe, y para cima. |
| `invulnerableTime` | `0.8` | Segundos sem levar dano novo. O Iki copia este valor para o `invulnerableTime` do seu `Damageable` a cada passo (é o `Damageable` que conta o tempo). O dano emite `move:hurt` com este número como força, e o piscar do prefab (`"scaleByStrength": true`) dura o mesmo tempo. |

O buffer de pulo (0,12 s: um `Z` apertado um pouco antes de tocar o chão ainda pula) não é mais um campo do Iki: é o `"buffer"` da ação `"jump"` no `examples/iki_demo/project.json`, ajustável no F1 em "Projeto · Entrada". O Iki lê esse valor a cada passo (`bufferWindow("jump")`).

Os números de juice (squash, inclinação, piscar, partículas) não ficam no Iki: ficam nos componentes do prefab e nos arquivos de efeito. Veja [Game juice](game-juice.md).

## Vida, energia, dano e morte

- O componente `Resources` guarda a vida (5) e as energias de gelo e de fogo (100). Ele recarrega sozinho as energias, 10 por segundo, 1 s depois do último gasto.
- O dano chega pelo `Damageable` do Iki (adicionado no `onCreate`, com uma `Hurtbox` do time `"player"` do tamanho do corpo). Para ferir o Iki, emita um `HitEvent` para ele (a tecla de debug `J` faz isso). O `Damageable` pergunta ao escudo, tira a vida uma vez só e emite `DamageTaken`. Durante `invulnerableTime`, um golpe novo é ignorado.
- `hurt(DamageInfo)` é só a reação ao `DamageTaken`: empurra, entra em `hurt` (ou `dead` com vida 0) e emite `move:hurt` (o piscar). Ele não tira vida.
- Vida em 0: o Iki entra em `dead` e o `Resources` emite `Died`. O `IkiDemoGame` ouve `Died` e chama a morte que já existe: escurece, restaura a sala e leva ao checkpoint.
- No renascimento, o jogo move o Iki (`respawnAt`) e chama `respawn()`: vida e energias cheias, sem invulnerabilidade, sem velocidade, sem pulo guardado, estado `idle`.
- O HUD de teste (canto superior esquerdo) mostra 5 corações, a barra azul de gelo e a laranja de fogo. Ele fica em `examples/iki_demo/src/iki_demo/ui/DebugHud.cpp` e será trocado pela UI do D9.

## Estacas de gelo

Valem numa superfície com a tag `climbable` (no `iki_demo`, `IceClimb`: IntGrid valor 9, ou `Terrain` com `iceClimb`; veja [Colisão e rampas](colisao-e-rampas.md)) e só com a habilidade desbloqueada. Sem ela, o gelo é uma parede ou teto comum.

- **Agarrar a parede:** no ar, segurando a direção de uma parede de gelo, o Iki entra em `wall`. Vale também com `↑` segurado. Ele agarra pelo meio do corpo: encostado numa parede comum com gelo só embaixo, ele não agarra até o meio do corpo chegar ao gelo.
- **Na parede:** `↑` sobe e `↓` desce a `wallClimbSpeed`. Sem direção, fica parado, sem limite de tempo. Soltar a direção da parede não o solta.
- **Salto da parede:** `Z` (ou um `Z` no buffer ao agarrar) dá `wallJumpVelocity`. Durante `wallJumpLock`, a entrada horizontal é ignorada e ele não reagarra a mesma parede.
- **Fim do gelo:**
    - subindo, sem parede acima: `ledgeHop` o põe na beirada;
    - subindo com uma parede comum acima, ou descendo pelo fim do gelo: ele solta (`fall`). Por `coyoteTime`, `Z` ainda dá o salto da parede. Ele só reagarra essa parede depois de soltar e apertar de novo a direção dela;
    - descendo até o chão: `idle`.
- **Teto:** no ar, com `↑` e um teto de gelo a até `ceilingGrabReach` acima do meio do corpo, ele entra em `ceiling` e encosta no teto. `←`/`→` anda a `ceilingMoveSpeed`. `↓` ou `Z` solta (cai, não pula; o `Z` é consumido). Se o meio do corpo sai do gelo, ele solta. Depois de soltar, só reagarra um teto depois de soltar e apertar `↑` de novo.
- **Canto interno:**
    - na parede, só `↑` até um teto de gelo: passa para `ceiling`;
    - no teto, segurando a direção de uma parede de gelo: passa para `wall`;
    - com `↑` e a direção da parede juntos, a parede ganha e o estado não alterna.
- **Dano** na parede ou no teto: solta, empurra e entra em `hurt`. Durante `hurt`, nada reagarra e não há salto da parede (o buffer e o coyote de parede são zerados).
- **Juice:** ao agarrar, o Iki emite `move:grab` nas mãos (parede) ou na cabeça (teto), que toca `ice_stake` (as mãos e a cabeça são calculadas do `collider`: um corpo maior no prefab leva as estacas junto); `state:move:wall` e `state:move:ceiling` tocam um squash; `move:climb` toca `ice_shards` a cada `climbParticleInterval` enquanto ele se move. Travessia nunca gasta energia.

## Combate do Iki

A segunda máquina, `"action"`, cuida do combate de gelo. Ela aparece no F1 ao lado da `"move"` e emite `StateChanged` (`state:action:<estado>`). Os estados ficam em `examples/iki_demo/src/iki_demo/iki/actions/`.

| Estado | Quando | Sai para |
|---|---|---|
| `none` | Sem ação. `V` no chão ergue o escudo, `C` atira, `X` golpeia. | `slash1..3`, `shoot`, `guard` |
| `slash1`, `slash2`, `slash3` | Um corte, por `slashDuration`. | o próximo corte, `none`, `guard`, `shoot` |
| `shoot` | Um passo, logo depois de lançar o estilhaço. | `none` |
| `guard` | `V` segurado no chão. | `none` |
| `guard_break` | O escudo quebrou: sem escudo nem golpe por `guardBreakStun`. | `none` |

- **Regras com a `"move"`:** `wall`, `ceiling`, `hurt` e `dead` levam a ação para `none` e bloqueiam ações novas (o `guard_break` continua durante o `hurt` do golpe que o quebrou). O escudo só sobe em `idle` ou `run`.
- **Sequência (`X`):** `X` começa o corte 1. Um `X` até `comboWindow` depois do fim de um corte começa o próximo, até o 3. Um `X` durante o corte (ou durante o hit stop) fica guardado (`attackBuffered`) e encadeia assim que o corte acaba. Essa fila do "próximo corte" vale pelo resto do corte atual, não por um tempo em segundos, então não é um buffer de toque do projeto (`InputBuffer`). Dano 1, 1 e 2. Cada corte avança `attackStep` no chão; correndo, a velocidade cai para `runSpeed * attackMoveFactor`. A sequência zera com dano, parede, teto, escudo e tiro; pousar não zera. Golpe não gasta energia.
- **No ar:** durante um corte que já está caindo (velocidade vertical 0 ou menos), a gravidade é `gravity * airAttackGravityFactor` e a queda para em `airAttackMaxFall`. Há uma sequência por salto: um corte que começa no ar gasta a do salto, e só tocar o chão, agarrar parede ou teto, ou levar dano a devolve.
- **Hitboxes:** os filhos `Slash1`, `Slash2` e `Slash3` do prefab têm uma `Hitbox` cada, à frente do Iki (o espelho segue o `flipX` dele). O corte liga a hitbox entre os eventos `hit_on` e `hit_off` da animação, se ela tiver `hit_on` em `animationEvents` (só o primeiro `hit_on` de cada corte conta: uma animação que repete não abre outra janela). Senão, entre `activeStart` e `activeEnd` da hitbox (0,05 a 0,15 s). Cada corte acerta cada alvo uma vez. O `knockback` do `Slash3` é o `finisherKnockback` do Iki (o prefab não põe `knockback` nessa hitbox: o valor fica num lugar só, ajustável no F1).
- **Estilhaço (`C`):** custa `shardCost` de gelo e lança o prefab `ice_shard` (do `ProjectilePool`) na direção do olhar, saindo do centro do corpo (encostado numa parede, ele quebra nela). A velocidade (14), o alcance (8) e o dano (1, gelo) são do componente `Projectile` do prefab. Entre tiros há `shootCooldown`. Sem gelo, nada sai e o Iki emite `move:no_energy`. Se o pool não consegue criar o estilhaço, o gelo não é gasto.
- **Escudo (`V`):** com ele, o Iki anda a `runSpeed * guardMoveFactor` e não pula. O `Damageable` do Iki chama o escudo antes de tirar vida:
    - golpe de trás (o empurrão aponta para onde o Iki olha) passa;
    - "de frente" olha só o sinal de x do empurrão; um golpe reto de cima ou de baixo conta como de frente;
    - até `parryWindow` depois de apertar `V`, é **aparo**: não custa gelo, quem atacou (`source`) fica `Stunned` por `parryStun` do `Damageable` e um projétil volta, agora do Iki. A janela conta tempo de cena: o hit stop não a gasta, e cada aperto de `V` a reinicia;
    - depois da janela, ou com `parryable` falso, é **bloqueio**: custa `blockCost` de gelo e empurra o Iki `blockPush` para trás;
    - sem gelo para o bloqueio, o golpe passa e a ação vira `guard_break` (emite `move:guard_break`). Para erguer de novo, solte e aperte `V`.
- **Animações** (provisórias, do `kaa_player`): no mapa `animations`, uma chave `action:<estado>` vale enquanto a ação não é `none` (`"action:slash1": "attack_prepare"`, `"action:slash2": "attack_stay"`, `"action:slash3": "attack_leave"`, `"action:guard": "push"`). Sem a chave, fica a animação da `"move"`. Essas animações ainda não têm `hit_on`: os quadros delas (100 a 143 ms) são longos demais para a janela de 0,05 a 0,15 s.
- **Peso** (seção `effects` do prefab, arquivos em `assets/effects/`): o Iki emite `move:hit` quando um acerto dele tira vida e `move:finisher` quando é o corte 3, no ponto do acerto (o centro da hurtbox do alvo).

| Gatilho | Efeito | O que faz |
|---|---|---|
| `state:action:slash1..3` | `slash_1..3` | Som do corte e um squash. |
| `move:hit` | `slash_hit` | Hit stop 0,05 s, tremor 0,1, som `hit_ice`, cacos. |
| `move:finisher` | `slash_finisher` | Hit stop 0,09 s, tremor 0,18, som mais grave, mais cacos. |
| `state:action:shoot` | `shard_throw` | Som do tiro. O estilhaço toca `shard_break` ao quebrar. |
| `hit:blocked` | `guard_block` | Hit stop 0,04 s, tremor 0,08, som `hit_block`. |
| `hit:parried` | `parry` | Hit stop 0,15 s, tremor 0,25, flash azul, som `parry`, faíscas. |
| `move:guard_break` | `guard_break` | Som e tremor do escudo quebrado. |
| `move:no_energy`, `state:action:guard` | (inline) | Sons `no_energy` e `guard_up`. |
| `hit:taken` | (inline) | Hit stop 0,06 s, tremor 0,15 e flash vermelho no Iki ferido. |

| Atributo | Padrão | Efeito |
|---|---|---|
| `comboWindow` | `0.35` | Segundos depois de um corte em que `X` encadeia o próximo. |
| `slashDuration` | `0.22` | Duração de cada corte. |
| `attackStep` | `0.3` | Avanço de cada corte no chão (u). |
| `attackMoveFactor` | `0.4` | Velocidade no chão durante o corte, em fração de `runSpeed`. |
| `airAttackGravityFactor` | `0.25` | Gravidade durante um corte no ar, só na descida. |
| `airAttackMaxFall` | `-3` | Queda máxima durante um corte no ar (u/s). |
| `finisherKnockback` | `8` | Empurrão do corte 3 (vai para o `knockback` da hitbox `Slash3`). |
| `shardCost` | `15` | Gelo por estilhaço. |
| `shootCooldown` | `0.25` | Segundos mínimos entre tiros. |
| `guardMoveFactor` | `0.3` | Velocidade com o escudo, em fração de `runSpeed`. |
| `blockCost` | `10` | Gelo por bloqueio. |
| `blockPush` | `3` | Empurrão para trás ao bloquear (u/s). |
| `parryWindow` | `0.15` | Segundos depois de apertar `V` em que o golpe de frente vira aparo. |
| `guardBreakStun` | `0.5` | Segundos sem escudo nem golpe depois de o escudo quebrar. |
| `parryStun` (no `Damageable`) | `1` | Segundos de `Stunned` para quem foi aparado. |

## Habilidades

Uma habilidade é um **componente** no prefab do Iki, ligado por uma **flag global** do `WorldState`. A primeira é `IceStakesAbility` (`examples/iki_demo/src/iki_demo/abilities/IceStakesAbility.hpp`):

```cpp
class IceStakesAbility : public Component {
    MUSH2D_COMPONENT(IceStakesAbility)
public:
    MUSH2D_EXPORT(std::string, unlockFlag, "ability.ice.stakes")
    bool unlocked(const Scene2D& scene) const; // lê o serviço WorldState; falso sem ele
};
```

- **Flags:** `WorldState::flag(nome)` e `setFlag(nome, bool)` guardam valores globais sob o iid reservado `"_global"`. A restauração da sala na morte não mexe neles, então a habilidade não se perde ao morrer.
- **Desbloquear:** um `ItemObject` com `kind` começando por `ability.` chama `setFlag(kind, true)` ao ser pego. Na sala de teste [`IceTest`](mundo-e-salas.md#a-sala-icetest), o item `ability.ice.stakes` fica antes da parede de gelo.
- O Iki pergunta `getComponent<IceStakesAbility>()` e `unlocked(scene)`. Sem o componente, sem o serviço ou sem a flag, as estacas não valem.

### Como criar uma habilidade nova

Exemplo: o pulo duplo de fogo.

1. Crie `examples/iki_demo/src/iki_demo/abilities/FireDoubleJumpAbility.hpp`, copiando `IceStakesAbility`, com `unlockFlag` `"ability.fire.doublejump"`.
2. Inclua o cabeçalho em `examples/iki_demo/src/iki_demo/objects/IkiObject.hpp`. Assim o `iki_demo_objects.cpp` gerado registra o componente e o prefab o encontra.
3. Acrescente ao prefab: `{ "type": "FireDoubleJumpAbility" }` em `components`.
4. No estado que usa a habilidade (por exemplo, `JumpState`), pergunte `getComponent<FireDoubleJumpAbility>()` e `unlocked(scene)` antes de agir. Se ela gasta energia, use `Resources::spend`.
5. Ponha no LDtk um item com `kind` igual à flag. Escreva um teste com e sem a flag (veja `iki_wall_cling_requires_ability`).

## Juice

O Iki não tem código de efeito. Ele só avisa o que aconteceu (`jumped`, `turned`, `hurt`, a troca de estado, o evento `step` da animação de corrida), e o prefab diz o que tocar:

```json
"effects": {
  "move:landed": ["land_dust"],
  "move:jumped": ["jump_dust", {"squash": [1.2, 0.8]}, {"squash": [0.8, 1.25], "delay": 0.033}],
  "move:turned": ["turn_dust"],
  "anim:step":   ["run_dust"],
  "move:grab":          ["ice_stake"],
  "state:move:wall":    [{"squash": [1.15, 0.9]}],
  "state:move:ceiling": [{"squash": [1.15, 0.9]}],
  "move:climb":         ["ice_shards"],
  "move:hurt":          [{"blink": 1.0, "scaleByStrength": true}]
}
```

Os efeitos ficam em `assets/effects/` (`land_dust`, `run_dust`, `turn_dust`, `jump_dust`, `ice_stake`, `ice_shards`). Para mudar a poeira do pouso, edite `land_dust.effect.json`: no Debug, salvar recarrega na hora. As partículas não emitem luz.

## Como adicionar um estado

Exemplo: um estado `crouch` (agachar) com `↓` no chão.

1. Crie `examples/iki_demo/src/iki_demo/iki/states/CrouchState.hpp`:

    ```cpp
    #pragma once

    #include "engine/world/StateMachine.hpp"
    #include "game/objects/IkiObject.hpp"

    // Agachado: parado enquanto ↓ estiver segurado.
    struct CrouchState : State<IkiObject> {
        const char* name() const override { return "crouch"; }
        const char* update(IkiObject& iki, float dt) override {
            iki.runHorizontal(dt, 0.0f); // freia
            iki.applyGravity(dt);
            iki.moveAndCollide(dt);
            if (!iki.downHeld() || !iki.grounded) return iki.nextLocomotion("");
            return nullptr; // fica
        }
    };
    ```

2. Em `IkiObject::onCreate` (`IkiObject.cpp`), acrescente `machine_.add(std::make_unique<CrouchState>());` e o `#include`.
3. Diga quando entrar: por exemplo, no `update` do `IdleState`, `if (iki.downHeld()) return "crouch";`.
4. No prefab, dê uma animação ao estado: `"animations": { "crouch": "..." }`. Se quiser juice, um gatilho `"state:move:crouch"` na seção `effects`.
5. Escreva um teste em `tests/tests.cpp` (veja os testes `iki_*`).

Uma transição por passo: o nome devolvido pelo `update` só vale no passo seguinte. Um nome que não existe avisa uma vez no stderr e o Iki fica no estado atual.

**Arquivos:** `examples/iki_demo/src/iki_demo/objects/IkiObject.hpp/.cpp`, `examples/iki_demo/src/iki_demo/iki/states/*.hpp`, `examples/iki_demo/src/iki_demo/abilities/*.hpp`, `examples/iki_demo/src/iki_demo/ui/DebugHud.cpp`, `assets/prefabs/iki.prefab.json`, `assets/effects/*.effect.json`.
