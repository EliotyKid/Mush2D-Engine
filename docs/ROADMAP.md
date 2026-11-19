# Mush2D — Roadmap

Engine 2D pessoal, em C++20 e Vulkan 1.3, para jogos próprios. O primeiro jogo é um metroidvania épico, com direção de arte inspirada em Ori. A arte começa em pixel art e pode virar HD. O renderer suporta as duas.

Decisões de base (2026-11-18):

- sprites em pixel art com snap, e luz, névoa e bloom em resolução nativa (opção para pixelar a luz);
- LDtk para níveis, mais ImGui no jogo para ajustes visuais;
- Vulkan 1.3, com o Steam Deck como referência de desempenho;
- gameplay em C++ (Lua só se surgir necessidade);
- atlas com mapas opcionais `_n` (normal), `_e` (emissão) e `_m` (máscara);
- dependências via CMake FetchContent, com VMA e vk-bootstrap.

Marque `[x]` ao concluir. Cada fase vira um documento de design próprio em `docs/superpowers/specs/` quando for começar.

## Fase 0 — Sala vitrine (em andamento)

Design: [specs/2026-11-18-sala-vitrine-design.md](superpowers/specs/2026-11-18-sala-vitrine-design.md)

- [ ] Fundação: `.gitignore`, FetchContent, build igual em Arch e Windows, shaders compilados pelo CMake
- [x] GPU: vk-bootstrap, VMA, 2 frames em voo, resize, validation layers
- [x] Separação engine × jogo
- [x] SpriteBatch bindless e materiais `_n/_e/_m`
- [x] Passes: G-buffer, luz, névoa, forward aditivo, bloom, composição
- [x] Carregador LDtk (IntGrid, tiles, parallax, entidades)
- [x] Painel ImGui e `.look.json`
- [ ] Sala montada, 60 fps, critérios de pronto atendidos (Windows/Intel UHD ok; falta o PC Arch)

Pendente no PC Arch (RTX 2060 Super): build debug sem erros de validação, e soma dos tempos de GPU < 3 ms em 1080p (`MUSH2D_PERF_LOG=10`). Ao medir, marque Fundação e a sala montada.

## Fase 1 — Mundo

- [x] Várias salas do LDtk, com transições entre elas e carregamento das salas vizinhas — [design](superpowers/specs/2026-11-19-mundo-continuo-design.md)
- [ ] Câmera dinâmica: zonas (limites, foco, zoom por área), look-ahead, suavização e shake
- [ ] Múltiplas câmeras com troca e mistura (blend) entre elas
- [ ] Checkpoints e save (posição, habilidades, mapa explorado)
- [ ] Mapa-múndi in-game
- [x] Modelo de entidades orientado a objetos revisto (hierarquia, componentes reutilizáveis, ciclo de vida, pai/filho) — [design](superpowers/specs/2026-11-19-modelo-entidades-design.md)
- [x] Pixel art suave: zoom livre e câmera em subpixel sem pixels quebrados (modo padrão)

## Fase 1.5 — Cinemática

- [ ] Sistema de cutscenes: linha do tempo com trilhas (câmera, entidades, animação, luz, áudio, diálogo, espera de input)
- [ ] Câmeras cinematográficas: trilhos/curvas, enquadramento, letterbox
- [ ] Cutscenes autoradas em arquivo de dados (decidir: JSON/LDtk, ou Lua — ver Fase 9)

## Fase 2 — Partículas e efeitos (o "brilho Ori")

- [ ] Sistema de partículas na GPU (compute), com milhares de partículas
- [ ] Emissores configurados no LDtk e ajustáveis no ImGui
- [ ] Vagalumes, pétalas, poeira, faíscas e trilhas do player
- [ ] Shaders de vento e balanço para vegetação
- [ ] Distorção (calor, água) e água 2D com reflexo

## Fase 3 — Luz avançada

- [ ] Sombras 2D (oclusores e campo de distância)
- [ ] Luz volumétrica e god rays entre camadas
- [ ] Luzes em tiles, para centenas de luzes
- [ ] Luzes de área e direcionais (sol/lua por região)
- [ ] Color grading por região, com LUT e transição suave
- [ ] Sprites translúcidos iluminados (bordas suaves para arte HD)

## Fase 4 — Movimento e combate de plataforma

- [ ] Controlador de personagem (coyote time, buffer de pulo, wall jump, dash, double jump)
- [ ] Habilidades que liberam áreas (gating de metroidvania)
- [ ] Combate: hitboxes/hurtboxes, knockback, i-frames, hit stop
- [ ] Inimigos com máquina de estados reutilizável

## Fase 5 — Animação

- [ ] Tags do Aseprite como animações, com eventos por frame (passos, golpes)
- [ ] Squash & stretch e animação procedural simples
- [ ] Animação esquelética (Spine ou similar), se a arte virar HD

## Fase 6 — Áudio

- [ ] miniaudio: efeitos, música e mixagem por canais
- [ ] Áudio 3D: som posicional com distância, pan estéreo e profundidade das camadas (parallax) influenciando volume/filtro
- [ ] Ambiente por região (reverb, abafamento atrás de paredes)
- [ ] Música adaptativa (camadas por intensidade)

## Fase 7 — Interface e entrada

- [ ] Gamepad e remapeamento de controles
- [ ] Texto com fontes MSDF, HUD e menus
- [ ] Localização PT/EN

## Fase 8 — Outros estilos 2D

- [ ] Módulo top-down (movimento 8 direções, ordenação por Y, colisão)
- [ ] Separar sistemas específicos de gênero dos genéricos

## Fase 9 — Ferramentas e produtividade

- [ ] Hot reload de texturas, shaders, LDtk e `.look.json`
- [ ] Console de debug e visualização de colliders/luzes
- [ ] Profiler de CPU e GPU no ImGui
- [ ] Lua (sol2) para cutscenes, eventos e chefes, só se C++ e LDtk virarem gargalo

## Fase 10 — Distribuição

- [ ] Build de release otimizado para Linux e Windows
- [ ] Teste e ajuste no Steam Deck
- [ ] Empacotamento de assets
