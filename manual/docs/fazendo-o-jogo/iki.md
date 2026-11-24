# Iki: estados e habilidades

O Iki é o personagem jogável de Ikigai (`IkiObject`). Ele nasce do prefab `assets/prefabs/iki.prefab.json` pela entidade `PlayerStart` da vitrine. As cenas Sandbox e Test continuam com o `PlayerObject` antigo.

- Tem a tag `player` e implementa `IPlayer`. Portas, alavancas, itens, checkpoints e teletransportes o encontram por isso, sem saber o tipo dele. Veja [Comunicação](comunicacao.md).
- A posição dele (`transform.position`) são **os pés**. O collider (0,6 × 1,6) fica acima dela (`offset` 0,8). Assim, o pivô do quadro, a inclinação e a poeira ficam no chão.
- Ele anda com o `CharacterController`: rampas, plataformas atravessáveis e descer com `↓` + `Z`. Veja [Colisão e rampas](colisao-e-rampas.md).
- Ele pula num slime e o mata, com a mesma regra do `PlayerObject` (`SlimeEnemy::stomp`).

## Controles

| Tecla | Ação |
|---|---|
| `←` `→` | Correr. |
| `Z` | Pular. Segurar pula mais alto; soltar subindo corta o pulo. |
| `↓` + `Z` numa plataforma atravessável | Descer por ela. |
| `↓` | Interagir (alavanca). |
| `J` (só Debug) | 1 de dano, empurrando para trás. |
| `L` (só Debug) | Gasta 30 de gelo e 30 de fogo. |

A seta `↑` **não** pula o Iki: ela vai subir paredes e agarrar tetos de gelo (D2, Task 6).

## Estados

O movimento é uma **máquina de estados** chamada `"move"`. Em cada momento o Iki está em exatamente um estado. Cada estado é uma classe pequena em `src/game/iki/states/`, com três partes:

- `enter`: roda ao entrar no estado;
- `update`: roda a cada passo e devolve o nome do próximo estado, ou `nullptr` para ficar;
- `exit`: roda ao sair.

| Estado | Quando | Sai para | Animação |
|---|---|---|---|
| `idle` | No chão, sem direção. Freia com `groundDecel`. | `run`, `jump`, `fall` | `idle` |
| `run` | No chão, com direção. Acelera até `runSpeed`. | `idle`, `jump`, `fall` | `run` |
| `jump` | No ar, subindo. Soltar `Z` corta o pulo uma vez. | `fall`, `idle`, `run` | `jump` |
| `fall` | No ar, descendo. O coyote time ainda deixa pular. | `idle`, `run` | `fall` |
| `hurt` | Levou dano. Sem controle por `hurtDuration`, empurrado por `hurtKnockback`. | o estado de movimento certo | `attack_leave` (provisório) |
| `dead` | Vida em 0. Sem controle. Só `respawn()` tira dele. | `idle` (no renascimento) | `fall` (provisório) |

- O nome do estado é também a chave da animação no prefab (`animations`). Ao trocar de estado, o Iki chama `SpriteAnimator::setState`.
- A máquina aparece no inspetor F1, na janela "Cena (F1)", com o Iki selecionado: `move: run (0.50 s)`.
- Toda troca de estado emite `StateChanged`, que os efeitos usam (`state:move:hurt` faz o Iki piscar).

## A ordem de cada passo

O `onStep` do Iki sempre segue a mesma ordem:

1. Entrada: o jogo chamou `applyInput` antes do passo.
2. `JumpAssist::tick`: conta o coyote time e o buffer de pulo.
3. `states().update`: o estado atual move o Iki com os métodos de física.
4. `SpriteAnimator`, 5. `SquashStretch`, 6. `VelocityLean` e `Blink`: os componentes rodam logo depois do `onStep`, na ordem do prefab (`components`).
7. `emitMovementCues`: avisa `landed`, `left_ground` e `turned`. Os efeitos tocam pelos gatilhos do prefab.

Os estados usam estes métodos do `IkiObject`, nesta ordem:

| Método | O que faz |
|---|---|
| `runHorizontal(dt, entrada)` | Leva `velocity_.x` até `entrada * runSpeed`, com aceleração ou desaceleração (chão ou ar). Vira o Iki para a entrada. Virar no chão rápido emite `turned`. |
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
| `jumpBuffer` | `0.12` | Segundos em que um `Z` apertado antes de tocar o chão ainda vale. |
| `cornerCorrection` | `0.2` | Subindo, a cabeça que bate na quina de um teto desliza até isto para o lado e continua subindo. |
| `wallClimbSpeed`, `wallJumpVelocity`, `wallJumpLock`, `ledgeHop`, `ceilingMoveSpeed` | | Estacas de gelo (Task 6). |
| `hurtDuration` | `0.35` | Segundos sem controle após o dano. |
| `hurtKnockback` | `[6, 6]` | Empurrão do dano: x no sentido do golpe, y para cima. |
| `invulnerableTime` | `0.8` | Segundos sem levar dano novo. O piscar do prefab (`state:move:hurt`) usa o mesmo tempo. |

Os números de juice (squash, inclinação, piscar, partículas) não ficam no Iki: ficam nos componentes do prefab e nos arquivos de efeito. Veja [Game juice](game-juice.md).

## Vida, energia, dano e morte

- O componente `Resources` guarda a vida (5) e as energias de gelo e de fogo (100). Ele recarrega sozinho as energias, 10 por segundo, 1 s depois do último gasto.
- `hurt(DamageInfo)`: tira vida, empurra e entra em `hurt`. Durante `invulnerableTime`, um dano novo é ignorado.
- Vida em 0: o Iki entra em `dead` e o `Resources` emite `Died`. O `IkigaiGame` ouve `Died` e chama a morte que já existe: escurece, restaura a sala e leva ao checkpoint.
- No renascimento, o jogo move o Iki (`respawnAt`) e chama `respawn()`: vida e energias cheias, sem invulnerabilidade, sem velocidade, sem pulo guardado, estado `idle`.
- O HUD de teste (canto superior esquerdo) mostra 5 corações, a barra azul de gelo e a laranja de fogo. Ele fica em `src/game/ui/DebugHud.cpp` e será trocado pela UI do D9.

## Juice

O Iki não tem código de efeito. Ele só avisa o que aconteceu (`jumped`, `turned`, a troca de estado, o evento `step` da animação de corrida), e o prefab diz o que tocar:

```json
"effects": {
  "move:landed": ["land_dust"],
  "move:jumped": ["jump_dust", {"squash": [1.2, 0.8]}, {"squash": [0.8, 1.25], "delay": 0.033}],
  "move:turned": ["turn_dust"],
  "anim:step":   ["run_dust"],
  "state:move:hurt": [{"blink": 0.8}]
}
```

Os efeitos ficam em `assets/effects/` (`land_dust`, `run_dust`, `turn_dust`, `jump_dust`, `ice_stake`, `ice_shards`). Para mudar a poeira do pouso, edite `land_dust.effect.json`: no Debug, salvar recarrega na hora. As partículas não emitem luz.

## Como adicionar um estado

Exemplo: um estado `crouch` (agachar) com `↓` no chão.

1. Crie `src/game/iki/states/CrouchState.hpp`:

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

**Arquivos:** `src/game/objects/IkiObject.hpp/.cpp`, `src/game/iki/states/*.hpp`, `src/game/ui/DebugHud.cpp`, `assets/prefabs/iki.prefab.json`, `assets/effects/*.effect.json`.
