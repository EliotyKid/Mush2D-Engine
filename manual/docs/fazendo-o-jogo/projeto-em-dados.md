# Projeto em dados

O arquivo `project.json`, na pasta do jogo, guarda o que é **do jogo** mas a engine precisa saber. Exemplos reais: `examples/iki_demo/project.json` e `games/ikigai/project.json`.

```json
{
    "id": "iki_demo",
    "name": "IkiDemo",
    "elements": ["Ice", "Fire"],
    "surfaces": [ ... ],
    "resources": [ ... ],
    "input": { ... },
    "actions": { ... }
}
```

`id` e `name` são obrigatórios. O `id` é o que se escreve em `--game`.

## `elements`

A lista dos elementos do jogo. O nome vale em prefabs e no código. Exemplo: `"elements": ["Ice", "Fire"]`.

Um prefab de combate pode dizer quanto cada elemento machuca (`"multipliers": {"Ice": 2}`). Se citar um elemento que não está na lista, a engine avisa (com o nome do prefab) e carrega o resto.

## `resources`

As barras do jogo (energia, mana, o que for). Cada uma é um objeto:

```json
{ "name": "ice", "element": "Ice", "max": 100, "regenPerSecond": 10, "regenDelay": 1 }
```

| Campo | Significa |
|---|---|
| `name` | nome da barra (único) |
| `element` | de qual elemento ela é (precisa estar em `elements`) |
| `max` | valor máximo (maior que 0). Se faltar, vale 100 |
| `regenPerSecond` | quanto volta por segundo (0 ou mais). Se faltar, vale 10 |
| `regenDelay` | segundos parada antes de começar a voltar (0 ou mais). Se faltar, vale 1 |

`name` e `element` são obrigatórios; os três números podem ser deixados de fora.
No painel F1, cada barra tem controles para mudar esses números enquanto o jogo roda.

## `surfaces`

Tipos de chão com comportamento especial (por exemplo, gelo que se escala). Cada um liga um nome da camada IntGrid do LDtk a uma flag do `Terrain`:

```json
{ "name": "IceClimb", "intGrid": "IceClimb", "terrainFlag": "iceClimb", "tags": ["climbable"] }
```

| Campo | Significa |
|---|---|
| `name` | nome do tipo (único; no máximo 255 tipos) |
| `intGrid` | o identificador do valor na camada `Collision` do LDtk |
| `terrainFlag` | o nome do campo do `Terrain` que liga esse tipo (veja `terreno.md`) |
| `tags` | etiquetas que o código do jogo pode consultar |

O modelo `ikigai` tem `"surfaces": []`: sem tipos especiais.

## `input`

Dois números da entrada com gamepad:

| Campo | Significa |
|---|---|
| `triggerThreshold` | quanto os gatilhos `L2`/`R2` precisam ser apertados (0 a 1) para contar como botão. Se faltar, vale 0.5 |
| `stickDeadzone` | o quanto o analógico precisa se mover antes de contar (ignora tremidas). Se faltar, vale 0.25 |

A seção `input` inteira pode ser deixada de fora: valem os dois padrões.
Os dois aparecem no F1, em **Projeto · Entrada**.

## `actions`

Uma **ação** é um nome para "o que o jogador quer fazer": `jump`, `attack`. O código e os prefabs falam só o nome; o `project.json` diz quais teclas e botões disparam cada ação.

```json
"jump":   { "keys": ["Z"], "gamepad": ["A"], "buffer": 0.12 },
"moveLeft": { "keys": ["Left"], "gamepad": ["DpadLeft", "LeftStickLeft"] }
```

| Campo | Significa |
|---|---|
| `keys` | teclas (veja a tabela) |
| `gamepad` | botões e eixos do controle (veja a tabela) |
| `buffer` | opcional: segundos que um toque fica guardado (o pulo "adiantado" que ainda vale) |

**Teclado e gamepad são iguais:** qualquer um dos dois dispara a ação, e os dois ao mesmo tempo contam como um só aperto. Se o controle é desligado no meio do jogo, as ações dele são soltas no mesmo instante.

Uma mesma tecla pode estar em duas ações (no `iki_demo`, `Down` é `moveDown`, `interact` e `dropThrough`). O código do jogo decide qual olhar em cada situação.

### Nomes das teclas

| Tipo | Nomes |
|---|---|
| Letras e números | `A` a `Z`, `0` a `9` |
| Funções | `F1` a `F25` |
| Teclado numérico | `Kp0` a `Kp9`, `KpDecimal`, `KpDivide`, `KpMultiply`, `KpSubtract`, `KpAdd`, `KpEnter`, `KpEqual` |
| Setas | `Left`, `Right`, `Up`, `Down` |
| Edição | `Space`, `Enter`, `Tab`, `Backspace`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Escape` |
| Pontuação | `Apostrophe`, `Comma`, `Minus`, `Period`, `Slash`, `Semicolon`, `Equal`, `LeftBracket`, `Backslash`, `RightBracket`, `GraveAccent` |
| Outras | `CapsLock`, `ScrollLock`, `NumLock`, `PrintScreen`, `Pause`, `Menu` |
| Modificadoras | `LeftShift`, `LeftControl`, `LeftAlt`, `LeftSuper`, `RightShift`, `RightControl`, `RightAlt`, `RightSuper` |

### Nomes dos botões do gamepad

| Tipo | Nomes |
|---|---|
| Botões de ação | `A`, `B`, `X`, `Y` |
| Ombros | `L1`, `R1` |
| Gatilhos (viram botão pelo `triggerThreshold`) | `L2`, `R2` |
| Menu | `Back`, `Start`, `Guide` |
| Apertar o analógico | `LeftStick`, `RightStick` |
| Direcional | `DpadUp`, `DpadRight`, `DpadDown`, `DpadLeft` |
| Analógico esquerdo (viram botão pelo `stickDeadzone`) | `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown` |
| Analógico direito | `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown` |

Os nomes seguem o mapeamento padrão do GLFW (`A` é o botão de baixo, como no controle de Xbox).

!!! note "Nome errado não derruba o jogo"
    Uma tecla ou botão que não existe gera um aviso no terminal e é ignorado. O resto do arquivo vale.

### Lendo uma ação no código

Um componente da engine, o `PlatformerMover` (`src/engine/world/PlatformerMover.hpp`), anda e pula um objeto lendo ações pelo nome (`actionLeft`, `actionRight`, `actionJump`, `actionDrop`, que o prefab pode trocar). Veja `games/ikigai/assets/prefabs/player.prefab.json`: o personagem só tem esse componente.

## Atualização ao vivo (hot reload)

No Debug, salvar o `project.json` do jogo atualiza o jogo sem fechar. Se o arquivo estiver com erro, o jogo **mantém o projeto anterior** e avisa no terminal, no formato `Project: <arquivo>: <campo>: <motivo>`. Corrija e salve de novo.

## No painel F1

A seção **Projeto · Entrada** tem os controles dos dois números de `input` e do `buffer` de cada ação. Cada barra de `resources` tem os seus controles. Mudanças no F1 valem na hora, mas não são salvas: para guardar, copie o número para o `project.json`.
