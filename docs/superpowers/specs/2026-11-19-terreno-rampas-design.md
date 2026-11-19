# Mush2D — Terreno livre, rampas e plataformas atravessáveis (Fase 1.2)

Data: 2026-11-19
Status: rascunho para aprovação

## Intenção

O chão do jogo não pode ser só reto com degraus. Interiores e construções humanas têm **rampas em grade**, com ângulos fixos. Florestas têm **chão irregular e orgânico**, como em Ori. O jogador pode **atravessar plataformas por baixo** e **descer por elas** com ↓ + pulo. Tudo é montado no LDtk.

## Decisões do autor

| Tema | Decisão |
|---|---|
| Rampas | As duas formas: rampas em grade (interiores e construções) e terreno livre (florestas). |
| Visual do terreno livre | A engine gera a base a partir da linha (borda + preenchimento). Props e arte pintada vão por cima nos destaques. |
| Plataformas atravessáveis | Sim: pular por baixo e descer com ↓ + pulo. |
| Física | Controlador próprio por segmentos (sem Box2D). |
| LDtk | Continua como editor externo (sem fork). A integração vem por hot reload. |

## Objetivos

1. Rampas em grade no IntGrid `Collision`, pintadas visualmente pelas auto-rules do LDtk.
2. Uma entidade `Terrain` no LDtk: linha ou polígono desenhado com pontos, colisão em qualquer ângulo e visual gerado pela engine.
3. Plataformas atravessáveis, tanto no IntGrid quanto no `Terrain`.
4. Um controlador de personagem por segmentos que o player e os slimes usam. Ele anda em rampas sem escorregar parado, gruda no chão na descida e trata superfícies íngremes como parede com deslize.
5. Hot reload do `.ldtk` em Debug: salvar no LDtk recarrega o mundo sozinho.
6. Conteúdo de teste em `Forest2` (terreno livre) e `Cave1` (rampas em grade), sem tocar no nível `Showcase` do autor.

## Não-objetivos

- Física de corpos rígidos, empurrar objetos e rampas que se movem. Plataformas móveis ficam para depois.
- Pulo de parede e dash pela parede. São da Fase 4. O deslize em superfície íngreme já prepara o terreno para isso.
- Colisão por pixel do sprite.
- Edição do terreno dentro da engine. Toda a edição é no LDtk.

## Anti-objetivos

- O player "engasgar" ou travar em quinas entre segmentos, ou entre um segmento e um bloco.
- O player escorregar parado numa rampa caminhável, ou ficar quicando ao descer.
- Atravessar o chão em queda rápida (velocidade máxima de 18 u/s com `dt` limitado a 0,1).
- Mudar a vitrine visualmente ou quebrar a jogabilidade atual dela (andar, pular, pisar no slime, portas).

## Desenho

### Geometria de colisão (`CollisionWorld`, engine)

- O mundo de colisão é uma lista de **segmentos** por sala: `struct Segment { glm::vec2 a, b; glm::vec2 normal; bool oneWay; std::string roomIid; };`. A normal aponta para fora do sólido.
- Os segmentos são indexados na `SpatialGrid` que já existe, por AABB do segmento. Eles entram e saem junto com a sala (streaming).
- **Origem dos segmentos:**
  - **IntGrid `Collision`:**
    - `1` = sólido. Gera só as **arestas externas** dos blocos mesclados, sem arestas internas entre células sólidas vizinhas. Isso evita travar em "costuras".
    - `2` = plataforma atravessável: só a aresta de cima, `oneWay = true`.
    - `3`/`4` = rampa de 45° subindo para a direita / para a esquerda (uma célula).
    - `5`/`6` = rampa de 22,5° subindo para a direita / esquerda, metade baixa (sobe de 0 a ½ célula).
    - `7`/`8` = rampa de 22,5° subindo para a direita / esquerda, metade alta (sobe de ½ a 1 célula).
    - Uma rampa gera a diagonal mais as faces verticais e de baixo que ficam expostas.
    - Rampas vizinhas no mesmo sentido e com o mesmo ângulo são **fundidas num segmento só**.
  - **Entidade `Terrain`** (camada de entidades `Terrain`, com grade de 4 px para ter resolução fina; as entidades das outras camadas continuam em 16 px):
    - `points`: array de `F_Point`, em células de 4 px da camada. A posição da própria entidade é o primeiro ponto.
    - `closed` (bool, padrão falso). Fechado é um polígono. Aberto é uma superfície preenchida para baixo até `thickness`.
    - `thickness` (unidades, padrão 3), usado só quando aberto.
    - `oneWay` (bool, padrão falso). Uma superfície atravessável, por exemplo um galho. Só os trechos com normal para cima (`normal.y > 0,7`) ficam atravessáveis; os outros são ignorados para colisão.
    - `style` (enum `forest`/`cave`, padrão `forest`).
    - A ordem dos pontos define o lado de fora. A engine corrige a orientação, para que a normal aponte para o ar.
- **Objetos sólidos dinâmicos** (portas, slimes) continuam com AABB. O controlador trata uma AABB como 4 segmentos.

### Controlador de personagem (`CharacterController`, engine)

- Ele substitui a física de plataforma do `PlayerObject` (`stepPlatformer` e o movimento por bisseção) e é usado também pelo `SlimeEnemy`.
- **Corpo:** uma caixa (AABB) do tamanho do collider atual. A colisão da caixa contra segmentos tem os cantos de baixo arredondados com raio 0,1 u. Isso faz o corpo deslizar suavemente nas quinas de rampa.
- **Movimento:**
  - Sub-passos de no máximo 0,1 u de deslocamento cada. Isso, junto com a espessura mínima das geometrias, impede atravessar o chão.
  - Em cada sub-passo, o corpo é empurrado para fora de cada segmento que penetra, ao longo da normal.
  - A velocidade perde a componente que entra na superfície.
- **Chão:**
  - Uma superfície conta como chão quando `normal.y >= cos(50°)`, ou seja, inclinação de até 50°.
  - Andar no chão projeta a velocidade horizontal ao longo da tangente. A velocidade efetiva é igual à do plano: andar numa rampa não fica mais lento nem mais rápido.
  - Parado numa rampa caminhável, a gravidade não acumula. O corpo não escorrega.
  - **Snap ao descer:** se o corpo estava no chão no frame anterior, não pulou e existe chão até 0,3 u abaixo, ele é puxado para o chão. Assim não "voa" ao descer rampa nem ao passar de rampa para plano.
- **Íngreme** (inclinação acima de 50°): não é chão. O corpo desliza ao longo da superfície com a gravidade projetada e não pode pular a partir dela. O wall jump fica para a Fase 4.
- **Teto:** a velocidade vertical para ao bater a cabeça.
- **Plataforma atravessável:** um segmento `oneWay` só colide quando todas estas condições valem:
  1. a normal aponta para cima;
  2. o corpo está descendo;
  3. a base do corpo estava acima do segmento no início do frame.
- **Descer pela plataforma:** com ↓ + pulo em cima de uma plataforma atravessável, o corpo ignora **aquele** segmento por 0,25 s ou até a base do corpo passar abaixo dele. Em chão sólido, ↓ + pulo é um pulo normal.
- **API:** `MoveResult move(Body&, glm::vec2 velocity, float dt, const CollisionQuery&)`. O retorno informa a posição final, a velocidade final, `grounded`, a normal do chão, `onOneWay` e `hitCeiling`. `CollisionQuery` junta os segmentos da grade com as AABBs sólidas da cena, excluindo o próprio corpo.
- O stomp do slime continua funcionando: ele usa `grounded` e a normal do chão do resultado, mais a regra atual de "caindo e acima do centro".

### Visual do terreno livre (`TerrainMesh`, engine)

- O visual é gerado com o `SpriteBatch` que já existe, sem pipeline nem shader novo:
  - **Preenchimento:** colunas de 0,5 u de largura. Cada coluna vai da superfície até `thickness` (aberto) ou entre os cruzamentos do polígono (fechado), e é quebrada em células de 1 u. O `uvRect` de cada célula vem da posição no mundo, para a textura de terra repetir sem emenda.
  - **Borda:** pedaços retangulares girados ao longo de cada trecho com normal para cima (`normal.y > 0,3`). Têm a altura da textura de borda e se sobrepõem um pouco para cobrir as junções, com o `uv` correndo ao longo do comprimento da linha. Trechos laterais e de baixo usam uma segunda borda (raiz/pedra) do mesmo estilo.
- Pixel art: o terreno girado fica bom no modo suave, que é o padrão. No modo nítido, as bordas giradas podem serrilhar, e isso é aceito.
- **Normal map girado:** corrige a pendência antiga em que a normal do sprite não acompanhava `inst.rotation`. O `gbuffer.frag` passa a girar a normal amostrada pela rotação da instância. Sem isso, a borda inclinada recebe luz errada.
- **Texturas por estilo:** `assets/terrain/<style>/edge.png`, `side.png` e `fill.png`, cada uma com `_n` gerado.
  - `forest` é gerado a partir do `grass.png` do kaa por um script em `tools/`: a faixa de topo da grama vira a borda, as laterais viram `side` e o miolo de terra vira `fill`.
  - `cave` é derivado do mesmo material, mais escuro.
  - O autor troca depois pela arte definitiva.
- A malha é gerada quando a sala carrega e fica guardada por sala, como os tiles estáticos.

### Rampas em grade: visual

- No LDtk, as auto-rules da camada `Ground_d0` passam a pintar tiles de rampa a partir dos valores 3–8 do IntGrid. O tileset `grass.png` não tem tiles de rampa, então um script gera `assets/kaa/tilesets/grass_slopes.png` (16 px) com as 6 rampas a partir dos tiles existentes, cortando na diagonal. O autor pode redesenhar depois.
- As regras ficam prontas na camada de `Cave1` adicionada pelo script. O nível `Showcase` não é alterado.

### Hot reload (Debug)

- A cada 0,5 s, a engine verifica a data de modificação do `.ldtk` e dos `.region.json`. Se mudaram, recarrega como o F5, mantendo o `WorldState` e reposicionando o player onde estava.
- Um `.ldtk` inválido, por exemplo salvo pela metade, mantém a cena atual e mostra um aviso. Essa proteção já existe no recarregamento.

## Testes (`mush2d_tests`)

- `intgrid_segments_outer_edges_only`: um bloco 3×2 sólido gera 4 segmentos externos, sem arestas internas.
- `intgrid_slopes_and_oneway`: rampas de 45° e 22,5° (baixa e alta) nos dois sentidos; rampas vizinhas iguais são fundidas; o valor 2 gera só a aresta de cima, com `oneWay`.
- `terrain_points_to_world`: pontos de 4 px de uma entidade numa sala deslocada; a orientação é corrigida e as normais apontam para fora.
- `controller_walks_up_and_down_45`: chega ao topo sem perder o contato e desce sem ficar `grounded = false` em nenhum frame.
- `controller_rests_on_slope`: parado numa rampa de 45° por 2 s, desloca menos de 0,01 u.
- `controller_slides_on_steep`: numa rampa de 60° desliza e não pode pular.
- `controller_no_snag_on_seams`: anda por 10 blocos e 3 rampas encadeadas sem a velocidade horizontal cair a zero.
- `controller_oneway_from_below_and_drop`: atravessa a plataforma pulando por baixo, pousa em cima e, com ↓ + pulo, cai através dela.
- `controller_no_tunnel_at_max_fall`: cai a 18 u/s com dt = 0,1 sobre uma plataforma de 0,25 u de espessura e para em cima.
- `terrain_mesh_covers_columns`: uma superfície aberta gera colunas do topo até `thickness`, sem buracos.

## Critérios de pronto

1. Na vitrine, andar, pular, pisar no slime e as portas se comportam como antes. A aparência não muda.
2. Em `Forest2`, existe um chão orgânico com morros e um galho atravessável. O player sobe, desce e para em rampa sem escorregar, atravessa o galho por baixo e desce por ele com ↓ + pulo.
3. Em `Cave1`, há rampas de 45° e 22,5° e uma plataforma atravessável em grade.
4. Salvar no LDtk recarrega o mundo sozinho em Debug.
5. Os testes passam e não há nenhum erro de validação. O build não tem warnings.
6. `src/engine/**` não inclui `src/game/**`.

## Abordagens consideradas

1. **Controlador próprio por segmentos** ← escolhida pelo autor.
2. Box2D v3: robusto, mas difícil de deixar com boa sensação num platformer, além de ser mais uma dependência e um segundo sistema de colisão.
3. Rampas só em grade: limita os ângulos e impede o chão orgânico.

## Decisões do Claude (após revisão; o autor pode corrigir)

1. **Coordenadas do `Terrain`.** Usamos 32 px por unidade, como no resto da engine.
   - No LDtk, o `__value` de um `F_Point` é `{cx, cy}` em células da grade da **camada** (4 px), absoluto dentro do nível e não relativo à entidade.
   - Os pontos da linha são a posição da entidade seguida de `points`. Se `points[0]` cair na mesma célula da entidade, a duplicata é descartada.
   - Conversão: `ldtkToWorld(level.worldPx, {cx*4, cy*4})`.
   - O LDtk aceita grade diferente por camada. A camada `Terrain` (4 px) convive com as de 16 px no mesmo nível.
2. **Slimes e portas.**
   - Portas continuam como AABB sólida.
   - O slime usa o controlador para andar: sobe e desce rampas e pousa em plataformas atravessáveis.
   - A IA continua simples: o slime vira ao bater em parede, em superfície íngreme ou na **beira de um degrau**, quando não há chão 0,3 u à frente.
   - Entre player e slime não há colisão física, igual a hoje: o contato é trigger, mais o pulo em cima que mata o slime.
3. **Sensação de jogo.** O controlador replica a física atual de `stepPlatformer`: `moveSpeed 4.5`, `gravity -32`, `jumpSpeed 11`, `jumpCutFactor 0.5`, `maxFallSpeed -18`. Nada novo como coyote time ou buffer de pulo, que ficam para a Fase 4.
   - Em superfície íngreme, o input horizontal só afasta o corpo da parede, não empurra contra ela.
   - "Comportar como antes" na vitrine se mede assim: a altura do pulo continua em cerca de 3,8 células e o player pousa no mesmo y.
4. **Quinas e emendas.**
   - Na geração, segmentos colineares encostados são fundidos, inclusive entre IntGrid e IntGrid.
   - Em vértices entre segmentos que não são colineares, só valem as normais dos próprios segmentos. Não há "normal de vértice" de canto exposto quando o vértice é compartilhado por dois segmentos ("ghost vertex"). Assim o corpo não engasga numa emenda.
   - O algoritmo exato fica com o construtor, mas o teste `controller_no_snag_on_seams` precisa passar.
5. **Plataforma atravessável com trechos íngremes** (galho do `Terrain` com `oneWay`): os trechos sem normal para cima não colidem. Dá para atravessar o galho pelos lados, e isso é intencional.
   - Um corpo que aparece ou pousa *dentro* da espessura de uma plataforma atravessável não colide com ela até ficar inteiro acima. A regra 3 já garante isso.
6. **Fusão no IntGrid.**
   - `5` seguido de `7` no mesmo sentido forma uma rampa contínua de 22,5° ao longo de 2 células e é fundido.
   - Rampas vizinhas no mesmo sentido e ângulo também são fundidas.
   - Faces que encostam em célula sólida (valores 1, 3–8) não geram segmento.
   - O valor 2 encostado num 1 gera só o topo do 2.
7. **Bordas de sala.**
   - A colisão de cada sala é gerada independentemente, sem fundir com a sala vizinha.
   - Faces verticais que ficam exatamente na **borda externa da sala** não são geradas. Elas criariam paredes invisíveis e quinas na emenda entre salas.
   - Fechar o mundo nas bordas é responsabilidade do level design.
8. **Hot reload.**
   - O `WorldState` é mantido: portas, alavancas e inimigos mortos continuam como estavam.
   - O player fica na mesma posição. Se ela agora estiver dentro de sólido, o controlador tenta empurrá-lo para fora (até 2 u). Se não conseguir, o player reaparece no checkpoint.
9. **Normal girado.** A rotação é em torno do centro da instância, como no vertex shader. Nenhum sprite da vitrine é girado hoje (props e animais usam rotação 0), então a aparência atual não muda.

## Deixado para o construtor

- A organização interna de `CollisionWorld`, `CharacterController` e `TerrainMesh`.
- O algoritmo exato de penetração e resolução, desde que os testes e os anti-objetivos valham.
- O desenho dos morros em `Forest2` e das rampas em `Cave1`.
