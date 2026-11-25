# Mush2D: Roadmap do Ikigai

Revisão de 2026-11-23. A engine deixa de ser genérica e passa a servir a um jogo: **Ikigai**.

## O jogo

Ikigai é um metroidvania em pixel art com render no estilo Ori (luz, névoa, bloom), com cenas cinematográficas e grandiosas.

- **Mundo.** Pessoas controlam poderes elementais. Este jogo mostra as civilizações do **fogo** e do **gelo**, em guerra. Os outros elementos aparecem só como referência, para jogos futuros.
- **Protagonista.** Iki é filho de um romance proibido entre gelo e fogo. Os pais lideraram uma revolução que unia quem tinha poderes e quem não tinha. Morreram protegendo os seus ideais e o filho, quando ele tinha cerca de 4 anos. Iki cresce com um senhor num antigo campo de batalha: um campo desolado, com partes verdes, ruínas e uma história esquecida.
- **Tema.** Iki descobre que domina **os dois** elementos, o único a conseguir isso. A história é a busca pelo seu propósito (ikigai): amizades, inimizades e decisões que mudam o mundo.

## Gameplay (decisões do autor)

- **Poderes.** Fogo, gelo e os dois combinados servem para **combate, movimento e puzzles**.
  - Gelo: estacas para escalar certas paredes e tetos, projéteis, ataque e defesa.
  - Fogo: pulo duplo, projéteis, ataque e defesa.
  - Combinados: ataques e defesas próprios.
  - Poderes específicos para atravessar certas áreas.
  - **Desbloquear poderes abre áreas novas.**
- **Mundo elemental.** O gelo criado pelo jogador derrete. O fogo queima certas áreas. Se o jogador agir de forma "otimizada", **a natureza recupera a área**.
- **Equipamento.** Roupas e itens que melhoram atributos, no estilo dos amuletos de Hollow Knight.
- **NPCs complexos.** Têm personalidade e reações. A forma como Iki os trata muda como eles falam e lidam com ele. As decisões importantes influenciam o mundo.

## Fluxo de trabalho (decisões do autor)

- A facilidade do **GameMaker** com a organização visual da **Godot**.
- O destino final é uma IDE própria, com um fork do LDtk adaptado e um editor visual de objetos, animações e atributos. **Não é prioridade agora.** O caminho até lá é gradual:
  1. objetos descritos em arquivo (prefabs);
  2. inspetor em árvore no painel F1;
  3. editor de animações no painel;
  4. só depois a IDE.

## Princípios: crescer sem refatorar

O roadmap é guiado pela demo: cada fase entrega algo jogável e constrói a **versão mínima** do que a demo precisa. Para que cada sistema cresça até o jogo final sem reescrita, vale o seguinte em todas as fases.

1. **Interface no tamanho do jogo final, implementação no tamanho da demo.**
   - Exemplo: o dano carrega um `Element` (Fogo, Gelo, Combinado, Nenhum) desde o primeiro dia, mesmo que a demo só use Gelo.
   - Cada sistema começa com os tipos e os pontos de extensão que o jogo final vai usar, e só implementa o que a demo exige.
2. **Conteúdo em dados, não em código.** Inimigos, NPCs, itens, diálogos, reações elementais e cutscenes ficam em arquivos (prefabs, tabelas e roteiros). Conteúdo novo não exige C++. O C++ entra só para um **comportamento** realmente novo.
3. **Uma fonte de verdade para o estado.**
   - Tudo o que persiste (poderes, equipamento, relações com NPCs, flags de decisão, áreas queimadas ou recuperadas, inimigos mortos) vive no `WorldState`, indexado por ids estáveis.
   - O save serializa só isso, então nenhum sistema novo precisa de "código de save" próprio.
4. **Sistemas conversam por eventos e tags, não por referências diretas.** Exemplos: "o fogo tocou vegetação" e "NPC viu Iki queimar a vila". Assim, um sistema novo assina eventos já existentes sem mexer nos antigos.
5. **Máquinas de estado reutilizáveis.** Uma só implementação serve ao player, aos inimigos, aos chefes e aos NPCs.
6. **Cada fase começa com um design** em `docs/superpowers/specs/`, com uma seção obrigatória: **"Como cresce até o jogo final"**. Ela lista o que fica de fora e por onde isso vai entrar sem mudar a interface.
7. **Testes por sistema** e **zero erros de validação** como critério de pronto. Fica de fora tudo o que não tiver uso na demo.

`src/engine/` continua sendo a tecnologia (render, mundo, física, dados). `src/game/` são as regras do Ikigai (elementos, poderes, NPCs).

Marque `[x]` ao concluir.

---

## Já feito: a base técnica

- [x] Render no estilo Ori: G-buffer, luz com normal map, névoa por profundidade, aditivo, bloom, grading e pixel art suave com zoom livre. [design](superpowers/specs/2026-11-18-sala-vitrine-design.md)
- [x] Painel F1 de look por região, salvo em `.region.json`.
- [x] Modelo de entidades: ids seguros, criação e destruição seguras, pai/filho, componentes, eventos e grade espacial. [design](superpowers/specs/2026-11-19-modelo-entidades-design.md)
- [x] Mundo contínuo: salas do LDtk com streaming, regiões, `WorldState`, respawn por distância e morte que restaura a sala. [design](superpowers/specs/2026-11-19-mundo-continuo-design.md)
- [x] Terreno livre, rampas e plataformas atravessáveis, com um controlador de personagem por segmentos e hot reload do LDtk. [design](superpowers/specs/2026-11-19-terreno-rampas-design.md)
- [x] Reorganização: regiões, observador de arquivos e helpers do LDtk movidos para a engine.
- [x] Guia para iniciantes: [GUIA.md](GUIA.md)

**Pendente, no PC Arch:**
- [ ] build e testes no Linux;
- [ ] redimensionar a janela sem nenhum erro de validação;
- [ ] medir a RTX 2060 Super (`MUSH2D_PERF_LOG=10`).

---

## Rumo à demo

**Demo:** 10 a 20 minutos no **campo de batalha esquecido**. Iki pega o primeiro poder (**gelo**), enfrenta alguns inimigos, conversa com o **senhor que o cria**, vê uma **cutscene** e luta contra um **chefe**.

Para cada fase abaixo:
- **Entra** é a versão mínima da demo.
- **Cresce para** é o que vem depois, pelos mesmos pontos de extensão.
- **Pronto quando** é o critério de conclusão.

### D1: Fluxo de criação (prefabs e inspetor)

Vem primeiro porque tudo o que vier depois será criado com ele.

**Entra:**
- [x] **Prefabs** (`.prefab.json`): tipo C++ base, atributos (números, textos, cores e referências), nós filhos (luz, hitbox, partícula, sprite extra), componentes e animações por estado.
- [x] **Herança de prefab** (`base: "enemy_base"`), o equivalente a um objeto pai.
- [x] **Registro automático dos tipos C++**: uma linha por classe, sem editar o `IkigaiGame`.
- [x] **Entidade do LDtk apontando para um prefab.** Os campos do LDtk sobrescrevem os atributos do prefab.
- [x] **Atlas e animações lidos da pasta pelo nome**, sem registro à mão em `GameAssets.cpp`.
- [x] **Inspetor em árvore no F1**: a cena ao vivo (objeto → filhos → componentes) com os atributos editáveis e um botão Salvar no prefab.
- [x] **Projeto Ikigai separado da vitrine**: `IkigaiGame` passa a ser o jogo padrão, e a vitrine vira uma sala de testes.

**Cresce para:**
- editor de animações no painel;
- validação de prefabs com um esquema versionado;
- IDE e fork do LDtk.

**Pronto quando:** um inimigo novo nasce copiando um prefab e mudando números, sem recompilar, e o inspetor mostra e salva os atributos dele.

### D2: Iki se move (estados e poderes de movimento)

**Entra:**
- [x] **Máquina de estados** reutilizável, usada pelo Iki: parado, correndo, pulando, caindo, preso na parede, escalando (parede e teto), ferido, morto. O estado de ataque entra no D3, como uma classe nova.
- [x] **Sistema de habilidades**: cada poder é um componente, e o desbloqueio fica no `WorldState` (`ability.ice.stakes`).
- [x] **Gelo: estacas.** Cria pontos de apoio para escalar **certas** paredes e tetos, marcados como superfície escalável no LDtk.
- [x] **Recursos**: vida e energia elemental. Decidido no D2: uma barra para cada elemento (gelo, fogo), e a travessia não gasta energia (veja "Decisões resolvidas").
- [x] **Ajustes finos do pulo**: coyote time, buffer de pulo e altura variável.
- [x] **Animação por estado** com eventos por frame (passo, golpe), usando as tags do Aseprite.

**Cresce para:**
- fogo: pulo duplo;
- dash;
- poderes de travessia;
- combinações.

Tudo isso entra como novos componentes de habilidade e novos estados, sem mudar a máquina.

**Pronto quando:** o Iki escala uma parede de gelo para chegar a uma área antes inacessível. Feito na sala `IceTest` do `showcase.ldtk`.

### D3: Combate e "peso" (feedback)

**Entra:**
- [ ] **Hitbox e hurtbox** como nós filhos do prefab.
- [ ] **`DamageInfo`** com quantidade, `Element`, knockback e origem. **Resistências e fraquezas por elemento** ficam no prefab.
- [ ] **Ataque corpo a corpo de gelo**, **projétil de gelo** (com pool de projéteis) e **defesa de gelo**.
- [ ] **Invencibilidade breve ao ser atingido**, **hit stop** (congelar o frame no impacto) e **tremor de câmera** simples.
- [ ] **Partículas básicas**, na CPU, para o essencial: estilhaços de gelo, poeira e impacto.
- [x] **Áudio básico** com miniaudio: efeitos e música por região.

**Cresce para:**
- **efeitos de status** como componentes (congelado, queimando);
- combos;
- partículas na GPU;
- áudio posicional;
- música adaptativa.

**Pronto quando:** um combate contra um alvo de teste tem impacto, som e partículas, e o dano respeita as fraquezas.

### D4: Inimigos

**Entra:**
- [ ] **IA por máquina de estados** com dados no prefab: patrulha, percepção (alcance e linha de visão), perseguição, ataque, recuo.
- [ ] **2 ou 3 inimigos** do campo de batalha (proposta: uma criatura terrestre, uma voadora e uma que reage ao gelo).
- [ ] Respawn pelas regras que já existem.

**Cresce para:**
- inimigos das duas civilizações;
- comportamentos novos como estados novos;
- grupos.

**Pronto quando:** dá para lutar contra 3 tipos de inimigo na sala de teste.

### D5: Mundo elemental mínimo

**Entra:**
- [ ] **Materiais** nas superfícies e objetos (vegetação, madeira, água, gelo e pedra), definidos no LDtk e nos prefabs.
- [ ] **Tabela de reações elemento × material em dados**. Na demo: o gelo do jogador derrete com o tempo, e o projétil de gelo congela água, que vira plataforma.
- [ ] **Estado das reações no `WorldState`**, para que persistam.

**Cresce para:**
- fogo queimando vegetação e madeira;
- **recuperação da natureza** quando o jogador age de forma "otimizada", como um estado de recuperação por área;
- novos pares na tabela, sem código novo.

**Pronto quando:** um puzzle da demo exige congelar água para passar.

### D6: Câmera dinâmica

**Entra:**
- [ ] **Zonas de câmera** como entidades retangulares no LDtk, com limites, foco, zoom e travamento de eixo.
- [ ] **Antecipação** na direção do movimento, suavização e tremor.
- [ ] **Várias câmeras com transição suave** entre elas.

**Cresce para:**
- câmeras em trilhos;
- enquadramento de dois alvos.

**Pronto quando:** a câmera enquadra bem as salas da demo e uma arena trava a câmera.

### D7: Texto, diálogo e NPCs

**Entra:**
- [ ] **Renderização de texto em pixel art** (fonte bitmap) e caixas de diálogo.
- [ ] **Diálogos em arquivo** com falas condicionais (o formato está em aberto, no fim deste documento) e escolhas.
- [ ] **NPC**: personalidade, **relação com o Iki** (confiança, afinidade) e **memória de eventos**, tudo no `WorldState`. As falas mudam conforme esse estado.
- [ ] **Flags de decisão do mundo**, que qualquer sistema pode ler.
- [ ] **O senhor que cria o Iki**, como o primeiro NPC.

**Cresce para:**
- NPCs que reagem ao que viram, por eventos;
- consequências de decisões no mundo;
- localização para PT e EN.

**Pronto quando:** a conversa com o senhor muda conforme uma escolha anterior.

### D8: Cutscenes

**Entra:**
- [ ] **Linha do tempo em arquivo** com trilhas: câmera, mover e animar atores, diálogo, luz e look, fade, áudio e esperar input.
- [ ] **Letterbox.**
- [ ] **Disparo por gatilho do LDtk.**
- [ ] **A cutscene da demo**: a lembrança dos pais ou o despertar do gelo.

**Cresce para:**
- trilhas novas, como partículas e música;
- cutscenes que dependem de flags;
- pular cutscene.

**Pronto quando:** a cutscene da demo roda, do início ao fim, a partir de um gatilho no mapa.

### D9: Interface e controles

**Entra:**
- [ ] **HUD**: vida e energia.
- [ ] **Pausa** e **menu de equipamento** mínimo, com 2 ou 3 itens no estilo dos amuletos.
- [ ] **Gamepad** e remapeamento.

**Cresce para:**
- mapa-múndi;
- inventário completo;
- opções;
- acessibilidade.

**Pronto quando:** a demo é jogável do início ao fim só com o gamepad.

### D10: Save e progressão

**Entra:**
- [ ] **Save do `WorldState` inteiro em slots**: posição, poderes, equipamento, relações, flags e mapa.
- [ ] **Pontos de save**, no estilo dos bancos.
- [ ] **Gating por habilidade** validado no mapa.

**Cresce para:**
- versões de save, para que os saves antigos continuem abrindo;
- autosave.

**Pronto quando:** sair do jogo e continuar mantém tudo.

### D11: Chefe

**Entra:**
- [ ] **Chefe como prefab com fases** (máquina de estados).
- [ ] **Arena**: a câmera trava e as portas fecham.
- [ ] **Barra de vida** do chefe.
- [ ] **Uma cutscene** curta de entrada.

**Pronto quando:** o chefe tem pelo menos 2 fases e a luta é vencível, justa e legível.

### D12: A demo

**Entra:**
- [ ] **O mapa do campo de batalha esquecido**: salas, regiões e look.
- [ ] Os momentos da história na ordem certa.
- [ ] Polimento visual: vagalumes, folhas e poeira.
- [ ] **Teste de jogo** com outras pessoas.
- [ ] **Desempenho**: 60 fps na Intel UHD e no Steam Deck.
- [ ] **Build de release** para Windows e Linux.

**Pronto quando:** uma pessoa de fora termina a demo sem ajuda.

---

## Depois da demo (em linhas gerais)

- **Fogo e combinações:** pulo duplo, projéteis, defesa, ataques combinados e travessias.
- **Fogo no mundo e recuperação da natureza.**
- **Equipamento e atributos completos.**
- **NPCs e decisões em escala:** várias facções, consequências no mundo e finais.
- **As duas civilizações**: novas regiões e chefes.
- **Mapa-múndi.**
- **Visual avançado:**
  - sombras 2D;
  - luz volumétrica e god rays;
  - partículas na GPU;
  - vento na vegetação;
  - água com reflexo;
  - LUT de cor por região.
- **Áudio avançado**: posicional por profundidade, reverberação por região e música adaptativa.
- **Ferramentas**: editor de animações, visualização de colliders e IA, profiler e, por fim, **IDE com fork do LDtk e editor visual de objetos**.
- **Scripts** (Lua), só se os dados e o C++ virarem gargalo.
- **Distribuição**: empacotamento e Steam.

---

## Decisões em aberto

Cada uma será decidida no design da fase indicada.

| Decisão | Fase | Opções |
|---|---|---|
| Formato dos diálogos | D7 | JSON próprio, Yarn Spinner ou ink |
| Formato das cutscenes | D8 | JSON próprio editado no painel, ou o mesmo formato dos diálogos |
| Origem da arte da demo | D12 | Arte nova do Ikigai, ou o kaa como placeholder |
| Controle principal | D9 | Gamepad primeiro, ou teclado primeiro |

## Decisões resolvidas

| Decisão | Fase | Resolução |
|---|---|---|
| Energia elemental | D2 | Uma barra para cada elemento (`Element::Ice`, `Element::Fire`), com recarga própria. A travessia (escalar, pendurar no teto) nunca gasta energia; a energia fica para o combate e as combinações. |

## Pendências técnicas registradas

- O `TeleportObject` zera a velocidade.
- ↓ é ao mesmo tempo "interagir" e "descer de plataforma".
- A sala atual pode alternar de um lado para o outro quando o player fica em cima da borda.
- Salvar o look grava valores que ninguém editou.
- Itens permanentes reaparecem ao morrer, o que permite duplicar itens quando houver inventário. Resolver em D10.
- Não há aviso quando o chão de salas vizinhas fica em alturas diferentes.
