# Objetos do jogo

Os objetos ficam em `src/game/objects/`. Cada entidade do LDtk cria o prefab de mesmo nome em `snake_case` (veja [Prefabs](fazendo-o-jogo/prefabs.md)). Um campo da entidade com o nome de um atributo muda o valor só naquela instância.

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

## Player

**O que faz:** o personagem controlado pelo jogador. Anda, pula (com pulo curto ao soltar o botão), dash e golpe, tem checkpoint e pode ser teleportado. Tags: `player`.

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

**Como nasce:** pela entidade `PlayerStart` (fábrica em código em `IkigaiGame.cpp`), não por prefab. Ela cria o player uma vez só, em modo `Platformer`, com `moveSpeed` 4,5, collider 0,6 × 1,6, `gravity` -32 e `jumpSpeed` 11. O player é `persistent` e não pertence a nenhuma sala.

**Casos de uso:**

- Ajustar a altura do pulo ao vivo: `F1` → "Cena (F1)" → player → `jumpSpeed`. O ápice é `jumpSpeed² / (2 × |gravity|)` unidades.
- Mandar o player para outro lugar em código: `player->teleport(posicao)`.
- Saber se o player apertou ↓ neste frame (para interagir): `player->interactPressed()`.

**Arquivos:** `src/game/objects/PlayerObject.hpp`, `src/game/spawners/Spawn.cpp`.

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

**Arquivos:** `src/game/objects/SlimeEnemy.hpp`, `assets/prefabs/slime.prefab.json`, `assets/prefabs/slime_fast.prefab.json`.

## Door

**O que faz:** sólida e visível enquanto fechada; aberta, some e deixa passar. O estado `open` fica no `WorldState`, então uma alavanca pode abri-la mesmo com a sala da porta descarregada. Do LDtk, ela toma o tamanho da caixa da entidade. Tag: `door`.

**Atributos:** só os de todo objeto.

**Casos de uso:**

- Uma porta de 1 × 3: estique a caixa da entidade `Door` no LDtk.
- Abrir por código: `state->set(porta.entityIid, "open", 1.0)` no `WorldState`.

**Arquivos:** `src/game/objects/DoorObject.hpp`, `assets/prefabs/door.prefab.json`.

## Lever

**O que faz:** com o player em cima, ↓ alterna a alavanca (`on`) e a porta ligada (`open`) no `WorldState`. Vermelha desligada, verde ligada. Tags: `trigger`, `lever`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `door` | EntityRef | `""` | A porta que ela abre (referência a uma entidade do LDtk). |

**Casos de uso:**

- Ligar alavanca e porta: na entidade `Lever`, campo `door` (Entity ref) apontando para a entidade `Door`.
- Duas alavancas para a mesma porta: as duas com `door` na mesma porta; cada uma inverte o estado.

**Arquivos:** `src/game/objects/LeverObject.hpp`, `assets/prefabs/lever.prefab.json`. Explicada linha por linha em [Objeto novo](fazendo-o-jogo/objeto-novo.md).

## Item

**O que faz:** coletado ao ser tocado pelo player. Ainda não há inventário: coletar só registra o estado (com a política de persistência) no `WorldState`. Tags: `trigger`, `item`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `kind` | String | `""` (prefab: `"coin"`) | Tipo do item. Hoje só informativo. |

**Casos de uso:**

- Uma moeda que nunca volta: campo `persistence` = `permanent`.
- Uma vida que volta depois de um tempo: campo `respawn` ligado.

**Arquivos:** `src/game/objects/ItemObject.hpp`, `assets/prefabs/item.prefab.json`.

## Checkpoint

**O que faz:** ao ser tocado, vira o ponto de reaparecimento do player e registra a hora da ativação no `WorldState`. O último ativado é o ponto de respawn. Tags: `trigger`, `checkpoint`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `oneShot` | Bool | `false` | Só funciona na primeira vez. |

**Casos de uso:**

- Um checkpoint antes de um trecho difícil: ponha a entidade `Checkpoint` no começo do trecho.
- Um checkpoint que não pode ser reativado depois de passar por outro: `oneShot` ligado.

**Arquivos:** `src/game/objects/CheckpointObject.hpp`, `assets/prefabs/checkpoint.prefab.json`.

## Critter

**O que faz:** animal de decoração. Um sprite com `SpriteAnimator`, com os pés na base da caixa da entidade.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `anim` | AssetRef | `""` | Animação "idle" (nome no atlas do sprite). Vazio = a do prefab (`fox_idle`). |

Use `flipX` para virar o animal.

**Casos de uso:** uma raposa olhando para a esquerda: entidade `Critter` com `flipX` ligado.

**Arquivos:** `src/game/objects/CritterObject.hpp`, `assets/prefabs/critter.prefab.json` (atlas `kaa_animals`).

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

**Arquivos:** `src/game/objects/PropObject.hpp`, `assets/prefabs/prop.prefab.json`.

## Crystal

**O que faz:** cristal brilhante: sprite emissivo, um filho `Halo` desenhado na passada aditiva (`crystal_halo`) e uma luz que tremula, tudo na cor da `variant`.

**Atributos:**

| Atributo | Tipo | Padrão | Efeito |
|---|---|---|---|
| `variant` | Int | `0` | `0` ciano, `1` magenta, `2` ciano claro, `3` rosa. |

**Casos de uso:** iluminar uma caverna: espalhe entidades `Crystal` com variantes diferentes. Cada uma traz a própria luz.

**Arquivos:** `src/game/objects/CrystalObject.hpp`, `assets/prefabs/crystal.prefab.json`, `assets/prefabs/crystal_halo.prefab.json`.

## Teleport e Wall

Usados só nas cenas antigas montadas em código (Sandbox, Test), sem prefab. São configurados por uma `Config` no construtor.

- **Teleport:** trigger que leva o player para `targetPosition` (opção `oneShot`). Tags: `trigger`, `teleport`.
- **Wall:** caixa sólida de tamanho `size`. Tag: `wall`.

**Arquivos:** `src/game/objects/TeleportObject.hpp`, `src/game/objects/WallObject.hpp`, `src/game/scenes/`.

---

## Entidades com fábrica em código

### PlayerStart

Cria o player (veja [Player](#player)). Se o player já existe (a sala carregou de novo), não faz nada.

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
