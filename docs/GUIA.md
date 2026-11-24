# Guia da Mush2D

O guia agora faz parte do manual em `manual/`. Para abrir: `py -m mkdocs serve -f manual/mkdocs.yml` e depois `http://127.0.0.1:8000`. As páginas estão em `manual/docs/`.

## O Iki e a máquina de estados (para quem está começando)

O Iki é o personagem jogável (`src/game/objects/IkiObject.hpp/.cpp`). Ele nasce do arquivo `assets/prefabs/iki.prefab.json`, que guarda os números dele (velocidade, pulo, vida) e o visual (animações, poeira, piscar).

Uma **máquina de estados** é um jeito de organizar o comportamento: em cada momento o Iki está em um só estado (`idle` parado, `run` correndo, `jump` subindo, `fall` caindo, `hurt` machucado, `dead` morto). Cada estado é uma classe pequena em `src/game/iki/states/`. A cada quadro, o jogo chama o `update` do estado atual. O `update` move o Iki e devolve o nome do próximo estado, ou `nullptr` para continuar no mesmo. No GameMaker, seria um `switch (state)` no Step Event, com um script por estado.

Por que assim: para acrescentar uma habilidade, cria-se um estado novo sem mexer nos outros. O nome do estado também escolhe a animação no prefab e pode disparar efeitos (`"state:move:hurt"` faz o Iki piscar).

O passo a passo, com um exemplo de estado novo, está no manual: `manual/docs/fazendo-o-jogo/iki.md`.
