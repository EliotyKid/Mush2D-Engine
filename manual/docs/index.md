# Manual da Mush2D

A Mush2D é uma engine 2D em C++20 e Vulkan 1.3. O jogo que roda nela hoje é o `IkigaiGame` (`src/game/`), com salas feitas no LDtk, um mundo contínuo, luz com normal maps, névoa e bloom.

Este manual é para quem conhece o GameMaker e sabe pouco de C++. Use a busca (tecla `/` ou `S`) para achar qualquer termo.

## Como o manual está dividido

| Seção | Para quê |
|---|---|
| [Primeiros passos](primeiros-passos.md) | Compilar, rodar, controles, variáveis de ambiente. |
| [Conceitos](conceitos/gamemaker.md) | O que cada coisa do GameMaker vira aqui, o que acontece em cada frame, o C++ mínimo. |
| [Fazendo o jogo](fazendo-o-jogo/mundo-e-salas.md) | Receitas: salas, colisão, terreno, prefabs, objetos novos, comunicação, painel F1. |
| [Objetos do jogo](objetos.md) | Cada objeto pronto: o que faz, atributos, casos de uso. |
| [Referência de sistemas](sistemas/core.md) | Cada pasta de `src/engine/`: o que faz, tipos principais, casos de uso, arquivos. |
| [Desempenho](desempenho.md) | Números atuais e como medir. |

## Abrir este manual

```powershell
py -m pip install -r manual/requirements.txt
```

```powershell
py -m mkdocs serve -f manual/mkdocs.yml
```

Depois abra `http://127.0.0.1:8000`. Editar um `.md` em `manual/docs/` atualiza a página sozinho. `py -m mkdocs build -f manual/mkdocs.yml` gera o site estático em `manual/site/`.

## Limitações atuais

O que já é fácil: montar salas no LDtk, hot reload de salas e prefabs, o painel de look, o inspetor de cena ao vivo, criar e destruir com segurança, e eventos parecidos com os do GameMaker.

O que ainda é trabalhoso, com a solução proposta:

1. **Recompilar e reabrir o jogo para testar código.** Solução proposta: hot reload de código, ou scripts (Lua) para a lógica de gameplay.
2. **Um jogo novo exige escrever um `IkigaiGame` próprio.** Solução proposta: um "jogo padrão" da engine que já faz salas, mundo, regiões, player e câmera. Você só escreve os objetos.
3. **Ids de sprite e animação em código** (`GameSpriteId`, `GameAnimationId`, registrados em `GameAssets.cpp`) para o player e o código antigo. Prefabs já acham os atlas sozinhos pelo nome do arquivo (veja [graphics e assets](sistemas/graficos-e-assets.md)); falta o player e as cenas antigas usarem o mesmo caminho.
