# Game juice

"Juice" é o retorno visual que faz o controle parecer vivo: poeira ao pousar, o corpo amassando e esticando, o sprite piscando ao levar dano. No Mush2D, cada efeito é um componente da engine (`src/engine/world/`). Qualquer objeto pode usar esses componentes, não só o player.

O jeito normal de usar é sem código: um arquivo de efeito descreve o efeito uma vez, e a seção `effects` do prefab diz quando tocá-lo. Veja [Efeitos reaproveitáveis](#efeitos-reaproveitaveis).

O exemplo completo é o Iki: `assets/prefabs/iki.prefab.json` liga os componentes, as animações, os eventos de passo e os efeitos de `assets/effects/`. Veja [Iki: estados e habilidades](iki.md#juice).

| Efeito | Onde | Parâmetros (exportados) |
|---|---|---|
| Squash e stretch | componente `SquashStretch`: mexe só em `SpriteData::drawSize` e volta à escala 1 com mola | `stiffness` (300), `damping` (18) |
| Inclinação | componente `VelocityLean`: gira o sprite levemente pela velocidade horizontal, medida pela variação da posição | `maxDegrees` (4), `fullSpeed` (5), `smoothing` (12), `teleportDistance` (1) |
| Piscar | componente `Blink`: alterna a visibilidade do sprite por um tempo (invulnerabilidade agora, golpe em inimigos no D3) | `rate` (16) |
| Partículas | componente `ParticleEmitter` | `maxParticles` (128) e a configuração de cada rajada |

Os números exportados aparecem no inspetor F1 e podem ir para o prefab (`components` → `props`).

## Partículas

```cpp
#include "engine/world/Particles.hpp"

auto* emitter = obj.getComponent<ParticleEmitter>();
if (!emitter) emitter = &obj.addComponent<ParticleEmitter>();
ParticleConfig dust;
dust.count = 6;
dust.speed = {1.0f, 3.0f};       // [min, max] u/s
dust.angle = {100.0f, 170.0f};   // [min, max] graus; 0 = direita, 90 = cima
dust.lifetime = {0.2f, 0.4f};    // [min, max] s
dust.gravity = -4.0f;            // u/s², negativo cai
dust.size = {0.14f, 0.03f};      // [início, fim] u
dust.colorStart = {0.95f, 0.92f, 0.85f, 1.0f};
dust.colorEnd = {0.9f, 0.86f, 0.78f, 0.0f};  // desbota até sumir
emitter->burst(dust, obj.transform.position, obj.flipX ? -1 : 1); // -1 espelha os ângulos
```

- As partículas ficam no mundo: não seguem o dono depois de nascer.
- Cada uma é desenhada como um sprite. Sem `frame`, é um pixel branco tingido pela cor; com `frame`, é um quadro do atlas do sprite do dono.
- Elas não acendem nada: não têm luz nem emissão. As não aditivas vão num passe com mistura alfa (iluminadas pela luz da cena, com a névoa da cena e escondidas atrás de sprites opacos mais à frente) e desbotam até alfa 0; `additive` (para brilhos) soma luz.
- `drag` tira essa fração da velocidade por segundo. `layer` ordena como a `layer` do sprite.
- Acima de `maxParticles`, a mais velha some.

No JSON (usado pelos arquivos de efeito), a mesma configuração é lida por `parseParticleConfig`:

```json
{"count": 6, "speed": [1, 3], "angle": [100, 170], "lifetime": [0.2, 0.4], "gravity": -4,
 "size": [0.14, 0.03], "color": [[0.95,0.92,0.85,1],[0.9,0.86,0.78,0]], "frame": "", "additive": false, "layer": 0}
```

As chaves que faltam ficam com o padrão. Uma chave desconhecida ou um tipo errado devolve erro com o nome da chave.

## Squash e stretch

```cpp
auto& ss = obj.addComponent<SquashStretch>();
ss.kick({0.8f, 1.25f});      // estica (pulo); {1.3f, 0.7f} amassa (pouso)
```

- As molas de x e de y são independentes e andam no `onStep` do componente.
- O componente se aplica sozinho ao sprite do dono a cada passo. O `SpriteAnimator` também o aplica logo depois de posicionar o quadro. Assim, a ordem dos componentes não importa, e um objeto só de prefab também amassa.
- `apply(sprite)` multiplica `drawSize` pelo fator atual e ajusta `offset`, para a base do sprite ficar no mesmo lugar. Ele lembra o que escreveu: chamar de novo, ou num sprite que ninguém redimensionou, parte do tamanho sem squash, sem acumular.
- Um sprite com `drawSize` {0, 0} (só a escala) não amassa.
- `transform.scale` nunca muda.

## Piscar

```cpp
obj.addComponent<Blink>().start(0.8f); // pisca por 0,8 s
```

O sprite começa escondido e alterna `rate` vezes por segundo. No fim, volta à visibilidade que tinha. Um objeto já escondido continua escondido.

## Inclinação

Com `VelocityLean`, o objeto inclina para a frente ao correr: até `maxDegrees` a partir de `fullSpeed` u/s, suavizado por `smoothing` (por segundo). A velocidade vem da variação da posição de mundo, então funciona com qualquer movimento. Um salto maior que `teleportDistance` num passo (respawn, teletransporte) não conta como velocidade. O sprite gira em torno do pivô (os pés). Os filhos do objeto giram junto.

## Eventos por quadro

O prefab liga quadros de uma animação a nomes de evento:

```json
"animations": { "run": "run" },
"animationEvents": { "run": { "3": "step", "11": "step" } }
```

- A chave é o nome da animação no atlas.
- O quadro conta a partir de 0 dentro da animação.
- O `SpriteAnimator` emite `AnimationEvent{object, animation, name}` no `scene.events()` cada vez que o quadro entra. Isso vale a cada volta do loop e também para um quadro pulado por um `dt` grande. Enquanto o quadro fica na tela, o evento não se repete.

```cpp
sub = scene.events().subscribe<AnimationEvent>([this](const AnimationEvent& e) {
    if (e.object == id && e.name == "step") { /* poeira de passo */ }
});
```

## Efeitos reaproveitáveis

Todo efeito pode ser usado por qualquer entidade só editando o prefab. O Iki usa o mesmo caminho que um slime usaria, sem código de efeito próprio.

### Arquivos de efeito

Um efeito fica em `assets/effects/<nome>.effect.json`. Ele é definido uma vez e usado pelo nome:

```json
{"version": 1, "actions": [
  {"particles": {"count": 6, "speed": [1, 3], "angle": [100, 170], "lifetime": [0.2, 0.4], "gravity": -4,
                 "size": [0.14, 0.03], "color": [[0.95,0.92,0.85,1],[0.9,0.86,0.78,0]]}},
  {"squash": [1.3, 0.7], "scaleByStrength": true}
]}
```

| Ação | O que faz |
|---|---|
| `particles` | Uma rajada com a configuração de partícula acima (formato de `parseParticleConfig`). Usa o `ParticleEmitter` do objeto e cria um se faltar. |
| `squash` | `SquashStretch::kick([x, y])`. Com `"scaleByStrength": true`, o desvio de 1 é multiplicado pela força do gatilho, limitada a 1: um pouso a meia velocidade amassa pela metade. |
| `blink` | `Blink::start(segundos)`. Com `"scaleByStrength": true`, os segundos são multiplicados pela força do gatilho, sem limite: o Iki manda `move:hurt` com força `invulnerableTime`, e `{"blink": 1, "scaleByStrength": true}` pisca esse tempo todo. |
| `effect` | Toca outro efeito pelo nome. Efeitos dentro de efeitos vão até 8 níveis: mais fundo (um ciclo, por exemplo) é pulado com aviso. |
| `hitstop` | `{"hitstop": 0.06}`: congela a cena por esse tempo (`Scene2D::hitstop`). Vários pedidos ficam com o maior, sem somar. Com `scaleByStrength`, os segundos são multiplicados pela força. |
| `shake` | `{"shake": {"amplitude": 0.15, "duration": 0.15}}`: tremor da câmera (`Camera2D::addShake`), um deslocamento aleatório que decai em linha reta até 0. Vários tremores somam até `maxShake` (0,5 u, ajustável no F1, seção "Camera"). A vista tremida não passa da borda da sala: encostada numa borda, a câmera treme só para dentro. Com `scaleByStrength`, a amplitude e a duração são multiplicadas pela força. |
| `flash` | `{"flash": {"color": [1, 1, 1], "duration": 0.08}}`: o sprite do dono fica todo da cor no ato e volta ao normal em linha reta ao longo da duração (`SpriteData::flash`). O flash não recebe luz nem ilumina nada: um flash branco é branco em qualquer sala. Usa o componente `Flash` do objeto e cria um se faltar. Com `scaleByStrength`, a duração é multiplicada pela força. Duração 0 (ou força 0 com `scaleByStrength`) não faz flash nenhum. |
| `sound` | `{"sound": "step", "volume": 0.5, "pitch": [0.9, 1.1]}`: toca `assets/audio/sfx/step.wav` pelo serviço [`Audio`](../sistemas/audio.md). `volume` (opcional, 1 por padrão, número >= 0) multiplica o volume de efeitos. `pitch` (opcional) é um intervalo `[min, max]`, com 0 < min <= max: cada toque sorteia um tom nele, para o mesmo passo não soar sempre igual. O som é global, sem posição. Não usa `scaleByStrength`. |

- **Tempo do hit stop:** durante o hit stop, a cena não avança objetos, componentes, física, partículas, esperas de `delay` nem o `flash`. Só o temporizador do próprio hit stop corre, em tempo real. O tremor da câmera, a interface, o painel F1 e a leitura das teclas continuam. Um aperto de tecla durante o congelamento fica guardado e vale quando o tempo volta (o buffer de pulo do Iki funciona). Apertar e soltar dentro do congelamento vale como um toque de um quadro: o pulo começa inteiro e é cortado no quadro seguinte, como um toque normal. Só o congelamento guarda apertos; a pausa do inspetor e um objeto inativo não.
- Para tingir um sprite sem flash, use `SpriteData::tint`: ele multiplica a `color`, e o código do jogo, que reescreve `color`, não o apaga.

- Qualquer ação aceita `"delay": segundos` e toca depois desse tempo. Exemplo, a antecipação do pulo: `{"squash": [1.2, 0.8]}` agora e `{"squash": [0.8, 1.25], "delay": 0.033}` dois quadros depois.
- Uma ação desconhecida avisa uma vez no stderr, com o nome do arquivo, e é ignorada. As outras ações do arquivo continuam valendo. O mesmo vale para um modificador de tipo errado (`"delay": "0.03"`, `"scaleByStrength": 1`), para uma ação só com modificadores (`{"delay": 0.1}`) e para um `hitstop`, `shake` ou `flash` de formato errado (`{"hitstop": "0.05"}`, um `shake` sem `duration`, uma cor com 2 números, uma chave desconhecida como `"mode"`, que o aviso nomeia), no arquivo, no prefab ou em código.
- O `shake` precisa do serviço `Camera2D` na cena (o `Engine` registra a câmera). Sem ele, avisa uma vez e não treme.
- O `sound` usa o serviço `Audio` (o `Engine` o registra). Sem ele, ou sem dispositivo de áudio, fica em silêncio sem avisar. Um `sound` que não é texto, um `volume` negativo ou de texto e um `pitch` fora do formato avisam e são ignorados, como as outras ações. Um nome de som desconhecido avisa uma vez quando toca, com o caminho esperado.
- O som toca em tempo real: o hit stop não o segura. Já um `sound` com `delay` espera no tempo da cena, como qualquer `delay`.
- Os efeitos do Iki já tocam som: `run_dust` (passo), `jump_dust` (pulo) e `land_dust` (pouso).
- No arquivo, um `particles` com chave errada (`"lifetme"`) ou valor errado também avisa ao carregar, com o arquivo e a chave, e a ação é ignorada.
- Um efeito que se inclui com `delay` também para no limite de 8 níveis: a espera não zera a contagem.
- Um arquivo inválido (JSON quebrado, sem a lista `actions`) avisa e mantém a versão anterior.
- No Debug, salvar ou criar um `.effect.json` recarrega a biblioteca na hora (hot reload). A recarga lê a pasta inteira: um arquivo apagado some na próxima recarga. A sala não recarrega, porque o efeito é procurado pelo nome cada vez que toca.
- Se a pasta não puder ser lida (apagada, ou travada pelo OneDrive), a recarga avisa e mantém todos os efeitos.
- Um nome de efeito que não existe avisa uma vez.

### Gatilhos no prefab

A seção `"effects"` do prefab liga gatilhos a uma lista. Cada item da lista é o nome de um arquivo de efeito ou uma ação inline:

```json
"effects": {
  "move:landed":       ["land_dust"],
  "move:jumped":       ["jump_dust", {"squash": [1.2, 0.8]}, {"squash": [0.8, 1.25], "delay": 0.033}],
  "move:turned":       ["turn_dust"],
  "anim:step":         ["run_dust"],
  "state:move:wall":   ["ice_stake", {"squash": [1.15, 0.9]}],
  "move:hurt":         [{"blink": 1.0, "scaleByStrength": true}]
}
```

| Gatilho | Quando dispara |
|---|---|
| `state:<máquina>:<estado>` | Uma máquina de estados registrada com `registerStateMachine("<máquina>", ...)` entra no estado. O primeiro estado (o `start`) também conta. |
| `anim:<evento>` | O `SpriteAnimator` entra num quadro de `animationEvents` com esse nome (veja [Eventos por quadro](#eventos-por-quadro)). |
| `move:<nome>` | Um `MovementCue` do objeto. `landed`, `left_ground` e `turned` vêm de `emitMovementCues`; os outros, o próprio objeto emite. |

- O efeito toca na posição do gatilho. Sem posição, toca nos pés do objeto: a base do collider ou, sem collider, a posição.
- A direção é o lado para onde o objeto olha (`flipX` espelha as partículas).
- A seção vira um componente `EffectTriggers`. Ele cria o `ParticleEmitter`, o `SquashStretch`, o `Blink` e o `Flash` só quando um efeito precisa.
- Na herança, `effects` é mesclado por gatilho: o filho troca a lista inteira de um gatilho.

### Dar juice a um objeto

1. Crie os efeitos em `assets/effects/` ou use os que já existem.
2. No prefab do objeto, acrescente a seção `effects` com os gatilhos.
3. Se o objeto tem estados, registre a máquina no `onCreate`: `registerStateMachine("move", &move);`. Toda máquina registrada avisa as mudanças de estado (`StateChanged`), em qualquer ordem de criação dos componentes.
4. Se ele anda com o `CharacterController`, chame a cada passo:

```cpp
#include "engine/world/Effects.hpp"

MoveResult now = move(body, velocity, dt, query, last.grounded, jumped);
emitMovementCues(scene, *this, last, now, velocity, maxFallSpeed); // landed, left_ground, turned
if (jumped) emitMovementCue(scene, *this, "jumped");               // movimentos próprios do objeto
last = now;
```

- `landed`: força = |velocity.y| / |maxFallSpeed|, limitada a 1.
- `turned`: no chão, a pelo menos `turnThreshold` (2 u/s), a velocidade inverteu ou o lado para onde o objeto olha virou contra o movimento. Um objeto que freia antes de inverter (como o Iki) passa o lado antes e depois do passo: `emitMovementCues(scene, *this, last, now, velocity, maxFallSpeed, DefaultTurnThreshold, ladoAntes, ladoAgora)` (-1 ou +1).
- Em código, `playEffect(scene, obj, "land_dust", força, posição, direção)` toca na hora um efeito ou uma ação inline.
