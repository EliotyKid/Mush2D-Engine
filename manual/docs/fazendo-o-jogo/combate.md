# Combate

O combate é feito de componentes da engine (`src/engine/world/Combat.hpp` e `Projectile.hpp`). Qualquer objeto os recebe pelo prefab: o Iki, os bonecos de treino, os inimigos do D4 e o chefe. Ninguém testa sobreposição no próprio código.

| Peça | O que é |
|---|---|
| `Hitbox` | o golpe: uma caixa que, enquanto ativa, acerta quem está do outro time |
| `Hurtbox` | onde o objeto pode ser atingido |
| `Damageable` | recebe o acerto: escudo, invencibilidade, fraquezas, vida |
| `Stunned` | um relógio de atordoamento que os outros componentes consultam |
| `Projectile` e `ProjectilePool` | tiros em linha reta, reaproveitados |

O jogo registra dois serviços da cena: `CombatWorld` (as hurtboxes) e `ProjectilePool`. Sem eles, hitboxes e projéteis não acertam nada, mas nada falha.

## Hurtbox

```json
"components": [
  { "type": "Hurtbox", "props": { "size": [0.8, 1.2], "offset": [0, 0.6], "team": "enemy" } }
]
```

- `size` e `offset` (u) formam a caixa em volta da posição do objeto.
- **Espelho:** segue para onde o objeto raiz olha (o `flipX` do ancestral do topo). Olhando para a esquerda, o x do `offset` inverte, e um filho fica espelhado em volta da raiz: um filho em `[0.6, 0.4]` fica na frente para os dois lados. Vale para hitbox e hurtbox. O `flipX` do próprio filho não conta.
- `team`: `"player"` ou `"enemy"`. Um golpe nunca acerta o próprio time.
- `enabled` falso: a hurtbox não é atingida (o boneco quebrado faz isso). Objeto inativo também não.
- Fica no `CombatWorld`, uma grade só de hurtboxes. Sai sozinha quando o objeto é destruído, quando a sala descarrega, na morte, no hot reload e no `removeComponent`.
- Uma hurtbox por objeto. Para mais de uma (cabeça e corpo), use objetos filhos: cada filho tem o seu `Damageable`.

## Hitbox

```json
"children": [
  { "name": "Bite", "position": [0.6, 0.4],
    "components": [{ "type": "Hitbox", "props": { "size": [0.6, 0.5], "damage": 1, "team": "enemy", "knockback": 4 } }] }
]
```

| Atributo | Padrão | Para quê |
|---|---|---|
| `size`, `offset` | 1 × 1, 0 | a caixa do golpe |
| `damage` | 1 | dano antes das fraquezas |
| `element` | `Ice` | elemento do golpe |
| `knockback` | 0 | força do empurrão, para quem reage ao dano |
| `team` | `"player"` | o time de quem ataca |
| `parryable` | verdadeiro | falso: o aparo vira só bloqueio |
| `activeStart`, `activeEnd` | 0,05 e 0,15 s | a janela do golpe, para o código que o controla quando a animação não tem os eventos `hit_on` e `hit_off` |

A caixa espelha como a da hurtbox (veja acima): ponha o filho na frente de quem olha para a direita.

A hitbox começa desligada. O código do ataque chama `activate()` no começo do golpe e `deactivate()` no fim. Cada ativação acerta cada hurtbox uma vez só: segurar a hitbox ligada não dá dano a cada quadro. Ativar de novo permite acertar de novo.

Cada acerto emite `HitEvent{attacker, target, info}`. O `info` (`DamageInfo`) leva o dano, o elemento, o empurrão, `parryable` e:

- `direction`: do atacante (o objeto raiz) ao centro da hurtbox do alvo (unitária). No mesmo lugar, para onde a raiz olha. Num projétil, a direção do voo;
- `source`: quem conta como autor. Na hitbox, é o objeto raiz do dono (o golpe do filho `Slash1` conta para o Iki), ou o campo `source` da hitbox se preenchido.

## Dano

O `Damageable` recebe os `HitEvent` cujo alvo é o dono, nesta ordem:

1. **Invencibilidade:** durante `invulnerableTime` (0,2 s) depois de um dano, o golpe é ignorado. Não há aparo nesse tempo.
2. **Escudo:** o callback `guard` (opcional, em C++) recebe o `DamageInfo` e devolve `Hit`, `Blocked` ou `Parried`. Bloqueado ou aparado não dá dano nem invencibilidade. Com `info.parryable` falso, o `guard` deve tratar o golpe como bloqueio (e cobrar o bloqueio); se ele devolver `Parried` mesmo assim, o `Damageable` o troca por `Blocked`: sem atordoar nem refletir.
3. **Fraquezas:** o multiplicador do elemento (veja abaixo).
4. **Vida:** tira o dano final do `Resources` do dono (alvo já morto ignora o golpe).
5. **Aviso:** emite `DamageTaken{target, info, finalAmount}`. Quem reage ao dano (empurrão, estado `hurt`) ouve esse evento.

No aparo, o `Damageable` também atordoa `info.source` por `parryStun` (1 s), se ele tiver o componente `Stunned`, e reflete o projétil que acertou (veja [Projéteis](#projeteis)).

### Gatilhos de efeito

Na seção `effects` do prefab, como os gatilhos `state:`, `anim:` e `move:`:

| Gatilho | Em quem | Quando | Força |
|---|---|---|---|
| `hit:taken` | alvo | dano maior que 0 | dano final |
| `hit:dealt` | `info.source` (senão quem acertou) | dano maior que 0 | dano final |
| `hit:immune` | alvo | multiplicador 0 | 1 |
| `hit:blocked` | alvo | `guard` devolveu `Blocked` | 1 |
| `hit:parried` | alvo | `guard` devolveu `Parried` | 1 |

Os efeitos tocam na posição do objeto, espelhados pela direção do golpe. `hit:blocked`, `hit:parried` e `hit:immune` têm força 1, porque não há dano para escalar: um `scaleByStrength` neles toca no tamanho normal.

```json
"effects": {
  "hit:taken": [{ "flash": { "color": [1, 1, 1], "duration": 0.08 } }, "hit_sparks"],
  "hit:immune": ["immune_ping"]
}
```

## Fraquezas e resistências

```json
"multipliers": { "Ice": 2.0, "Fire": 0.5 }
```

- Multiplica o dano do elemento. Elemento ausente = 1.
- Arredonda para o inteiro mais próximo, com mínimo 1 se o multiplicador for maior que 0: 1 × 2 = 2, 1 × 0,5 = 1, 2 × 0,5 = 1.
- 0 = imune: sem dano, com o gatilho `hit:immune`.
- A seção cria o `Damageable` se ele faltar. Herança: o filho troca só os elementos que escreve. Um elemento desconhecido ou um valor negativo avisa e é ignorado.

## Atordoamento

`Stunned` é só um relógio: `start(segundos)` (fica com o maior, se já estiver atordoado) e `active()`. Inimigos e bonecos o consultam antes de atacar. O hit stop o segura, como todo tempo de jogo.

## Projéteis

```json
{
  "version": 1,
  "sprite": { "atlas": "props", "frame": "shard 0" },
  "components": [{ "type": "Projectile", "props": { "speed": 5, "range": 10, "damage": 1, "element": "Ice" } }],
  "effects": { "move:break": ["shard_break"] }
}
```

| Atributo | Padrão | Para quê |
|---|---|---|
| `speed` | 10 u/s | velocidade |
| `range` | 8 u | distância máxima; depois some, sem quebrar |
| `team` | `"enemy"` | trocado pelo `spawn` |
| `damage`, `element`, `parryable` | 1, `Ice`, verdadeiro | o golpe |
| `size` | 0,25 × 0,25 | a caixa que acerta as hurtboxes |

- Anda em linha reta e bate nos segmentos do terreno (`CollisionWorld`). Os de mão única deixam passar.
- Quebra na parede ou no primeiro acerto: emite o gatilho `move:break` onde quebrou, some e volta ao pool. As partículas da quebra continuam tocando.
- Se duas hurtboxes de outro time estão no caminho de um mesmo passo, acerta a mais perto de onde ele estava.
- Um alvo invencível também quebra o projétil, sem tomar dano.
- Um projétil criado enquanto uma sala nasce (num `onCreate` durante o carregamento dela) pertence a essa sala e some quando ela descarrega. Fora disso, ele não tem sala.
- Atirar: `scene.service<ProjectilePool>()->spawn("dummy_bolt", posição, direção, "enemy", atirador.id)`. Velocidade e dano vêm do prefab. O pool reaproveita um projétil parado do mesmo prefab.
- **Refletir:** no aparo, o projétil volta pela mesma linha, com o mesmo dano, `source` = quem aparou e o `team` da hurtbox dele. Assim ele acerta quem atirou, e o `hit:dealt` é de quem aparou. Só reflete uma vez (`reflected`).

## Como dar um ataque a um inimigo

1. No prefab do inimigo: `Hurtbox` (`team` `"enemy"`), `Damageable`, `Resources` e, se ele puder ser aparado, `Stunned`.
2. Um filho com a `Hitbox` do golpe (`team` `"enemy"`), na frente dele.
3. No estado de ataque da máquina do inimigo: `activate()` no evento de quadro `hit_on` e `deactivate()` no `hit_off` (ou por tempo, com `activeStart` e `activeEnd`). Pule o ataque enquanto `Stunned::active()`.
4. Para atirar, chame o `ProjectilePool` com um prefab de projétil.
5. Juice: `hit:taken` no inimigo e `hit:dealt` no atacante, na seção `effects`.

```cpp
if (auto* stun = getComponent<Stunned>(); stun && stun->active()) return;
if (GameObject* bite = scene.findObject(biteId)) bite->getComponent<Hitbox>()->activate();
```

Veja também a [referência do world](../sistemas/world.md#combate).
