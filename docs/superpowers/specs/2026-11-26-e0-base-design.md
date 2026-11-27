# E0: Base — engine e jogo separados

Fase E0 do [ROADMAP](../../ROADMAP.md). Base: `main` em `2ca9366`, mais as duas correções de build do Linux (`Scene2D.hpp`, `FileWatcher.cpp`) que vieram para o branch `feat/e0-base`.

## Intenção

Hoje a engine e o exemplo do Iki estão misturados: `src/engine/` tem o `Element` fixo (`None, Ice, Fire`), `maxIce`/`maxFire`, `Surface::IceClimb` e botões com nome de jogo no `InputState`. O `main.cpp` só abre o `IkigaiGame`.

A E0 separa as duas coisas para o autor começar o Ikigai de verdade:

- a engine diz **como** (sistemas genéricos);
- cada jogo diz **o quê**, num `project.json` com elementos, barras de recurso, superfícies e ações de entrada;
- o código atual vira o exemplo `examples/iki_demo/`, que funciona igual a antes;
- nasce o modelo `games/ikigai/`, de onde o autor cria o jogo.

## Decisões do autor

1. **Teclado e gamepad, com uma ótima experiência no gamepad.** O autor quer controles responsivos e simples de entender, mesmo com combos e combinações de poderes.
2. **Esquema de poderes pensado pelo autor (é conteúdo do jogo, não da E0):**
   - atacar sem segurar nada: golpe físico (mão e chute);
   - segurar L1: ataques e defesa de fogo; segurar L2: de gelo;
   - o combo vem de alternar L1 e L2 durante o ataque;
   - precisa ser testado para ver se fica divertido. Por isso a entrada tem que ser fácil de mudar sem recompilar.
3. **Combinações geram efeitos novos, como no Spellbreak**, nos dois casos: quando os poderes se encontram no mundo e dentro do golpe (alternando L1/L2).
4. **Escolher o jogo ao abrir.** No começo será mais o `IkiDemo` (validar a engine); depois, mais o `Ikigai`.
5. **Abordagem A** (abaixo): mover primeiro, depois trocar os vazamentos um por vez.
6. **Comentários explicando o que cada parte faz**, em todo código novo ou mexido, para o autor (iniciante em C++) conseguir ler.
7. Propostas aceitas sem mudança: buffer por ação, gatilho analógico como botão, analógico com zona morta, uma tabela de reações única (na E2), um programa de desenvolvimento com tela de escolha, `--game`, e o modelo do `games/ikigai/`.

## Objetivos

1. **`project.json` por jogo**, lido pela engine ao abrir o jogo, com hot reload no Debug:
   - `elements`: lista aberta de nomes (o `iki_demo` declara `Ice` e `Fire`);
   - `resources`: barras com nome, máximo, recarga e atraso, cada uma opcionalmente ligada a um elemento;
   - `surfaces`: tipos de superfície com propriedades (por exemplo `climbable`), ligados pelo nome do valor da IntGrid do LDtk;
   - `actions`: ações com teclas, botões e eixos do gamepad, buffer por ação, limite dos gatilhos e zona morta do analógico.
2. **Entrada por ações**, no lugar dos botões fixos do `InputState`:
   - cada ação informa `pressed`, `down` e `released`, e um `buffered` (apertada nos últimos N ms, ainda não consumida);
   - teclado e gamepad alimentam a mesma ação; o jogo não sabe de qual aparelho veio;
   - gamepad pelo `glfwGetGamepadState` do GLFW (mapeamento padrão), sem biblioteca nova;
   - gatilhos (L2/R2) viram botão a partir de um limite; analógico com zona morta, também lido como direção digital;
   - o `keepEdges` do hit stop continua valendo para todas as ações.
3. **Elementos em dados:** o `Element` fixo sai de `Damage.hpp`. `DamageInfo`, `Hitbox`, `Projectile` e os multiplicadores do `Damageable` usam o elemento do projeto (prefabs continuam escrevendo `"Ice"`).
4. **Barras em dados:** o `Resources` troca `maxIce`/`maxFire` e `spendBoth` por barras com nome vindas do projeto. O `EnergyChanged` passa a dizer qual barra mudou.
5. **Superfícies em dados:** `Surface::IceClimb` e o `v == 9` fixo saem do `CollisionWorld`. O campo `iceClimb` do `Terrain` vira um campo genérico de superfície.
6. **Pastas e executável:**
   - `examples/iki_demo/` com o código, os prefabs, os efeitos, os sons, os atlas e a sala atuais;
   - `games/ikigai/` com o modelo inicial;
   - um programa de desenvolvimento `Mush2D` que abre com uma tela de escolha (lista das pastas com `project.json`), lembra a última escolha (Enter abre direto) e aceita `--game <nome>`.
7. **Modelo `games/ikigai/`:** uma sala vazia no LDtk, um personagem que anda e pula feito só com componentes da engine, e o `project.json` com `Fire`, `Ice` e as ações (incluindo L1 e L2).
8. **A engine não cita o Ikigai:** remover de `src/engine/` os comentários e nomes com Iki, kaa, Ikigai, gelo e fogo (hoje são 48 ocorrências), e um teste que confere isso e confere que `src/engine/` não inclui nada de `examples/` nem de `games/`.
9. **Manual e documentação:** páginas "Criando o seu jogo", "Projeto em dados" (com a entrada) e a regra de divisão; README, `docs/GUIA.md` e ROADMAP atualizados; `mkdocs build --strict` passando.

## Não-objetivos

- A tabela de reações entre elementos e materiais (fica na E2, junto com os materiais).
- O esquema L1/L2 e os golpes do Ikigai: são conteúdo do jogo, feitos pelo autor.
- Remapear teclas dentro do jogo e ícones de botões na tela (E8).
- Vibração do gamepad.
- O executável de distribuição do Ikigai sem a tela de escolha (só quando chegar a hora de distribuir).
- Zoom inicial da câmera em dados (o autor pediu para deixar como está por enquanto).

## Anti-objetivos

- Qualquer mudança na sensação de jogo do `iki_demo`: tempos de pulo, combate, peso e câmera têm que ficar iguais.
- Mexer no conteúdo do `showcase.ldtk` (ele só muda de pasta; os caminhos relativos para os tilesets continuam válidos).
- Um mapa de ações que só funciona bem no teclado, com o gamepad como remendo.
- Nomes de jogo voltando para `src/engine/` por outra porta (por exemplo, valores padrão como `"Ice"` dentro da engine).

## Abordagens consideradas

- **A. Mover primeiro, depois trocar um vazamento por vez** (escolhida). Cada passo deixa o `iki_demo` rodando e os testes passando, então quando algo quebra se sabe qual passo foi.
- **B. Dados primeiro, mover no fim.** Mesmo resultado, mas a mudança grande de pastas fica no fim, com muito código novo misturado, e fica difícil achar o que quebrou.
- **C. Estrutura nova do zero, reescrevendo o Iki.** Mais limpa no papel, mas arrisca perder a sensação ajustada no D2/D3 e torna "funciona igual a antes" difícil de provar.

## Ordem (abordagem A)

1. **Mover:** `src/game/` vai para `examples/iki_demo/src/`, os assets do demo para `examples/iki_demo/assets/`. CMake, `all_objects.cpp`, testes e `assetPath` passam a olhar a pasta do jogo. Nasce a tela de escolha e o `--game`. O `iki_demo` roda igual.
2. **`project.json` e elementos.**
3. **Barras de recurso.**
4. **Superfícies.**
5. **Ações de entrada e gamepad** (o maior passo; resolve também a pendência "↓ é interagir e descer de plataforma").
6. **Limpeza da engine** e o teste da regra de divisão.
7. **Modelo `games/ikigai/`.**
8. **Manual e documentação.**

## Escolhas do Claude (após a revisão do construtor)

O autor pode mudar qualquer uma delas.

1. **Vários jogos num programa só.** Cada jogo é uma biblioteca estática (`iki_demo`, `ikigai`) com uma função de registro explícita, que fica numa tabela de jogos compilados. Ao escolher o jogo, só os tipos dele entram no registro de tipos. Assim, um `PlayerObject` de um jogo não colide com o de outro, e os prefabs de um jogo não enxergam os tipos de outro. A geração de `all_objects.cpp` vira uma por jogo. A tela de escolha lista só os jogos compilados. Uma pasta com `project.json` mas sem código compilado aparece com aviso e não abre.
   - **Ajuste no plano:** o C++ registra os tipos na partida do programa, em qualquer arquivo que inclua o header. Por isso não dá para separar o registro por jogo sem quebrar a regra de definição única do C++ (ODR). O registro fica global, e a regra passa a ser "nomes de tipo únicos entre os jogos": dois tipos com o mesmo nome dão erro na partida, com o nome. Os prefabs de um jogo continuam na pasta dele.
2. **O `games/ikigai/` tem um C++ mínimo do autor.** Ele traz uma subclasse de `IGame` com poucas linhas (carrega a sala e cria o personagem pelo prefab). O personagem que anda e pula usa um componente novo da engine, `PlatformerMover`, sem nenhum nome de jogo dentro. As ações que ele lê (`moveLeft`, `jump`...) são propriedades do prefab, e as velocidades e os tempos são `MUSH2D_EXPORT`. Assim a engine continua sem nomes de jogo, e o autor começa com dados mais uma classe pequena que ele entende.
3. **"↓ interage e desce de plataforma".** No `iki_demo`, o comportamento fica igual (as duas coisas continuam no ↓, para a sensação não mudar). A E0 resolve a causa: interagir e descer viram duas ações separadas, que o projeto pode ligar à mesma tecla ou a teclas diferentes. O `games/ikigai/` já nasce com elas separadas.
4. **O buffer da engine substitui os buffers do Iki.** O `jumpBuffer` (0,12 s) e o `attackBuffered` passam a ser o buffer das ações `jump` e `attack`, com os mesmos tempos e ajustáveis no F1. O consumo segue a regra atual: um toque durante o hit stop é guardado (`keepEdges`) e consumido no primeiro passo que roda. Testes medem que os tempos ficaram iguais.
5. **As entradas da engine continuam da engine.** Câmera livre, zoom, arrastar a câmera, seguir, F1, mouse e roda são ferramentas de desenvolvimento, não ações do jogo. Elas ficam fixas na engine, fora do `project.json`. As ações do jogo (`jump`, `attack`, `shoot`, `guard`, `respawn`, `hit`...) vão para o projeto.

## Escolhas deixadas ao construtor

- O formato exato do `project.json`, desde que cubra os objetivos e cada número de jogo possa ser ajustado no F1.
- Como o elemento é guardado em memória (índice ou nome), desde que os prefabs e o inspetor continuem mostrando nomes.
- Como a forma da célula (sólido, rampas, atravessável) convive com o tipo de superfície: hoje os valores 1 a 8 da IntGrid são formas e o 9 é "sólido + escalável". A sala `showcase.ldtk` não pode mudar.
- O que fica em `assets/` comum (shaders, fontes, coisas da engine) e o que vai para cada jogo.
- Onde a última escolha da tela de abertura é guardada.

## Como cresce até o jogo final

- **E2 (natureza):** a tabela de reações (`Fire` + `Ice` → vapor, `Fire` + planta → queima) entra no `project.json` e usa a lista de elementos da E0. O mesmo resultado serve para o encontro no mundo e para o golpe combinado do jogo (o código do jogo pergunta à tabela).
- **Efeitos combinados com elemento próprio:** como a lista é aberta, o vapor pode ser um elemento com fraquezas próprias nos prefabs.
- **Barras:** o modelo de energia do Ikigai (quantas barras, como recarregam) é decidido pelo autor só em dados.
- **E3 (cenas):** as ações podem ser bloqueadas ou trocadas durante uma cena sem mexer nos objetos.
- **E8 (interface):** o remapeamento de teclas e gamepad grava por cima do mapa do `project.json`; os ícones dos botões leem o aparelho em uso, que a E0 já sabe.
- **Distribuição:** o executável do Ikigai é o mesmo programa com o jogo fixo e sem a tela de escolha.

## Testes (`mush2d_tests`)

- Leitura do `project.json`: elementos, barras, superfícies e ações; erros claros para nomes repetidos ou desconhecidos (avisos esperados capturados).
- Elementos: prefab com `"multipliers": {"Ice": 2}` funciona com o elemento vindo do projeto; nome desconhecido dá aviso.
- Barras: gastar, recarregar, `refill`, gasto de duas barras juntas (o que o `spendBoth` faz hoje).
- Superfícies: um valor da IntGrid com o nome de uma superfície escalável vira segmento escalável.
- Entrada: `pressed`/`down`/`released`; buffer expira no tempo certo e é consumido uma vez; teclado e gamepad na mesma ação; gatilho acima e abaixo do limite; zona morta; `keepEdges` com as ações.
- Regra de divisão: nenhum nome do Ikigai em `src/engine/`, nenhum include de `examples/` ou `games/`.
- Os testes atuais continuam passando, adaptados aos dados.

## Critérios de pronto

- O `iki_demo` funciona igual a antes, no teclado, e também no gamepad com as mesmas ações.
- `games/ikigai/` abre pela tela de escolha e pelo `--game ikigai`, com o personagem andando e pulando no teclado e no gamepad.
- `src/engine/` não cita conteúdo do Ikigai (verificado pelo teste).
- `mush2d_tests` passa com saída limpa; zero erros de validação do Vulkan; build no Linux e no Windows.
- Todo número novo de jogo (buffer, limite do gatilho, zona morta, barras) é ajustável no F1.
- Manual com as páginas novas e `mkdocs build --strict` passando; README, GUIA e ROADMAP atualizados.
