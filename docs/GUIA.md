# Guia da Mush2D

O guia agora faz parte do manual em `manual/`. Para abrir: `py -m mkdocs serve -f manual/mkdocs.yml` e depois `http://127.0.0.1:8000`. As páginas estão em `manual/docs/`.

Os jogos são pastas (`examples/iki_demo/`, `games/ikigai/`), cada uma com `project.json`, `assets/` e `src/<id>/`. As teclas e os botões ficam no `project.json`, em `"actions"`. Veja `manual/docs/fazendo-o-jogo/criando-o-seu-jogo.md` e `projeto-em-dados.md`.

## O Iki e a máquina de estados (para quem está começando)

O Iki é o personagem jogável (`examples/iki_demo/src/iki_demo/objects/IkiObject.hpp/.cpp`). Ele nasce do arquivo `assets/prefabs/iki.prefab.json`, que guarda os números dele (velocidade, pulo, vida) e o visual (animações, poeira, piscar).

Uma **máquina de estados** é um jeito de organizar o comportamento: em cada momento o Iki está em um só estado (`idle` parado, `run` correndo, `jump` subindo, `fall` caindo, `hurt` machucado, `dead` morto, `wall` agarrado numa parede de gelo, `ceiling` pendurado num teto de gelo). Cada estado é uma classe pequena em `examples/iki_demo/src/iki_demo/iki/states/`. A cada quadro, o jogo chama o `update` do estado atual. O `update` move o Iki e devolve o nome do próximo estado, ou `nullptr` para continuar no mesmo. No GameMaker, seria um `switch (state)` no Step Event, com um script por estado.

Por que assim: para acrescentar uma habilidade, cria-se um estado novo sem mexer nos outros. O nome do estado também escolhe a animação no prefab e pode disparar efeitos (`"state:move:wall"` amassa o Iki quando ele agarra a parede).

**Combate de gelo:** o Iki tem uma segunda máquina de estados, `"action"`, que roda junto com a `"move"`. Assim ele pode correr e golpear ao mesmo tempo. `X` dá um corte; aperte `X` de novo logo depois e vem o segundo, e depois o terceiro, mais forte (estilo Ori). `C` lança um estilhaço de gelo que gasta 15 de gelo. Segurar `V` no chão ergue o escudo: ele bloqueia o que vem de frente gastando gelo, e se o golpe chega logo depois de você apertar `V`, é um **aparo**: de graça, o atacante fica tonto e um projétil volta para quem atirou. Cada acerto "pesa": o jogo congela por um instante (hit stop), a câmera treme, sai som e cacos. Esses números ficam no prefab e nos arquivos de efeito, e aparecem no F1. Detalhes em `manual/docs/fazendo-o-jogo/iki.md`, seção "Combate do Iki". Para testar, vá à sala `IceTest`: lá há bonecos de treino. O rosa é fraco a gelo (a sequência tira 2, 2 e 4), o ciano é resistente (1, 1 e 1) e o magenta, perto da porta, atira devagar em você. Com 0 de vida um boneco quebra e volta 3 s depois. Eles são só prefabs (`training_dummy*.prefab.json`) com os componentes de combate, e estão em `manual/docs/objetos.md`.

**Estacas de gelo:** depois de pegar o item `ability.ice.stakes` (na sala `IceTest`, à esquerda da vitrine), o Iki crava estacas no gelo. No ar, segurando `←` ou `→` contra uma parede de gelo, ele se agarra (`wall`): `↑`/`↓` sobe e desce, `Z` salta para longe. Segurando `↑` embaixo de um teto de gelo, ele se pendura (`ceiling`): `←`/`→` anda, `↓` ou `Z` solta. Sem o item, o gelo é uma parede comum.

O passo a passo, com um exemplo de estado novo, está no manual: `manual/docs/fazendo-o-jogo/iki.md`.
