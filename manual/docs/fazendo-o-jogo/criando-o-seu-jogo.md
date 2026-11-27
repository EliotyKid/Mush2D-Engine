# Criando o seu jogo

Na Mush2D, cada jogo é uma **pasta**. A engine (`src/engine/`) é uma só; os jogos ficam do lado dela. Hoje há dois:

| Pasta | O que é |
|---|---|
| `examples/iki_demo/` | O exemplo: o Iki, o combate e a sala vitrine. Serve para aprender. |
| `games/ikigai/` | O modelo inicial do Ikigai: uma sala vazia e um personagem que anda e pula. |

## O que tem dentro de uma pasta de jogo

```
games/ikigai/
  project.json          os dados do projeto (veja "Projeto em dados")
  assets/               salas (LDtk), prefabs, sprites, sons
  src/ikigai/           o código do jogo
    IkigaiGame.hpp/.cpp a classe do jogo
    objects/            um .hpp por objeto novo
```

Os caminhos `assets/...` no código são relativos à pasta do jogo. O build copia `project.json` e `assets/` para perto do executável.

## Ligando um jogo ao build

No `CMakeLists.txt` da raiz, cada jogo é uma linha:

```cmake
mush2d_add_game(ikigai games/ikigai IkigaiGame ikigai/IkigaiGame.hpp)
```

Os quatro valores são: o id do jogo, a pasta, o nome da classe do jogo e o cabeçalho dela (relativo a `src/` do jogo). A função está em `cmake/Games.cmake`. Ela compila o jogo como uma biblioteca, inclui sozinha todo `src/<id>/objects/*.hpp` e copia `project.json` e `assets/`.

!!! warning "Nomes de tipo únicos"
    Todos os jogos compilados ficam dentro do mesmo programa. Por isso o nome de um objeto ou componente (`MUSH2D_OBJECT`) não pode se repetir entre dois jogos. Se repetir, o programa para ao abrir, com uma mensagem dizendo qual nome está duplicado. Dê aos seus tipos nomes próprios do jogo.

## Abrindo um jogo

O programa é o `Mush2D` (`build/debug/Mush2D`).

- Sem argumento, mostra a **tela de escolha**: um botão por jogo, o último jogo aberto marcado, e `Enter` abre o marcado. A escolha fica em `mush2d_last_game.json`, ao lado do executável.
- Com `--game <id>` a tela é pulada: `build/debug/Mush2D --game ikigai`.
- Com um id que não existe, ele lista os ids válidos e mostra a tela de escolha.

## Criando um jogo novo

1. Copie `games/ikigai/` para uma pasta nova e troque o `id` e o `name` no `project.json`.
2. Renomeie `src/ikigai/` para `src/<id>/` e a classe do jogo.
3. Acrescente a linha `mush2d_add_game(...)` no `CMakeLists.txt`.
4. Rode `cmake --preset debug` de novo e compile.

## A sala do modelo Ikigai

!!! note "O chão tem duas partes"
    Na sala do modelo (`games/ikigai/assets/ikigai.ldtk`), o chão que você **vê** é feito de entidades `Block`. O chão em que o personagem **pisa** vem da camada `Collision` (IntGrid). Os dois são independentes: se mexer em um, mexa no outro também, senão o desenho e a colisão não combinam.

## Engine diz como, jogo diz o quê

Esta regra mantém a engine reaproveitável:

- `src/engine/` **nunca cita conteúdo de jogo** (personagens, elementos, salas). Também nunca inclui código de `examples/` ou `games/`.
- Tudo que é do jogo vem **em dados**: os elementos, as barras de recurso, os tipos de superfície e as ações de entrada ficam no `project.json`.
- O código do jogo pode usar a engine, nunca o contrário.

O teste `engine_has_no_game_content` (em `tests/tests.cpp`) confere isso: ele falha se algum arquivo de `src/engine/` citar um nome de jogo.
