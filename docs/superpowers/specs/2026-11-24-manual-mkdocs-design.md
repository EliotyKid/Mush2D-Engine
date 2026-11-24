# Manual da engine em MkDocs — design

## Por quê

Uso pessoal: ver a documentação da Mush2D de forma mais fácil do que em `.md` soltos, pesquisar rápido, e entender o que cada coisa faz com casos de uso.

## Objetivos

- Site local com menu lateral, busca em português e índice por página.
- Cada sistema e cada objeto do jogo explicado: o que faz, principais tipos/atributos, casos de uso, arquivos.
- Conteúdo continua em Markdown, editável e versionado no git.

## Não objetivos

- Hospedar online, traduzir, referência de API gerada (Doxygen).

## Anti-objetivos

- Duas cópias do mesmo texto (README/GUIA e manual) que divergem com o tempo.
- Descrever comportamento que o código não tem: tudo vem da leitura do código atual.

## Abordagens

- **A. MkDocs + Material (escolhida).** Busca offline boa, navegação, admonitions, conteúdo em `.md`. Custo: `pip install`.
- B. Docsify: sem build, mas depende de CDN e servidor local; busca e visual mais fracos.
- C. HTML único à mão: sem dependências, mas o conteúdo fica preso no HTML e desatualiza.

## Design

Pasta nova `manual/` na raiz:

```
manual/
  mkdocs.yml          tema material, language: pt, busca pt, modo escuro, copiar código
  requirements.txt    mkdocs-material (versão fixa)
  docs/               páginas .md
  site/               saída do `mkdocs build` (no .gitignore)
```

Uso: `pip install -r manual/requirements.txt`, depois `mkdocs serve -f manual/mkdocs.yml` e abrir `http://127.0.0.1:8000`.

Páginas (menu):

1. **Início** — o que é a engine, como navegar o manual.
2. **Primeiros passos** — pré-requisitos, build, executar, testes, controles, variáveis de ambiente.
3. **Conceitos** — tabela GameMaker → Mush2D, ciclo do jogo (o que acontece por frame), C++ mínimo.
4. **Fazendo o jogo** — mundo contínuo e salas LDtk, colisão e rampas, terreno, prefabs, criar um objeto novo, comunicação entre objetos, inspetor F1 e salvar prefab.
5. **Objetos do jogo** — uma seção por objeto (`Player`, `Slime`, `Door`, `Lever`, `Item`, `Checkpoint`, `Critter`, `Prop`, `Crystal`, `Teleport`, `Wall`): atributos, comportamento, casos de uso.
6. **Referência de sistemas** — uma página por pasta de `src/engine/` (core, platform, world, graphics/assets, render + shaders, gpu, tools, reflect): o que faz, tipos principais, casos de uso, arquivos.
7. **Desempenho** — tabela atual e como medir.

Formato fixo das páginas de referência: "O que faz", "Tipos principais", "Casos de uso" (exemplos curtos de código ou passos), "Arquivos".

## Decisões minhas (corrija se quiser)

- Idioma: português.
- `docs/GUIA.md` é dividido nas páginas do manual e substituído por um link para ele. O README fica só com pré-requisitos, build e um link para o manual; controles, mundo, prefabs, variáveis e desempenho passam para o manual.
- Specs, plans e ROADMAP ficam fora do manual.
- `manual/` na raiz, não dentro de `docs/`, para não misturar com as specs.

## Para quem constrói

- Ordem e divisão exata das seções dentro de cada página.
- Quais casos de uso mostrar por sistema, desde que reais no código atual.
