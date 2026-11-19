# Mush2D — Mundo contínuo (Fase 1, primeiro item)

Data: 2026-11-19
Status: rascunho para aprovação

## Intenção

O metroidvania precisa de um mundo grande, feito de várias salas, que o jogador percorre **sem cortes**, como em Ori. Não há tela preta nem troca de cena ao cruzar de uma sala para outra. As salas são montadas no LDtk. A engine carrega as vizinhas antes do jogador chegar e descarrega as distantes. O mundo lembra o que o jogador fez: inimigos mortos, itens coletados, portas abertas.

## Decisões do autor

| Tema | Decisão |
|---|---|
| Conexão entre salas | Mundo contínuo (estilo Ori), não transição por sala. |
| Inimigos | Campo para definir se volta: com `respawn` ligado, volta quando o jogador estiver a uma certa distância do spawn; desligado, nunca volta. |
| Itens | Depende do tipo de item. |
| Portas e alavancas | Permanecem como o jogador deixou. |
| Morte | O jogador volta a um ponto predefinido. A sala onde ele estava volta ao estado de quando ele entrou nela. |
| Arquitetura | Uma única cena, com objetos marcados pela sala de origem (abordagem 1). |

## Objetivos

1. Um projeto LDtk com várias salas posicionadas no mundo (`worldX`/`worldY`). A engine usa coordenadas de mundo únicas.
2. Streaming: ficam carregadas a sala atual e as que encostam nela; as demais são descarregadas, com histerese.
3. Atravessar a fronteira entre salas é só andar. Colisão, física, câmera e objetos não percebem a troca.
4. Estado persistente por `iid` (`WorldState`), com regras de respawn de inimigos e de itens.
5. Morte: o jogador volta ao checkpoint e a sala da morte é restaurada à "foto" tirada na entrada.
6. Regiões: fundos de parallax e look (névoa, cor, bloom) pertencem a uma região que abrange várias salas, sem saltos nas emendas. O look muda suavemente ao trocar de região.
7. A vitrine continua funcionando, agora como uma sala do mundo, com mais 2 salas de teste conectadas.

## Não-objetivos (ficam para os próximos itens do roadmap)

- Câmera com zonas e limites por sala, e múltiplas câmeras. Nesta etapa, a câmera é limitada ao retângulo do mundo inteiro.
- Save em disco. O `WorldState` é desenhado para ser serializado depois, mas a escrita em arquivo fica para o item de save.
- Mapa-múndi.
- Transição cruzada (fade) entre fundos de regiões diferentes. Nesta etapa a emenda entre regiões deve ser escondida pelo level design (túnel, caverna, parede). O fade de verdade depende de sprites translúcidos iluminados (Fase 3).
- Inimigos que perseguem o jogador para outra sala com persistência entre salas. Ao descarregar a sala de origem, o inimigo vai junto.

## Anti-objetivos

- Qualquer engasgo visível ao entrar numa sala. A criação de uma sala tem que caber no frame (alvo: menos de 2 ms na Intel UHD).
- Perder edições feitas pelo autor no `showcase.ldtk`.
- Quebrar a vitrine, o Sandbox ou o Test.

## Desenho

### LDtk e coordenadas

- O loader passa a ler **todos** os níveis do projeto: `identifier`, `iid`, `worldX`, `worldY`, `pxWid`, `pxHei`, e o campo de nível `region` (texto). O `LdtkLevel` ganha `worldPx` (ivec2) e `region`.
- Conversão para o mundo, com um único eixo Y para cima: `world = ((worldX + px.x) / 32, -(worldY + px.y) / 32)`. Isso substitui a conversão por nível de hoje, `(pxHei - y)`. Testes cobrem os dois cantos de uma sala deslocada.
- O layout do mundo no LDtk pode ser *Free* ou *GridVania*. A engine só usa as posições.
- As camadas de parallax **saem das salas** e passam para as regiões (ver abaixo). As salas mantêm `Entities`, `Foreground_d-2` (props), `Collision` e `Ground`. Uma camada de tiles com `_d` diferente de 0 dentro de uma sala continua funcionando, mas ancorada no mundo (o centro da sala deixa de ser referência).

### Salas e streaming (`World`, engine)

- `class World` (engine, `src/engine/world/World.*`):
  - carrega o `.ldtk` uma vez (todas as salas parseadas);
  - mantém o conjunto de salas carregadas;
  - expõe `currentRoom()`, `roomAt(worldPos)` e `update(focusPos)`.
- **Vizinhança:** duas salas são vizinhas quando os retângulos se tocam ou se sobrepõem, com tolerância de 1 px. É calculada na carga a partir das posições, sem depender de `__neighbours`.
- **Conjunto carregado:** a sala que contém o jogador mais as vizinhas dela.
- **Histerese:** uma sala só é descarregada quando não é vizinha da sala atual **e** o jogador está a mais de 4 unidades da borda dela. Isso evita carregar e descarregar repetidamente quando o jogador fica parado na fronteira.
- **Carregar uma sala:**
  - cria os sprites estáticos (tiles) daquela sala no `LevelLoader`, agora indexados por sala;
  - cria os colliders do IntGrid;
  - chama as fábricas das entidades.
  - Todo objeto criado recebe `roomIid`, um novo campo em `GameObject`.
- **Descarregar:**
  - destrói os objetos com aquele `roomIid` e remove os sprites estáticos da sala.
  - Objetos com `persistent = true` sobrevivem (jogador, câmera, o que o jogador carrega). O jogador nunca tem `roomIid`.
- **Troca de sala de um objeto:** só o jogador e objetos marcados `followsPlayer` são "adotados" pela sala nova. Os demais pertencem para sempre à sala de origem, inclusive um inimigo que cruzou a borda.
- **Orçamento:** a sala inteira é criada num frame.
  - `// shortcut:` sem fatiamento entre frames. Se uma sala passar de cerca de 2 ms, fatiar por número de entidades por frame.
  - A carga acontece quando a sala vira vizinha, ou seja, bem antes de entrar na tela.

### Regiões (fundo e look)

- Cada região é um arquivo `assets/regions/<nome>.region.json`. Ele contém:
  - `look`: o mesmo formato do `.look.json` de hoje, com ambient, névoa, bloom, grading e pixel art;
  - `layers`: uma lista de `{ image, depth, parallax: [x, y], offset: [x, y], repeatX: bool }`. As imagens são PNG com `_n/_e/_m` opcionais. O offset é em unidades de mundo, ancorado na origem do mundo.
- A camada de região é desenhada como sprites repetidos na horizontal (`repeatX`) só no trecho visível. A emenda entre salas da mesma região é perfeita por construção.
- A região atual é a da sala atual. Ao trocar de região, o look é interpolado em 1,5 s. As camadas da região nova entram e as da antiga saem quando a troca acontece (ver não-objetivos: sem fade).
- O painel F1 passa a editar e salvar a **região atual** (look e camadas). O `showcase.look.json` atual vira `assets/regions/forest.region.json`, com as camadas bg1..bg5 convertidas a partir das camadas de parallax da vitrine (mesma imagem, profundidade, parallax e posição), para que a aparência não mude.

### Estado persistente (`WorldState`, engine)

- `WorldState` é um mapa de `iid` para um `EntityState`:
  - `bool gone`: morto ou coletado;
  - `double goneTime`;
  - `glm::vec2 spawnPos`;
  - `std::map<std::string, double> values`: estado genérico, como `"open"` → 1.
- API:
  - `isGone(iid)`, `markGone(iid, spawnPos)`, `clearGone(iid)`;
  - `get(iid, key, default)`, `set(iid, key, value)`.
- **Política por entidade** (campos do LDtk, lidos pela fábrica):
  - `persistence`: `"permanent"`, `"respawn"` ou `"none"`. O padrão é `"none"`.
  - `respawnDistance`: número em unidades. O padrão é 12.
  - Inimigos: o campo do autor `respawn` (booleano) é mapeado para `persistence = respawn ? "respawn" : "permanent"`.
- **Regras:**
  - `permanent`: depois de `markGone`, a fábrica nunca mais cria o objeto.
  - `respawn`: o objeto volta quando estas condições valem ao mesmo tempo:
    - o jogador está a mais de `respawnDistance` do `spawnPos`;
    - o `spawnPos` está fora da tela;
    - a sala do objeto está carregada.
    - O `World` verifica isso a cada 0,5 s e chama a fábrica de novo com a entidade original.
  - `none`: não é registrado e volta sempre que a sala recarrega.
- **Componente `PersistentComponent { iid, policy, respawnDistance }`.** As fábricas o anexam aos objetos que têm `iid`. Quando o objeto chama `kill()`/`collect()` (helper do jogo que marca e destrói), o estado é gravado.
- **Interativos** (portas, alavancas): leem `get(iid, "open")` no `onCreate` e gravam `set` ao mudar. Ficam como o jogador deixou, porque o estado está fora da sala.

### Morte e foto da sala

- Ao entrar numa sala (a sala atual mudou), o `World` tira uma **foto**: uma cópia das entradas do `WorldState` cujas entidades pertencem àquela sala. Isso exige um índice de `iid` → sala, montado na carga do `.ldtk`.
- `World::onPlayerDeath(respawnPoint)`:
  1. restaura a foto da sala da morte, apagando entradas criadas depois e voltando as alteradas;
  2. recarrega essa sala (descarrega e carrega);
  3. teleporta o jogador para o `respawnPoint`;
  4. faz o streaming em volta do destino de forma síncrona, atrás de um fade curto (0,4 s), porque o destino pode estar longe.
- **Ponto de respawn:** o último `Checkpoint` ativado, que é uma entidade do LDtk e grava no `WorldState` (`"activated"`). Sem checkpoint ativado, vale o `PlayerStart`.
- A vitrine ainda não tem dano. O teste de morte usa uma tecla de debug (K = morrer), ativa só em Debug.

### Câmera nesta etapa

- O limite (`bounds`) da câmera é o retângulo que envolve **todas as salas**. A câmera segue o jogador livremente pelo mundo.
- Zonas e limites por sala ficam para o próximo item (câmera dinâmica).

### Conteúdo de teste

- O `showcase.ldtk` ganha mais 2 salas, adicionadas por um script único que **acrescenta** níveis ao arquivo existente sem alterar o nível `Showcase`, preservando as edições do autor:
  - uma sala à direita, mesma região;
  - uma sala acima à direita, região `cave`, com um look mais escuro.
  - Cada uma tem chão, plataformas, 1 slime com `respawn` ligado, 1 slime com `respawn` desligado, 1 item `permanent`, 1 item `respawn`, 1 alavanca que abre uma porta, e 1 `Checkpoint`.
- O autor depois edita tudo livremente no LDtk.

## Testes (`mush2d_tests`)

- `ldtk_world_coordinates`: uma sala com `worldX = 512` e `worldY = -256`; os cantos convertidos batem com a fórmula.
- `world_neighbours_by_touching_rects`: salas encostadas, salas sobrepostas e salas separadas por 2 px.
- `world_streaming_loads_neighbours_and_unloads_with_hysteresis`: um jogador simulado andando por 3 salas em linha, com objetos de teste e sem GPU.
- `world_unload_keeps_persistent_objects`.
- `worldstate_permanent_never_respawns` e `worldstate_respawn_by_distance_offscreen`.
- `interactive_state_survives_unload`: a porta é aberta, a sala é descarregada e recarregada, e a porta continua aberta.
- `death_restores_room_snapshot`: o jogador entra na sala, mata um inimigo permanente, abre a porta e morre. A sala volta ao estado da entrada; as outras salas mantêm o estado.
- `region_look_blend`: interpolação do look em 1,5 s.

## Critérios de pronto

1. Andar da vitrine até as 2 salas novas e voltar sem nenhum corte visível. O GPU timer não mostra pico ao carregar sala; os picos de tempo de CPU ficam registrados no log com `MUSH2D_PERF_LOG`.
2. Inimigos, itens, alavanca e porta seguem as regras acima ao sair e voltar.
3. A tecla K (morte) restaura a sala e leva ao checkpoint.
4. A troca de região muda o look suavemente.
5. Testes passam. Zero erros de validação. A aparência da vitrine não muda.
6. `src/engine/**` não inclui `src/game/**`.

## Abordagens consideradas

1. **Uma cena com objetos marcados por sala** ← escolhida pelo autor. Reaproveita o modelo de entidades novo: criação e destruição seguras, coordenadas de mundo e grade.
2. Uma cena por sala mais uma cena global. Isola as salas, mas colisão, eventos e câmera passam a atravessar várias cenas.
3. Streaming por blocos fixos do mundo, independentes das salas. Não casa com o LDtk e é exagero para o tamanho de um metroidvania.

## Decisões do Claude (após revisão; autor pode corrigir)

1. **Objetos de jogo mínimos entram no escopo, só o necessário para testar.**
   - `DoorObject`: bloqueia quando fechada e não bloqueia quando aberta. Lê e grava `"open"`.
   - `LeverObject`: trigger. Ao encostar e apertar ↓, alterna o próprio estado e o da porta ligada pelo campo `door` (referência de entidade do LDtk, `F_EntityRef`, que resolve para o `iid` da porta).
   - `ItemObject`: coletado ao encostar. Campos `kind` (texto) e `persistence`. Não há inventário ainda; a coleta só registra o estado.
   - `CheckpointObject`: reaproveita o de hoje e ativa ao encostar. Grava `"activated"` e passa a ser o ponto de respawn.
   - `SlimeEnemy` (já existe): morre quando o jogador pula em cima dele (stomp: o jogador está caindo e a base do jogador está acima do centro do slime). É o dano mínimo, sem sistema de combate.
2. **O script de salas de teste pode acrescentar definições** em `defs`: as entidades acima, os campos `persistence`, `respawnDistance`, `respawn` e `door`, e o campo de nível `region`. Também acrescenta os níveis novos. O conteúdo do nível `Showcase` (camadas e entidades) não é alterado; só o valor do campo `region` é preenchido com `forest`. Sala sem `region` usa `forest`.
3. **Persistência só para entidades com o campo `persistence`, ou `respawn` nos inimigos.** Luzes, cristais, props e animais são `none` e são recriados a cada carga.
4. **Sala atual:**
   - É a sala que contém o centro do jogador.
   - Se mais de uma sala contém o centro (sobreposição), continua valendo a atual enquanto ela ainda o contiver.
   - Se nenhuma sala contém o centro (vão no mundo), mantém a última.
5. **Câmera:** pode mostrar áreas vazias num mundo em forma de L. Isso é aceito até o item de câmera dinâmica.
6. **Tiles por sala:**
   - O `LevelLoader` guarda os sprites estáticos indexados por sala.
   - `appendStatic` percorre só as salas carregadas.
   - Tiles com `_d` diferente de 0 dentro de uma sala são ancorados na origem do mundo, como as camadas de região.
7. **Fade:**
   - Um fator `fade` (0..1) em `FrameSettings`/`FrameUniforms`, multiplicado na cor final do `composite.frag`.
   - Animado pelo `World` na morte: escurece em 0,2 s, faz o streaming síncrono e clareia em 0,2 s.
8. **Respawn de inimigos e itens:** o `World` guarda o `LdtkEntity` original de cada `iid` persistente enquanto o projeto está carregado, e chama a fábrica de novo com ele.

## Deixado para o construtor

- Organização interna de `World`, `WorldState` e do registro de regiões.
- Formato exato do `.region.json`, desde que contenha `look` e `layers` como descrito.
- Layout visual das 2 salas de teste.
