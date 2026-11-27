# Mundo e salas

`assets/levels/showcase.ldtk` é um **mundo contínuo**: as salas são carregadas e descarregadas conforme o jogador anda, sem cortes. Quem faz isso é o `World` (veja [world](../sistemas/world.md)).

## Salas atuais

| Sala | Região | O que tem |
|---|---|---|
| `Showcase` | `forest` | A sala vitrine. |
| `Forest2` | `forest` | À direita da vitrine, pela passagem aberta na parede direita. Chão orgânico (`Terrain`) com dois morros, uma descida de ~30° e um galho atravessável sobre a descida. |
| `Cave1` | `cave` (mais escura) | Acima e à direita de `Forest2`; o chão continua a borda alta da direita de `Forest2`. Mesa com rampas de 45°, morro com rampas de 22,5° e plataforma atravessável com um slime em cima. |
| `IceTest` | `cave` | À esquerda da vitrine, pela porta na parede esquerda dela (fora do caminho principal). Sala de teste das estacas de gelo e do combate: veja abaixo. |

`Forest2` e `Cave1` foram geradas uma vez por `tools/add_test_rooms.py`, e o terreno e as rampas por `tools/add_terrain_content.py`. Cada uma tem chão, um slime que volta (`respawn` ligado), um que não volta, um item `permanent`, um item `respawn`, uma alavanca que abre uma porta e um `Checkpoint`. Edite tudo livremente no LDtk.

## A sala `IceTest`

A sala tem 64 x 40 células (1024 x 640 px). O chão fica na mesma altura do chão da vitrine. Da porta para a esquerda, ela tem:

1. o item `kind: "ability.ice.stakes"` no chão. Pegá-lo liga a flag global que desbloqueia as estacas;
2. um vão raso no chão, sob o teto de gelo;
3. uma parede de gelo de 8 u, a face direita de um pilar;
4. no alto do pilar, um teto de gelo de 6 u para a direita, sobre o vão. Não há chão nessa altura;
5. uma plataforma alta, 6 u acima do chão, sob o fim do teto, com um `Checkpoint`.

No chão entre a porta e o vão ficam os [bonecos de treino](../objetos.md#bonecos-de-treino): o atirador perto da porta e, embaixo da plataforma alta, o fraco a gelo e o resistente. Eles não ficam no caminho de escalada.

O percurso: pegue o item, pule contra a parede segurando `←` e suba com `↑`. No canto, solte `←` e continue com `↑` para pendurar no teto. Ande para a direita até o fim do teto e caia na plataforma. Sem o item, o pulo (~1,9 u) não alcança nada: a sala não tem outra subida.

A sala foi criada por `tools/add_ice_test_room.py`. O script pode rodar de novo: se tudo já existe, ele não muda nada. Ele acrescenta:

- o valor `9` na IntGrid;
- a sala e as entidades dela;
- os três bonecos de treino (D3): as definições `TrainingDummyWeak`, `TrainingDummyResist` e `TrainingDummyShooter` e uma instância de cada. Cada um entra só se ainda não existir;
- a porta da vitrine: as colunas 0 e 1, linhas 23 a 26 da camada `Collision`, com 8 células zeradas e os tiles de `Ground_d0` correspondentes removidos. A porta tem 2 u de altura, porque o Iki mede 1,6 u. Foi uma exceção aprovada pelo autor; o resto da vitrine não mudou.

## Marcar gelo no LDtk

- **IntGrid:** na camada `Collision`, pinte com um valor cujo nome (o *identifier* na definição da camada no LDtk) é o `intGrid` de uma superfície declarada em `surfaces` no `project.json`. No `iki_demo`, o valor `9` se chama `IceClimb` (azul-gelo `#7fd4ff`). Ele colide como o `1`, e todas as faces dele recebem a superfície. Paredes e tetos comuns continuam `1`. Pinte só a face que deve ter a superfície, como a coluna da direita do pilar da `IceTest`.
- **Terrain:** ligue o campo booleano que a superfície declara em `terrainFlag` (no `iki_demo`, `iceClimb`). Todos os segmentos da entidade recebem a superfície.
- **No `project.json`:** `"surfaces": [{"name": "IceClimb", "intGrid": "IceClimb", "terrainFlag": "iceClimb", "tags": ["climbable"]}]`. `tags` são rótulos livres que o código do jogo consulta (`activeProject().surfaceHas(id, "climbable")`). Um nome de valor que nenhuma superfície declara é ignorado (a célula fica vazia).

O gelo só agarra com a habilidade desbloqueada. Sem ela, é uma parede comum. Veja [Colisão e rampas](colisao-e-rampas.md) e [Iki](iki.md).

## Regiões

Uma **região** (`forest`, `cave`) define o fundo em parallax e o look (névoa, cor, bloom) de uma área. Ao passar de uma região para outra, a transição é suave. Cada região fica em `assets/regions/<nome>.region.json` e é editada pelo [painel F1](inspetor.md).

A região também escolhe a música, pelo campo `"music"` do `.region.json`:

```json
{ "look": { ... }, "layers": [ ... ], "music": "forest_theme" }
```

O nome aponta para `assets/audio/music/<nome>.wav`. Ao trocar de região, o [`Audio`](../sistemas/audio.md) faz a transição cruzada para a música nova em `musicFade` (1 s). Duas regiões com a mesma música não a reiniciam. Sem `"music"`, a região fica em silêncio (a música anterior desce). Hoje, `forest` toca `forest_theme` e `cave` toca `cave_theme`. Salvar a região pelo painel mantém o campo.

## Estado do mundo

O `WorldState` lembra o que morreu, o que foi coletado e o que está aberto, indexado pelo id da entidade no LDtk. Por isso uma porta aberta continua aberta quando você sai da sala e volta.

A persistência de cada objeto vem do `PersistentComponent`:

| Política | Efeito |
|---|---|
| `None` | Nada é lembrado: o objeto volta sempre que a sala carrega. |
| `Permanent` | Depois de removido (morto, coletado), nunca volta. |
| `Respawn` | Volta quando a sala dele está carregada, o ponto onde ele nasceu está fora da tela, e o jogador está a mais de `respawnDistance` (padrão 12 unidades) desse ponto. |

No LDtk, a política vem dos campos da entidade. Um campo bool `respawn` vence: `true` → `Respawn`, `false` → `Permanent`. Senão vale o campo texto `persistence` (`permanent`, `respawn` ou `none`). O campo numérico `respawnDistance` muda a distância.

## Morte e respawn

Ao morrer (no Debug, a tecla `K` na vitrine força a morte):

1. a tela escurece;
2. a sala da morte volta ao estado de quando o jogador entrou nela;
3. o jogador reaparece no último `Checkpoint` ativado, ou no `PlayerStart`.

`R` na vitrine também é uma morte (escurece e restaura a sala). Nas cenas Sandbox e Test, `R` só leva o player ao checkpoint.

## Casos de uso

### Criar uma sala nova

1. Abra o `.ldtk` no LDtk e crie um nível encostado nos outros.
2. Pinte a camada `Collision` (1 = sólido, 2 = plataforma, 3 a 8 = rampas, 9 = gelo escalável; veja [Colisão e rampas](colisao-e-rampas.md)).
3. Pinte os tiles e salve.
4. Com o jogo rodando em Debug, ele recarrega sozinho. Ande até a sala nova.

### Um inimigo que volta depois de um tempo longe

Na entidade `Slime` do LDtk, ligue o campo `respawn` e ponha `respawnDistance` = `20`. O slime volta quando o jogador fica a mais de 20 unidades do ponto onde ele nasceu.

### Um baú que só pode ser aberto uma vez

Use a política `Permanent` (campo `persistence` = `permanent`, ou `respawn` = `false`) na entidade. Depois de removido, ele não volta nem ao recarregar a sala.
