# Colisão e rampas

A colisão do cenário vem da camada IntGrid `Collision` de cada sala do LDtk (células de 16 px). A engine transforma as células em linhas (`CollisionWorld`), e o `CharacterController` anda sobre elas.

## Valores do IntGrid

| Valor | Significado |
|---|---|
| `1` | sólido |
| `2` | plataforma atravessável: só o topo colide, e só de cima |
| `3` / `4` | rampa de 45° subindo para a direita / esquerda |
| `5` / `6` | rampa de 22,5°, metade baixa (sobe de 0 a ½ célula), para a direita / esquerda |
| `7` / `8` | rampa de 22,5°, metade alta (sobe de ½ a 1 célula), para a direita / esquerda |
| `9` | gelo escalável: colide como o `1`, e as faces recebem a superfície `IceClimb` (as estacas de gelo do Iki agarram parede e teto) |

Uma rampa de 22,5° ocupa 2 células: `5` seguida de `7` (subindo para a direita) ou `8` seguida de `6` (descendo). Rampas vizinhas iguais viram um segmento só. Faces de superfícies diferentes não se juntam: uma fileira `1` `9` tem dois topos, um comum e um de gelo.

Num `Terrain`, o campo booleano `iceClimb` faz o mesmo que o `9`: todos os segmentos da entidade viram `IceClimb`.

!!! warning "Bordas da sala"
    Faces verticais na borda da sala não colidem. Feche o mundo no level design.

Para chão em qualquer ângulo, use a entidade [Terrain](terreno.md).

## Tiles de rampa (auto-rules)

`assets/kaa/tilesets/grass_slopes.png` (gerado por `tools/gen_terrain_textures.py`) é o `grass.png` com mais uma linha (linha 6) com os tiles de rampa dos valores `3` a `8`, nessa ordem. A camada `Ground_d0` de `Cave1` usa esse tileset (override da camada) e tem os tiles colocados à mão pelo script.

Para pintar rampas sozinho no LDtk:

1. Camadas `Tiles` como `Ground_d0` não têm regras. Em *Project layers*, crie uma camada *Auto-layer* (por exemplo `Slopes_d0`, logo acima de `Ground_d0`), com tileset `GrassSlopes` e fonte (*Source IntGrid layer*) `Collision`.
2. Crie um grupo de regras. Para cada valor de `3` a `8`, uma regra 1×1: a célula central igual ao valor e, como tile, o da linha 6 na mesma ordem (valor `3` = primeira coluna).
3. Apague os tiles de rampa colocados à mão em `Ground_d0` de `Cave1`: agora a camada nova os pinta. Os tiles do `1` em volta continuam como estão (bordas de grama).
4. Para pintar também o sólido por regra, faça o mesmo com o `1` (9-slice do `grass.png`, nas mesmas posições do `GrassSlopes`).

!!! note "Scripts que editam o `.ldtk`"
    `tools/add_terrain_content.py` escreve o `.ldtk` no mesmo layout do LDtk (`tools/ldtk_json.py`) e recusa rodar se o arquivo não fizer o caminho de ida e volta byte a byte.

## Casos de uso

### Uma subida suave de 22,5° para a direita

Pinte `5` e depois `7` lado a lado, na mesma linha, com sólido (`1`) embaixo. Repita o par uma célula acima e à direita para continuar subindo.

### Uma plataforma que o jogador atravessa por baixo

Pinte uma fileira de `2`. O jogador sobe por baixo e fica em cima. Para descer, `↓` + pulo.

### Uma parede de gelo para escalar

Pinte uma coluna de `9` onde o Iki deve agarrar. O resto da sala continua com `1`. Para um teto de gelo, pinte a fileira de baixo do teto com `9`.

### Uma mesa com rampas dos dois lados

`3` à esquerda, `1` no meio e `4` à direita, todos na mesma linha. É o que `Cave1` usa.
