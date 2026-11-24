# Game juice

"Juice" é o retorno visual que faz o controle parecer vivo: poeira ao pousar, o corpo amassando e esticando, o sprite piscando ao levar dano. No Mush2D, cada efeito é um componente da engine (`src/engine/world/`). Qualquer objeto pode usar esses componentes, não só o player.

| Efeito | Onde | Parâmetros (exportados) |
|---|---|---|
| Squash e stretch | componente `SquashStretch`: mexe só em `SpriteData::drawSize` e volta à escala 1 com mola | `stiffness` (300), `damping` (18) |
| Inclinação | componente `VelocityLean`: gira o sprite levemente pela velocidade horizontal, medida pela variação da posição | `maxDegrees` (4), `fullSpeed` (5) |
| Piscar | componente `Blink`: alterna a visibilidade do sprite por um tempo (invulnerabilidade agora, golpe em inimigos no D3) | `rate` (16) |
| Aceleração e desaceleração | física do Iki | veja o `IkiObject` em [Objetos do jogo](../objetos.md) |
| Gravidade no topo, queda rápida, correção de quina | física do Iki | idem |
| Partículas | componente `ParticleEmitter` | `maxParticles` (128) e a configuração de cada rajada |
| Antecipação | squash curto de 2 quadros no pulo saindo do chão | `jumpAnticipation` (Iki) |

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
dust.size = {0.12f, 0.0f};       // [início, fim] u
dust.colorStart = {0.8f, 0.75f, 0.65f, 0.9f};
dust.colorEnd = {0.8f, 0.75f, 0.65f, 0.0f};
emitter->burst(dust, obj.transform.position, obj.flipX ? -1 : 1); // -1 espelha os ângulos
```

- As partículas ficam no mundo: não seguem o dono depois de nascer.
- Cada uma é desenhada como um sprite. Sem `frame`, é um pixel branco tingido pela cor; com `frame`, é um quadro do atlas do sprite do dono.
- Elas não acendem nada: não têm luz nem emissão. `additive` (para brilhos) existe para o D3.
- `drag` tira essa fração da velocidade por segundo. `layer` ordena como a `layer` do sprite.
- Acima de `maxParticles`, a mais velha some.

No JSON (usado pelos arquivos de efeito), a mesma configuração é lida por `parseParticleConfig`:

```json
{"count": 6, "speed": [1, 3], "angle": [100, 170], "lifetime": [0.2, 0.4], "gravity": -4,
 "size": [0.12, 0.0], "color": [[0.8,0.75,0.65,0.9],[0.8,0.75,0.65,0]], "frame": "", "additive": false, "layer": 0}
```

As chaves que faltam ficam com o padrão. Uma chave desconhecida ou um tipo errado devolve erro com o nome da chave.

## Squash e stretch

```cpp
auto& ss = obj.addComponent<SquashStretch>();
ss.kick({0.8f, 1.25f});      // estica (pulo); {1.3f, 0.7f} amassa (pouso)
// a cada quadro, depois do SpriteAnimator posicionar o quadro:
ss.apply(*obj.sprite);
```

- As molas de x e de y são independentes e andam no `onStep` do componente.
- `apply` multiplica `drawSize` pelo fator atual e ajusta `offset`, para a base do sprite ficar no mesmo lugar.
- `apply` não guarda o tamanho original. Chame-o só depois de algo reescrever `drawSize` no quadro (o `SpriteAnimator`), senão o fator se acumula.
- `transform.scale` nunca muda.

## Piscar

```cpp
obj.addComponent<Blink>().start(0.8f); // pisca por 0,8 s
```

O sprite começa escondido e alterna `rate` vezes por segundo. No fim, volta à visibilidade que tinha. Um objeto já escondido continua escondido.

## Inclinação

Com `VelocityLean`, o objeto inclina para a frente ao correr: até `maxDegrees` a partir de `fullSpeed` u/s, suavizado. A velocidade vem da variação da posição de mundo, então funciona com qualquer movimento. O sprite gira em torno do pivô (os pés).

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
