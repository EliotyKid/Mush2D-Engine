# Mundo e salas

`assets/levels/showcase.ldtk` é um **mundo contínuo**: as salas são carregadas e descarregadas conforme o jogador anda, sem cortes. Quem faz isso é o `World` (veja [world](../sistemas/world.md)).

## Salas atuais

| Sala | Região | O que tem |
|---|---|---|
| `Showcase` | `forest` | A sala vitrine. |
| `Forest2` | `forest` | À direita da vitrine, pela passagem aberta na parede direita. Chão orgânico (`Terrain`) com dois morros, uma descida de ~30° e um galho atravessável sobre a descida. |
| `Cave1` | `cave` (mais escura) | Acima e à direita de `Forest2`; o chão continua a borda alta da direita de `Forest2`. Mesa com rampas de 45°, morro com rampas de 22,5° e plataforma atravessável com um slime em cima. |

As duas salas de teste foram geradas uma vez por `tools/add_test_rooms.py`, e o terreno e as rampas por `tools/add_terrain_content.py`. Cada uma tem chão, um slime que volta (`respawn` ligado), um que não volta, um item `permanent`, um item `respawn`, uma alavanca que abre uma porta e um `Checkpoint`. Edite tudo livremente no LDtk.

## Regiões

Uma **região** (`forest`, `cave`) define o fundo em parallax e o look (névoa, cor, bloom) de uma área. Ao passar de uma região para outra, a transição é suave. Cada região fica em `assets/regions/<nome>.region.json` e é editada pelo [painel F1](inspetor.md).

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

`R` volta ao checkpoint sem morrer.

## Casos de uso

### Criar uma sala nova

1. Abra o `.ldtk` no LDtk e crie um nível encostado nos outros.
2. Pinte a camada `Collision` (1 = sólido, 2 = plataforma, 3 a 8 = rampas; veja [Colisão e rampas](colisao-e-rampas.md)).
3. Pinte os tiles e salve.
4. Com o jogo rodando em Debug, ele recarrega sozinho. Ande até a sala nova.

### Um inimigo que volta depois de um tempo longe

Na entidade `Slime` do LDtk, ligue o campo `respawn` e ponha `respawnDistance` = `20`. O slime volta quando o jogador fica a mais de 20 unidades do ponto onde ele nasceu.

### Um baú que só pode ser aberto uma vez

Use a política `Permanent` (campo `persistence` = `permanent`, ou `respawn` = `false`) na entidade. Depois de removido, ele não volta nem ao recarregar a sala.
