# Ikigai e Mush2D: visão

Este documento registra como o Ikigai deve ser e quais sistemas a Mush2D precisa oferecer para que o autor construa o jogo. Ele vem antes do ROADMAP: o ROADMAP diz em que ordem os sistemas são feitos, e este documento diz o que eles são e por quê.

## Papéis

- **A engine (Mush2D)** oferece sistemas genéricos, a documentação (manual) e exemplos. Ela é feita pensando no que o Ikigai precisa, mas não contém nada que seja só do Ikigai.
- **O jogo (Ikigai)** é criado pelo autor, com dados (prefabs, LDtk, efeitos, diálogos) e com código de jogo quando for preciso. O Claude ajuda e explica, mas não escreve o conteúdo do jogo.
- **O exemplo (`examples/iki_demo/`)** é o código atual do Iki, do combate e dos bonecos de treino. Serve para mostrar como usar a engine.

### Regra de divisão

A engine responde **como** algo funciona. O jogo diz **o quê** existe.

| Engine | Jogo |
|---|---|
| Existem elementos, e um golpe tem um elemento. | Os elementos são Gelo e Fogo. |
| Existem barras de recurso com recarga. | O Iki tem uma barra de gelo e uma de fogo. |
| Superfícies têm um tipo que vem do LDtk. | A parede de gelo é escalável. |
| Ações de entrada com teclas configuráveis. | `X` ataca, `Z` pula. |
| Personagens têm memória, opiniões e agenda. | O lenhador fica bravo com queimadas sem sentido. |

Nada em `src/engine/` pode citar o Iki, Gelo, Fogo, o lenhador ou qualquer outro conteúdo do Ikigai.

## Os pilares

Ao terminar o jogo, o jogador deve:

1. **ficar perplexo com a grandeza do mundo;**
2. **ficar maravilhado com a beleza do jogo;**
3. **ficar impressionado com a profundidade dos personagens.**

Tudo isso acontece num **mundo vivo e com memória**: o que o jogador faz muda o mundo, e o mundo continua vivendo quando ele não está olhando.

## O mundo vivo

### Tempo

- **Relógio do mundo**, com dia e noite. Move as coisas pequenas: a grama que volta, as rotinas dos NPCs e dos animais, o clima.
- **Capítulos da história.** Movem as coisas grandes: a árvore plantada pelo lenhador que cresce, áreas restauradas ou devastadas, NPCs que mudam.

### Memória

- **Fatos do mundo:** cada acontecimento importante vira um registro, com o que aconteceu, quem causou e o contexto. Por exemplo: "fogo começou na área X, causado pelo Iki, sem combate por perto". Os fatos são salvos e qualquer sistema pode consultá-los.
- **Estado das áreas:** áreas queimadas, cortadas, esvaziadas ou restauradas continuam assim até mudarem.

### Simulação fora da tela

Como o mundo é dividido em salas, só as salas carregadas simulam ao vivo. Quando uma sala volta a ser carregada, ela calcula o que mudou pelo tempo que passou. Por exemplo: "faz 3 dias, a grama voltou"; "o lenhador começou a cortar a área ontem, já cortou metade". Assim o mundo inteiro tem memória e o jogo mantém o desempenho.

### Exemplo: a queimada

O Iki queima uma árvore perto de um rio.

- O fogo se espalha um pouco, como no Red Dead Redemption 2, sem destruir a sala inteira. Os animais por perto fogem.
- Se foi uma queimada sem sentido, fora de um combate contra um monstro, o lenhador reclama e fica bravo com o Iki.
- Se o Iki apaga o fogo antes de se espalhar, o lenhador diz que erros acontecem e o que importa é corrigir e não repetir.
- A vegetação demora a voltar. Mais tarde, o lenhador aparece olhando o estrago e planta uma árvore nova, que cresce ao longo da história.

## Natureza que reage

Água, neve, plantas e fogo conversam pelas mesmas regras de materiais: inflamável, congelável, derrete, cresce, boia, afunda.

### Fogo

Pega no que é inflamável e se espalha para o vizinho, com limite. Água e gelo apagam.

### Água

- Cachoeiras reagem quando o Iki passa.
- O Iki nada na superfície e debaixo d'água.
- Objetos boiam ou afundam conforme o material.
- Com uma habilidade, o Iki congela e derrete a superfície. Também pode haver passos que congelam a água, como no Minecraft.
- Puzzles de nível da água, como no Ori: fazer a água escoar libera uma área nova. O nível fica salvo.

### Neve

- deixa pegadas;
- acumula com o tempo;
- cai dos galhos quando o Iki passa;
- derrete com fogo e vira água ou poça;
- neve funda deixa o Iki mais lento.

### Plantas

- se curvam quando o Iki passa;
- podem ser cortadas;
- queimam e voltam a crescer;
- reagem a elementos (flores que se abrem com fogo, plantas que congelam, cogumelos que brilham);
- algumas servem de plataforma ou de mecânica.

### Cordas, cipós e correntes

Balançam com o vento e com quem encosta. O Iki se agarra, balança e pega impulso para soltar.

## Vida: animais e inimigos

- **Animais:**
  - têm rotina (comer, beber, dormir);
  - fogem do perigo;
  - caçam uns aos outros;
  - se assustam com o Iki conforme a espécie.
- **Inimigos:**
  - caçam animais;
  - defendem território;
  - brigam entre si (por exemplo, criaturas de fogo contra criaturas de gelo).
- **População:** os inimigos mortos voltam com o tempo, para o mundo não ficar vazio. Mas a população de cada área depende da história: uma área devastada pela narrativa perde os inimigos ou ganha outros tipos.

## Personagens

- **Diálogo com escolhas:** o jogador escolhe as respostas e os NPCs reagem a elas.
- **Vida própria:** a vida dos NPCs continua sem o Iki. Se o lenhador diz que vai cortar uma área, no dia seguinte ele está cortando, e muitos dias depois a área já foi cortada.
- **Relações:**
  - cada NPC tem história, personalidade, amigos, pessoas próximas e inimigos;
  - as relações entre NPCs continuam sem o Iki e avançam com a história.
- **Mudança:** com os capítulos, a aparência e o jeito de falar mudam conforme o que o NPC viveu. Um NPC alegre pode ficar triste depois de a guerra levar quem ele amava.
- **Expressão:** animações e reações variadas, e possivelmente dublagem.

## O Iki e a progressão

- **História:** Iki é filho de um pai do gelo e de uma mãe do fogo, que morreram defendendo uma revolução quando ele tinha uns 4 anos. Ele vive com um velho num campo de batalha esquecido e busca o seu propósito, o seu ikigai.
- **Combate e movimento:**
  - ataque e defesa com cada elemento e com os dois combinados;
  - estacas de gelo, pulo duplo de fogo, projéteis e outros poderes de travessia.
- **Desbloqueio de poderes:**
  - O Iki encontra pelo mapa estátuas e totens dos pais, ou de outra pessoa ou coisa, que contam a história deles.
  - Cada descoberta traz uma lembrança ou visão: algo que os pais fizeram, ou um momento deles com o Iki criança, passando valores e conselhos. A visão mostra a habilidade nova em uso, e o Iki a desbloqueia.
  - Certos poderes dão acesso a áreas novas.
- **Visões:** a maioria é cinematográfica, e algumas têm momentos jogáveis.
- **Itens e talismãs:** comprados, ganhos na história, dados por NPCs a partir de certo nível de amizade, ou recebidos por favores feitos.

## Interface, mapa e save

- Interface no estilo do Ori: discreta e bonita.
- Mapa no estilo do Ori: se revela ao explorar, com marcadores.
- Save em totens espalhados de forma estratégica pelo mapa. O save guarda o mundo inteiro: fatos, NPCs, relógio, capítulo, inventário e estado das áreas.

## Grandeza e beleza

- A câmera se afasta para mostrar vales enormes e criaturas gigantes.
- **Clima:** chuva, tempestade de neve, vento que move tudo, neblina.
- **Luz:** muda com o dia e a noite e com as estações.
- **Fundos:** várias camadas, com coisas acontecendo ao longe, como uma batalha da guerra ou uma criatura passando.
- **Música:** muda conforme o momento.
- Render no estilo do Ori (já existe).

## Sistemas que a engine oferece

| Grupo | Sistemas | Situação |
|---|---|---|
| Base | Separação entre engine e jogo; projeto em dados (elementos, barras de recurso, tipos de superfície, ações de entrada); `examples/` e `games/`; modelo inicial com personagem que anda e pula | a fazer |
| Já existe | Render no estilo Ori, prefabs e inspetor, máquinas de estado, efeitos e game juice, partículas, combate (hitbox, hurtbox, dano, projéteis), áudio, mundo contínuo, LDtk | feito, com os vazamentos a corrigir |
| Tempo e memória | Relógio com dia e noite, capítulos, fatos do mundo, estado das áreas, simulação fora da tela pelo tempo decorrido | a fazer |
| Natureza | Materiais com reações, fogo que se espalha, água (superfície, nado, empuxo, nível, congelar), cachoeiras, neve, plantas interativas, cordas e correntes agarráveis | a fazer |
| Vida | Percepção, IA por necessidades e rotinas, relações entre espécies e facções, territórios, população por área e capítulo | a fazer |
| Personagens | Diálogo com escolhas e condições, agenda e tarefas, rede social, memória e opinião, variantes por capítulo, voz e legendas | a fazer |
| Progressão | Habilidades por desbloqueio, inventário e talismãs, loja e moeda, favores e missões | parcial (habilidades) |
| Cenas | Cutscenes e visões (cinematográficas, com trechos jogáveis), troca temporária de personagem controlado | a fazer |
| Grandeza | Câmera dinâmica e cinematográfica, clima e vento, luz do dia e das estações, fundos vivos, música por momento | parcial (fundos, música por região) |
| Interface | HUD e menus no estilo Ori, mapa que se revela, totens de save, save completo | a fazer |

## Próximo passo

Reescrever o ROADMAP com fases da engine:

- a primeira fase separa engine e jogo e cria o modelo do `games/ikigai/`;
- as seguintes seguem os grupos acima, na ordem que o autor escolher;
- cada fase entrega o sistema, o manual e um exemplo pequeno de uso.
