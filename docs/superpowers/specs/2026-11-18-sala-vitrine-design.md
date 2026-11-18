# Mush2D — Sala Vitrine (primeiro marco)

Data: 2026-11-18
Status: rascunho para aprovação

## Intenção

Mush2D é uma engine 2D pessoal, em C++20 e Vulkan, para construir os jogos do próprio autor. Não será lançada ao público. O primeiro jogo é um metroidvania épico, com paisagens grandiosas e direção de arte inspirada em Ori and the Will of the Wisps: luz dinâmica, brilho, névoa entre camadas e profundidade. A engine deve servir também a outros jogos 2D (plataforma, top-down etc.), reaproveitando o máximo de sistemas.

A arte começa em pixel art, mas pode mudar para arte HD pintada. O renderer não pode depender de pixel art: pixel art é um modo.

A **sala vitrine** é o primeiro marco. Ela prova a direção visual e a nova arquitetura antes de crescer o resto. O que vem depois está em [docs/ROADMAP.md](../../ROADMAP.md).

## Objetivos da vitrine

1. Uma sala do metroidvania montada no LDtk e carregada pela engine.
2. De 4 a 6 camadas de parallax, com profundidade.
3. Player com normal map, andando e pulando, colidindo com o cenário.
4. Três tipos de luz: ambiente, pontual e a luz própria do player.
5. HDR, bloom e névoa entre as camadas.
6. Painel ImGui dentro do jogo para ajustar ao vivo luzes, névoa, cor, bloom e parallax, salvando em arquivo.
7. 60 fps estáveis no notebook com Intel UHD Graphics, que é mais fraco que o Steam Deck.
8. Compila e roda no Arch Linux e no Windows com o mesmo CMake.

## Não-objetivos (ficam para o roadmap)

Partículas avançadas, áudio, várias salas e transições, save, modo top-down, sombras 2D, luz volumétrica, scripting (Lua), gamepad, UI/HUD e menus.

## Anti-objetivos (seria fracasso mesmo funcionando)

- Um renderer que só funciona com pixel art.
- Código de jogo dentro da engine (como hoje `EngineApp.hpp` inclui `PlayerObject`).
- Ajustar luz e cor exigir recompilar.
- Build que só funciona numa das duas máquinas.
- Desempenho que depende de poucos sprites (uma draw call por sprite).

## Restrições

- C++20, CMake ≥ 3.24, Vulkan 1.3 (dynamic rendering, synchronization2, descriptor indexing).
- Dependências baixadas pelo CMake via FetchContent, em versões fixas: VMA, vk-bootstrap, GLFW, glm, nlohmann_json, Dear ImGui. `stb_image.h` continua em `external/`. A única instalação manual é o Vulkan SDK (fornece `glslc` e as validation layers) e o compilador.
- Gameplay somente em C++. Comportamentos simples são configurados por campos de entidade do LDtk.

## Abordagens consideradas

1. **Refatorar tudo à mão**, com as dependências vindas do sistema. Ensina cada detalhe, mas gera muito boilerplate de memória e setup, e no Windows obriga a caçar bibliotecas.
2. **Refatorar com bibliotecas padrão (VMA, vk-bootstrap) via FetchContent.** ← **Escolhida.** Corta umas 600 linhas de setup, o build fica idêntico nos dois sistemas e o tempo vai para iluminação.
3. **Reescrever do zero.** Mais limpo, mas descarta código que funciona (atlas, colisão, objetos, cenas) e atrasa a vitrine.

Na abordagem 2, o código de jogo existente é preservado e só muda de lugar. O renderer é reescrito por dentro, porque o atual (um sprite por draw call, render direto na swapchain) não serve de base para a iluminação.

## Decisões já tomadas com o autor

| Tema | Decisão |
|---|---|
| Pixel art × luz | Sprites em pixel art com snap. Luz, névoa, bloom e partículas na resolução nativa. Opção para pixelar a luz. |
| Montagem de níveis | LDtk para salas, tiles e entidades, mais ImGui no jogo para ajustes visuais salvos num arquivo sobreposto. |
| API gráfica | Vulkan 1.3, Steam Deck como referência de desempenho. |
| Gameplay | Só C++. Lua talvez no futuro, se surgir necessidade. |
| Materiais | Cada atlas tem arquivos opcionais `_n` (normal), `_e` (emissão) e `_m` (máscara), com valores neutros quando ausentes. |
| Primeiro marco | Sala vitrine (este documento) e roadmap registrado em arquivo. |

## Arquitetura

### Separação engine × jogo

```
src/engine/   núcleo reutilizável: nunca inclui nada de src/game/
  core/       loop, tempo, logs, caminhos de arquivos
  platform/   janela GLFW, input (já existe: GlfwInput)
  gpu/        device (vk-bootstrap), VMA, swapchain, frames em voo, upload
  render/     SpriteBatch, passes (gbuffer, luz, forward, bloom, composição)
  assets/     atlas + materiais (já existe: AtlasLoader, TextureAtlas)
  world/      Scene2D, GameObject, colisão (já existe), carregador LDtk
  tools/      painel ImGui, arquivo de overrides visuais
src/game/     o metroidvania: objetos, cenas, fábrica de entidades LDtk
src/main.cpp  cria a engine e o jogo
```

O jogo implementa uma interface pequena, que a engine chama: `init`, `update(dt)` e `onImGui()`. Hoje `EngineApp` mistura essas funções e passa a ser só engine.

### GPU

- vk-bootstrap cria instance, device e swapchain. As validation layers ficam ligadas em Debug.
- VMA faz todas as alocações de buffers e imagens.
- 2 frames em voo, cada um com seu comando, fence, semáforos e buffers dinâmicos.
- A swapchain é recriada no resize e quando chega `VK_ERROR_OUT_OF_DATE_KHR` ou `VK_SUBOPTIMAL_KHR`. Os render targets da cena são recriados junto.
- Tudo com dynamic rendering e `vkCmdPipelineBarrier2`. Não há `VkRenderPass` nem `VkFramebuffer`.

### Texturas bindless e materiais

- Um array global de `sampler2D` (descriptor indexing, `PARTIALLY_BOUND`, `UPDATE_AFTER_BIND`) com capacidade fixa de 1024. Cada textura carregada recebe um índice.
- Material = 4 índices: albedo, normal, emissão e máscara. Se o atlas `characters.png` tiver ao lado `characters_n.png`, `characters_e.png` ou `characters_m.png`, esses arquivos são carregados. Senão, o material usa as texturas neutras 1×1:
  - normal (128,128,255);
  - emissão preta;
  - máscara (R=0 rim, G=255 oclusão nenhuma).
- Os mapas `_n/_e/_m` têm o mesmo tamanho e layout do atlas. As coordenadas UV são as mesmas.
- Normais de sprites espelhados (scale.x negativo) têm o canal X invertido no shader.

### SpriteBatch

- Cada sprite visível vira uma instância num buffer de armazenamento por frame: posição, tamanho, rotação, uvRect, cor, índice de material, profundidade da camada e flags.
- As instâncias são ordenadas por (camada, ordem). As opacas são desenhadas numa única draw call instanciada. Não há troca de descriptor por sprite.
- **Pixel art:** sampler `NEAREST`. A posição de cada sprite é arredondada para a grade de 1/`pixelsPerUnit` em coordenadas de mundo. A câmera também é arredondada. É o que `PixelArtSettings` já faz hoje, levado para o batch. Para arte HD, desliga-se o snap e usa-se `LINEAR`, sem mudar mais nada.
- **Escala da tela em pixel art:** o zoom fica restrito a uma escala inteira (1 pixel de arte = N×N pixels de tela). N é o maior inteiro que ainda mostra pelo menos 400 pixels de arte na horizontal. Por exemplo, N = 3 em 1280×800 (426×266 de arte) e N = 4 em 1920×1080. Não há letterbox: a sobra mostra mais mundo. No modo HD o zoom é livre. *(decisão do Claude — revisar)*
- **Bordas suaves (arte HD):** nesta vitrine, a geometria iluminada usa recorte por alpha, suficiente para pixel art. Bordas translúcidas iluminadas (passada forward com luz) ficam registradas no roadmap, Fase 3, e entram quando a arte HD for adotada. *(decisão do Claude — revisar)*

### Pipeline de render (por frame)

1. **G-buffer** (resolução nativa): sprites opacos e com alpha recortado (`discard` se alpha < 0.5). Saídas:
   - albedo RGBA8 (sRGB);
   - normal RGBA8 (RG = normal, B = rim da máscara, A = AO da máscara);
   - emissão RGBA16F;
   - profundidade da camada R16F.
2. **Luz** (tela cheia, HDR RGBA16F):
   - cor = albedo × (ambiente × AO + Σ luzes pontuais) + emissão;
   - cada luz pontual é Lambert com o normal map, usando como 3ª dimensão a altura da luz e a diferença entre a profundidade da luz e a do pixel, de modo que luzes da camada de jogo iluminam pouco o fundo distante;
   - rim: soma o termo de luz que vem por trás, ponderado pelo canal R da máscara;
   - **névoa:** mistura com a cor da névoa por `1 - exp(-densidade × profundidade)`, com gradiente vertical opcional. É isso que cria a névoa entre as camadas de parallax;
   - as luzes ficam num buffer de armazenamento. A vitrine assume até 64 luzes, todas avaliadas por pixel (tiling fica no roadmap);
   - **pixelar a luz** (opcional): esta passada roda numa resolução reduzida (`nativa / fator`) e é amostrada com `NEAREST` na composição.
3. **Forward aditivo** (sobre o HDR): sprites translúcidos e brilhos, como halos e partículas simples. São desenhados com o mesmo SpriteBatch, com blend aditivo e sem luz.
4. **Bloom:** threshold suave, cadeia de downsample/upsample (5 níveis) em RGBA16F, começando em meia resolução.

**Escala de qualidade:** a passada de luz pode rodar em resolução inteira ou meia, configurável no painel e no arquivo de configuração. Na Intel UHD o padrão é meia resolução. O G-buffer e a composição ficam sempre em resolução nativa.
5. **Composição** (para a swapchain): HDR + bloom × intensidade, exposição, tonemap (AgX ou ACES) e color grading simples (lift/gamma/gain, saturação e temperatura).
6. **ImGui** desenhado por cima, na swapchain.

### Parallax e camadas

- Cada camada tem `parallax` (x, y) e `depth`, em unidades de mundo (as mesmas da altura das luzes). 0 é a camada de jogo, positivo fica mais ao fundo e negativo é primeiro plano, desenhado na frente do player com parallax > 1. *(decisão do Claude — revisar)*
- A névoa usa `max(depth, 0)`, então o primeiro plano não recebe névoa. A atenuação das luzes usa `|depth_luz - depth_pixel|`.
- O offset da câmera é aplicado por camada no vertex shader, com `parallax × posição da câmera`.
- A profundidade da camada vai para o G-buffer e é usada na névoa e na atenuação das luzes.

### Shaders

GLSL em `shaders/`, compilados para SPIR-V pelo CMake com o `glslc` do Vulkan SDK. Os `.spv` deixam de ser versionados.

### Caminhos de arquivos

Assets e shaders são resolvidos a partir do diretório do executável. O CMake copia `assets/` e os `.spv` para o diretório de build, então o jogo roda a partir de qualquer pasta.

## LDtk

- Formato: projeto `.ldtk` único (sem "separate level files"), em `assets/levels/`. A vitrine usa um nível.
- **Camada IntGrid `Collision`:** valor 1 = sólido. Células vizinhas são fundidas em retângulos, que viram colliders estáticos (`ColliderData`).
- **Camadas de tiles:** cada tile vira uma instância no SpriteBatch, usando o tileset (um atlas com material). O parallax vem dos campos nativos do LDtk `parallaxFactorX/Y`. A profundidade vem de um campo de camada `depth`, ou, se ausente, da ordem da camada.
- **Entidades:** a engine lê identificador, posição, `iid` e campos. O jogo registra fábricas por identificador. A vitrine precisa de:
  - `PlayerStart`;
  - `Light` (cor, intensidade, raio, altura, profundidade, `flicker` opcional).
- A luz do player é criada em código, presa ao objeto do player.
- Unidade: 1 unidade de mundo = `pixelsPerUnit` pixels do LDtk (32, como hoje).

## Overrides visuais (ImGui)

- Arquivo `assets/levels/<nivel>.look.json`, carregado depois do LDtk. Ele guarda:
  - ambiente, névoa, bloom, exposição e grading;
  - ajustes de `parallax/depth` por camada;
  - edições de luzes do LDtk, identificadas pelo `iid`.
- Painel (F1 abre/fecha):
  - sliders e seletores de cor para tudo acima;
  - lista de luzes com edição de propriedades;
  - liga/desliga de pixel art e de pixelar a luz;
  - botão "Salvar";
  - tempos de GPU por passada (timestamp queries) e fps.
- **Onde salvar:** em Debug, o "Salvar" grava no `assets/levels/` do código-fonte. O caminho vem de uma definição de compilação `MUSH2D_SOURCE_DIR`, para que o ajuste vá para o git e sobreviva a rebuilds. Em Release, grava ao lado do executável. *(decisão do Claude — revisar)*
- O LDtk continua sendo a fonte do layout. O `.look.json` só sobrepõe valores visuais. Se o arquivo não existir, valem os valores do LDtk e os padrões.

## Gameplay da vitrine

- `PlayerObject` existente: andar, pular, gravidade e colisão AABB com os colliders do LDtk. O pulo usa altura variável, soltando o botão para cortar o pulo.
- A câmera segue o player com suavização e respeita os limites da sala.
- As cenas atuais em C++ (Sandbox e Test) e os objetos Slime, Checkpoint e Teleport continuam rodando. Como usam `SpriteData`, passam a ser desenhados pelo SpriteBatch novo com material neutro, sem porte específico. O `Renderer2D` antigo é removido. *(decisão do Claude — revisar)*

## Arte provisória

O autor vai trocar depois. Todo o conteúdo é feito de forma simples, para testar o sistema:

- **Tileset** de 32 px (`assets/tilesets/placeholder.png`) com chão, parede e borda, mais `_n.png` gerado a partir do relevo e `_m.png`.
- **Camadas de parallax:** 5 PNGs (céu, montanhas distantes, floresta média, árvores próximas e primeiro plano escuro), em silhuetas com gradientes, em `assets/parallax/`. O primeiro plano tem profundidade negativa.
- **Cristais emissivos** num atlas `props` com `_e.png`, para testar emissão e bloom.
- **Player:** os sprites atuais de `characters.png` ganham `characters_n.png` gerado.
- **Geração:** um script Python versionado (`tools/gen_placeholders.py`, usando só Pillow) gera as imagens e os normal maps (relevo a partir do alpha e do brilho). Ele roda uma vez e o resultado é commitado.
- **Sala:** `assets/levels/showcase.ldtk`, montada à mão no formato LDtk, com IntGrid de colisão, as camadas acima, `PlayerStart` e 4 a 6 `Light`. O arquivo pode ser aberto e editado no LDtk depois.

## Hardware de teste

- **Notebook Windows, Intel UHD Graphics:** alvo mínimo (mais fraco que o Steam Deck). Antes de começar, confirmar com `vulkaninfo` que o driver expõe Vulkan 1.3.
- **PC Arch, RTX 2060 Super:** máquina principal de desenvolvimento.

## Higiene do repositório

- `.gitignore` com `build/`, `*.spv` e saídas de IDE. A pasta `build/` sai do controle de versão.
- `README.md` com os passos de build para Arch e Windows.

## Testes

Um executável `mush2d_tests` com testes simples baseados em `assert`, sem framework, rodado pelo CTest:

- fusão do IntGrid em retângulos;
- leitura de um `.ldtk` mínimo (camadas, parallax, entidades);
- resolução de material: arquivos `_n/_e/_m` presentes e ausentes;
- leitura e escrita do `.look.json`.

O render é verificado visualmente, com zero erros nas validation layers.

## Critérios de pronto

1. `cmake --preset debug` e o build passam no Arch e no Windows, com zero erros de validação ao abrir, redimensionar e fechar.
2. A sala do LDtk aparece com 4 a 6 camadas de parallax, névoa entre elas e o player iluminado com o normal map.
3. Mudar luz, névoa ou grading no painel tem efeito imediato. Salvar e reabrir o jogo preserva os ajustes.
4. Trocar o modo pixel art para HD (snap e filtro) funciona sem recompilar.
5. 60 fps estáveis no notebook com Intel UHD Graphics em 1280×720. Essa GPU é mais fraca que o Steam Deck, então funciona como alvo mínimo real. No PC do Arch (RTX 2060 Super), a soma dos tempos de GPU fica abaixo de 3 ms em 1080p.
6. `src/engine/` não inclui nenhum arquivo de `src/game/`.

## Deixado para o construtor

- Organização interna das classes dentro de cada módulo.
- Escolha entre AgX e ACES, curvas exatas do bloom e formato do grading.
- Versões exatas das dependências, desde que fixas.
- Ordem interna das tarefas, respeitando: fundação (build, GPU, separação engine × jogo) antes do renderer novo, e o renderer novo antes de LDtk e ImGui.
