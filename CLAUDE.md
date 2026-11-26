# Mush2D / Ikigai: instruções para o Claude

Leia primeiro: [docs/VISAO.md](docs/VISAO.md) (o jogo e os sistemas) e [docs/ROADMAP.md](docs/ROADMAP.md) (fases E0 a E8 e o estado atual).

## Papéis

- O **Claude constrói a engine** (`src/engine/`), o manual (`manual/`) e os exemplos (`examples/`).
- O **autor cria o jogo Ikigai** (`games/ikigai/`). O Claude ajuda e explica, mas **não escreve conteúdo do Ikigai** (personagens, salas, diálogos, regras do jogo), a não ser que o autor peça.
- O código atual do Iki, do combate de gelo, dos bonecos de treino e da sala `IceTest` é **exemplo** de uso da engine. Na E0 ele vai para `examples/iki_demo/`.
- O autor é iniciante em C++. Explique em português, sem jargão, e aponte arquivos e exemplos.

## Regra de divisão: engine diz como, jogo diz o quê

- A engine é feita para o Ikigai (metroidvania 2D, render no estilo Ori, mundo vivo e com memória), mas `src/engine/` **nunca cita conteúdo do Ikigai** (Iki, Gelo, Fogo, kaa, personagens).
- Elementos, barras de recurso, tipos de superfície e ações de entrada são definidos pelo projeto, em dados.
- `src/engine/**` nunca inclui código de jogo ou de exemplo.

## Processo

1. Cada fase começa com uma conversa de design. Fazer uma pergunta por vez, propor uma escolha com o motivo, e esperar a aprovação.
2. O design vai para `docs/superpowers/specs/`, com a seção "Como cresce até o jogo final" e uma revisão por um construtor antes de entregar.
3. O plano vai para `docs/superpowers/plans/`.
4. A execução é por subagentes: uma tarefa por vez, uma revisão de cada tarefa, correções com nova revisão, e uma revisão final do branch.
5. Cada fase entrega o sistema, a página do manual (`manual/docs/`, com `nav` em `manual/mkdocs.yml`, e `mkdocs build --strict` passando) e um exemplo pequeno.
6. **Sempre atualizar o manual e a documentação** (README, `docs/GUIA.md`, ROADMAP) quando algo novo entra.
7. Commit ou push só quando o autor pedir. Trabalhar num branch (`feat/...`), nunca direto no `main`. Merge só com a aprovação do autor.
8. Mensagens de commit terminam com uma linha em branco e `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Critérios de pronto

- Testes por sistema em `tests/tests.cpp` (`mush2d_tests`), com saída limpa: avisos esperados são capturados.
- Zero erros de validação do Vulkan; a saída de validação vai para stdout.
- Todo número de jogo e de juice é ajustável no inspetor F1 (`MUSH2D_EXPORT`, ou um controle no painel quando não é um objeto).

## Cautelas

- Não alterar o conteúdo do `assets/levels/showcase.ldtk` sem o autor pedir. Mudanças que forem pedidas são feitas por scripts idempotentes em `tools/`, só com acréscimos.
- O player não emite luz.
- Não usar `git stash`: o OneDrive trava os arquivos no Windows.
- Não fazer push sem pedido.

## Build

Pré-requisitos e detalhes: [README.md](README.md).

- **Arch Linux:** `cmake --preset debug && cmake --build build/debug && ctest --test-dir build/debug`
- **Windows** (PowerShell, MSVC):
  1. carregar o ambiente do Visual Studio (`vcvars64`);
  2. depois `cmake --preset debug; cmake --build build/debug; ctest --test-dir build/debug`.

Para conferir o jogo rodando, use uma captura com a variável `MUSH2D_SCREENSHOT` e um gancho temporário, removido antes do commit.

## Estado atual

- **Feito:** base técnica, D1 (prefabs e inspetor), D2 (movimento, estados, efeitos), D3 (combate, peso, áudio). Tudo está no `main`.
- **Próximo:** **E0**. Separar engine e jogo, e mover os vazamentos para dados:
  - o `Element` fixo;
  - `maxIce`/`maxFire` no `Resources`;
  - `Surface::IceClimb`;
  - os botões fixos do `InputState`.

  Criar `examples/iki_demo/` e o modelo `games/ikigai/`, com um personagem que anda e pula. Começar pela conversa de design.
- **Pendente no Arch:**
  - build e testes no Linux (o áudio nunca foi compilado no Linux);
  - redimensionar a janela sem erros de validação;
  - medir a RTX 2060 Super.
