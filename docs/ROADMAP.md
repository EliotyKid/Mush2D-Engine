# Mush2D: Roadmap da engine

Revisão de 2026-11-25. A Mush2D é a engine **feita para o Ikigai**. Ela oferece os sistemas, e o autor cria o jogo.

- **O jogo e os sistemas que ele pede:** [VISAO.md](VISAO.md). Este ROADMAP diz em que ordem os sistemas são feitos.
- **Papéis:**
  - a engine (`src/engine/`) oferece os sistemas genéricos, o manual (`manual/`) e exemplos (`examples/`);
  - o jogo (`games/ikigai/`) é criado pelo autor;
  - o Claude constrói a engine e ajuda no jogo, mas não escreve o conteúdo do Ikigai.

## Fluxo de trabalho (decisões do autor)

- A facilidade do **GameMaker** com a organização visual da **Godot**.
- O destino final é uma IDE própria, com um fork do LDtk adaptado e um editor visual de objetos, animações e atributos. **Não é prioridade agora.** O caminho até lá é gradual:
  1. objetos descritos em arquivo (prefabs) — feito;
  2. inspetor em árvore no painel F1 — feito;
  3. editor de animações no painel;
  4. só depois a IDE.

## Princípios

1. **Engine diz como, jogo diz o quê.**
   - Nada em `src/engine/` cita o Iki, Gelo, Fogo, personagens ou qualquer conteúdo do Ikigai.
   - Elementos, barras de recurso, tipos de superfície e ações de entrada são definidos pelo projeto, em dados.
2. **Feita para o Ikigai.** A engine só ganha sistemas que o Ikigai precisa (metroidvania 2D com render no estilo Ori, mundo vivo e com memória). Não é uma engine para qualquer gênero.
3. **Conteúdo em dados, não em código.** Inimigos, NPCs, itens, diálogos, reações de materiais e cutscenes ficam em arquivos (prefabs, tabelas e roteiros). O C++ do jogo entra só para um **comportamento** realmente novo.
4. **Uma fonte de verdade para o estado.** Tudo o que persiste vive no `WorldState`, por ids estáveis. O save serializa só isso.
5. **Sistemas conversam por eventos e tags, não por referências diretas.** Um sistema novo assina eventos que já existem sem mexer nos antigos.
6. **Interface no tamanho do jogo final, implementação no tamanho do que se usa agora.**
7. **Cada fase começa com um design** em `docs/superpowers/specs/`, com a seção obrigatória "Como cresce até o jogo final".
8. **Cada fase entrega três coisas:** o sistema, a página do manual e um exemplo pequeno em `examples/`. O exemplo do Iki é atualizado quando a engine muda.
9. **Testes por sistema** e **zero erros de validação** como critério de pronto.

Marque `[x]` ao concluir.

---

## Já feito

### Base técnica

- [x] Render no estilo Ori: G-buffer, luz com normal map, névoa por profundidade, aditivo, bloom, grading e pixel art suave com zoom livre. [design](superpowers/specs/2026-11-18-sala-vitrine-design.md)
- [x] Painel F1 de look por região, salvo em `.region.json`.
- [x] Modelo de entidades: ids seguros, criação e destruição seguras, pai/filho, componentes, eventos e grade espacial. [design](superpowers/specs/2026-11-19-modelo-entidades-design.md)
- [x] Mundo contínuo: salas do LDtk com streaming, regiões, `WorldState`, respawn por distância e morte que restaura a sala. [design](superpowers/specs/2026-11-19-mundo-continuo-design.md)
- [x] Terreno livre, rampas e plataformas atravessáveis, com controlador de personagem por segmentos e hot reload do LDtk. [design](superpowers/specs/2026-11-19-terreno-rampas-design.md)
- [x] Guia para iniciantes ([GUIA.md](GUIA.md)) e manual em MkDocs (`manual/`).

### Fases D1 a D3 (antes da separação entre engine e jogo)

- [x] **D1, prefabs e inspetor:** reflexão (`MUSH2D_EXPORT`), prefabs com herança, entidades do LDtk por prefab, atlas lidos da pasta, inspetor em árvore no F1, salvar no prefab, hot reload. [design](superpowers/specs/2026-11-23-d1-prefabs-inspetor-design.md)
- [x] **D2, movimento:** máquinas de estado (várias por objeto, estados compartilhados), tempos de pulo, superfícies escaláveis, recursos, efeitos reaproveitáveis (biblioteca e gatilhos no prefab), partículas, squash, piscar, inclinação, eventos de animação. [design](superpowers/specs/2026-11-24-d2-iki-move-design.md)
- [x] **D3, combate e peso:** hitbox, hurtbox, dano com multiplicadores por elemento, atordoamento, projéteis com pool, hit stop, tremor de câmera, flash, partículas com transparência iluminadas, áudio (miniaudio) com música por região. [design](superpowers/specs/2026-11-25-d3-combate-design.md)

O Iki, o combate de gelo, os bonecos de treino e a sala `IceTest` foram feitos nessas fases. Na E0 eles viram o exemplo `examples/iki_demo/`.

### Pendente, no PC Arch

- [ ] build e testes no Linux (incluindo o áudio);
- [ ] redimensionar a janela sem nenhum erro de validação;
- [ ] medir a RTX 2060 Super (`MUSH2D_PERF_LOG=10`).

---

## Fases da engine

Para cada fase:
- **Entra** é o que a engine passa a oferecer.
- **Exemplo** é o que vai para `examples/`.
- **Pronto quando** é o critério de conclusão.

### E0: Base — engine e jogo separados

**Entra:**
- [x] Projeto em dados (`project.json` do jogo):
  - elementos (no lugar do `Element` fixo);
  - barras de recurso com nome (no lugar de `maxIce`/`maxFire`);
  - tipos de superfície pelo nome do valor da IntGrid (no lugar de `Surface::IceClimb`);
  - mapa de ações de entrada com teclas e gamepad configuráveis (no lugar dos botões fixos do `InputState`).
- [x] Remover da engine os comentários e nomes que citam o Iki, o kaa ou o Ikigai.
- [x] Pastas `examples/iki_demo/` (o código, os prefabs, os efeitos, os sons e a sala atuais) e `games/ikigai/`.
- [x] O executável escolhe qual jogo abrir.
- [x] Modelo inicial de `games/ikigai/`:
  - uma sala vazia no LDtk;
  - um personagem que anda e pula, feito só com componentes da engine;
  - o `project.json` com os elementos e as ações do Ikigai.
- [x] Manual: "Criando o seu jogo", "Projeto em dados" e a regra de divisão entre engine e jogo.

**Estado:** feita no branch `feat/e0-base` (ainda sem merge no `main`).

**Exemplo:** o `iki_demo` inteiro, adaptado aos dados do projeto.

**Pronto quando:** o `iki_demo` funciona igual a antes; `games/ikigai/` abre com o personagem andando e pulando; `src/engine/` não cita conteúdo do Ikigai (verificado por um teste ou script).

### E1: Tempo e memória

**Entra:**
- [ ] Relógio do mundo com dia e noite (velocidade configurável, pausa em cutscenes).
- [ ] Capítulos da história como marcos que o jogo avança.
- [ ] Fatos do mundo: registro de acontecimentos com causa e contexto, consultável por qualquer sistema e salvo.
- [ ] Estado das áreas (queimada, cortada, esvaziada, restaurada) com tempo de recuperação.
- [ ] Simulação fora da tela: ao carregar uma sala, ela calcula o que mudou pelo tempo que passou.
- [ ] Save completo do mundo (fatos, relógio, capítulo, áreas, entidades persistentes), em totens ou por chamada do jogo.
- [ ] Resolve a pendência "itens permanentes reaparecem ao morrer".

**Exemplo:** uma grama que, cortada, volta depois de N horas do relógio, mesmo com a sala descarregada.

**Pronto quando:** fatos e estados sobrevivem a salvar e carregar; uma sala revisitada mostra a passagem do tempo.

### E2: Natureza

**Entra:**
- [ ] Materiais com reações em dados (inflamável, congelável, derrete, cresce, boia, afunda) e uma tabela de reações entre elementos e materiais.
- [ ] Fogo que se espalha para vizinhos com limite e se apaga com água ou gelo.
- [ ] Água: volumes do LDtk, superfície com ondas e respingos, nado na superfície e mergulho, empuxo e arrasto, congelar e derreter, nível animado e salvo, cachoeiras que reagem à passagem.
- [ ] Neve: pegadas, acúmulo, queda dos galhos, derreter em água ou poça, neve funda que deixa lento.
- [ ] Plantas: curvar com a passagem, cortar, queimar e voltar a crescer (E1), reagir a elementos, servir de plataforma.
- [ ] Cordas, cipós e correntes: física de corda, balanço com vento e contato; o personagem se agarra, balança e pega impulso.
- [ ] Eventos de natureza para os outros sistemas (por exemplo, "fogo começou", com causa).

**Exemplo:** uma sala com rio, cachoeira, ponte de cordas, neve e plantas inflamáveis; o exemplo do Iki congela a água e queima a vegetação.

**Pronto quando:** cada material reage como descrito na visão; o estado da natureza persiste pela E1; o desempenho se mantém (medido).

### E3: Câmera e cenas

**Entra:**
- [ ] Câmera dinâmica: zonas de câmera do LDtk, enquadramento, zoom suave, afastar para mostrar a escala, várias câmeras e transições. Mantém o tremor do D3.
- [ ] Cutscenes e visões em roteiro (dados): câmera, personagens, falas, música, efeitos e filtro de memória; disparadas por objetos do mapa; com trechos jogáveis e troca temporária do personagem controlado.
- [ ] Desbloqueio de habilidade como passo de uma cena.

**Exemplo:** uma estátua que mostra uma visão cinematográfica com um trecho jogável e libera uma habilidade do exemplo.

**Pronto quando:** uma cena roda do começo ao fim, pode ser pulada, e o jogo volta ao estado certo.

### E4: Personagens

**Entra:**
- [ ] Diálogo com escolhas e condições (fatos, opinião, capítulo, humor), com voz, legendas e expressões.
- [ ] Agenda e tarefas dos NPCs com começo e duração, simuladas fora da tela (E1).
- [ ] Rede social: relações entre NPCs e com o jogador, que mudam por fatos e pela história.
- [ ] Memória e opinião: o NPC lembra do que viu ou soube e julga o contexto.
- [ ] Variantes por capítulo e vivência: aparência, humor e tom das falas.

**Exemplo:** o lenhador da visão, reagindo à queimada (com e sem combate, apagada ou não) e plantando a árvore que cresce por capítulo.

**Pronto quando:** o exemplo do lenhador funciona do começo ao fim, com a vida dele continuando fora da tela.

### E5: Vida

**Entra:**
- [ ] Percepção: visão, audição e calor, com alcance e linha de visão.
- [ ] IA por necessidades e rotinas (fome, sede, sono, medo, caça), usando as máquinas de estado.
- [ ] Relações entre espécies e facções em dados (quem caça, quem foge, quem briga com quem).
- [ ] Territórios.
- [ ] População por área, capítulo e estado do mundo, com retorno ao longo do tempo (no lugar do respawn simples).

**Exemplo:** cervos que bebem água e fogem do fogo, um predador que os caça e dois tipos de inimigo que brigam entre si.

**Pronto quando:** as criaturas do exemplo vivem, caçam e fogem sem script; a população muda com o capítulo.

### E6: Progressão

**Entra:**
- [ ] Inventário e itens.
- [ ] Talismãs e equipamentos que mudam os números do personagem, com espaços para equipar.
- [ ] Loja e moeda.
- [ ] Favores e missões com estado (aceito, feito, recompensado), ligados à amizade (E4) e aos fatos (E1).

**Exemplo:** um talismã ganho por amizade e outro comprado, mudando o exemplo do Iki.

**Pronto quando:** itens, talismãs e favores sobrevivem a salvar e carregar.

### E7: Atmosfera

**Entra:**
- [ ] Clima: chuva, tempestade de neve, neblina e vento que move partículas, plantas, cordas e água.
- [ ] Luz pelo relógio do dia e pelas estações.
- [ ] Fundos vivos: camadas com coisas acontecendo ao longe.
- [ ] Música por momento (exploração, combate, cena), com transições.

**Exemplo:** um dia completo no exemplo, com uma tempestade de neve passando.

**Pronto quando:** clima e luz mudam com o tempo sem custo visível de desempenho.

### E8: Interface

**Entra:**
- [ ] HUD e menus no estilo do Ori.
- [ ] Mapa que se revela ao explorar, com marcadores.
- [ ] Totens de save.
- [ ] Remapear teclas e gamepad (sobre as ações da E0).

**Exemplo:** o HUD, o mapa e um totem de save no exemplo do Iki.

**Pronto quando:** o jogo do exemplo pode ser jogado só com menus, mapa e save, sem o painel F1.

---

## Decisões tomadas

- **Controle (E0):** teclado e gamepad são iguais (as mesmas ações, no `project.json`), com cuidado especial com o gamepad (zona morta, gatilhos, desligar no meio do jogo).

## Decisões em aberto

Cada uma é decidida no design da fase indicada.

| Decisão | Fase | Opções |
|---|---|---|
| Formato das cutscenes | E3 | JSON próprio editado no painel, ou o mesmo formato dos diálogos |
| Formato dos diálogos | E4 | JSON próprio, Yarn Spinner ou ink |

As decisões de jogo (por exemplo, o modelo de energia do Iki ou a origem da arte) são do autor, no `games/ikigai/`, e não deste ROADMAP.

## Pendências técnicas registradas

- O `TeleportObject` zera a velocidade.
- A sala atual pode alternar de um lado para o outro quando o player fica em cima da borda.
- Salvar o look grava valores que ninguém editou.
- Itens permanentes reaparecem ao morrer. Resolver na E1.
- Não há aviso quando o chão de salas vizinhas fica em alturas diferentes.
- Áudio: mudar o volume só afeta sons que começam depois; uma terceira música no meio de uma transição corta a mais antiga; não há limite de vozes.
- Partículas: a comparação de profundidade com o G-buffer é imprecisa a partir da profundidade 2 (R16F); ajustar se surgirem camadas distantes.
