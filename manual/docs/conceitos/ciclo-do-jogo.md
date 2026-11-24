# Ciclo do jogo

## O que acontece quando o jogo abre

```
main.cpp ──► cria IkigaiGame (o "jogo") e Engine (a "máquina")
             engine.run(game)
                 │
                 ├─ abre a janela, liga a GPU (Vulkan), o renderer e o input
                 ├─ game.init(...)          ← o jogo carrega assets e a primeira sala
                 └─ loop até fechar a janela, ~60 vezes por segundo:
                       1. lê teclado e mouse
                       2. game.update(dt)   ← tudo do jogo acontece aqui
                       3. a câmera segue o player
                       4. o renderer desenha o frame (luz, névoa, bloom...)
```

## Um frame dentro de `game.update`

`game.update` é o "Step" de todo o jogo. O `IkigaiGame` faz, nesta ordem:

1. teclas globais (TAB troca de cena, F5 recarrega) e hot reload;
2. passa o input para o player;
3. **`scene.step(dt)`**: chama o `onStep` de **todos** os objetos (como o Step Event de todas as instâncias);
4. verifica os triggers, chamando `onTriggerEnter`/`onTriggerExit`;
5. **`scene.lateStep(dt)`**: o End Step;
6. o mundo carrega e descarrega salas conforme o player anda, checa respawns e cuida da névoa e cor por região e do fade de morte;
7. envia sprites e luzes para o renderer.

`dt` é o tempo em segundos desde o frame anterior. Multiplique velocidades por `dt` para que o movimento não dependa do fps.

## Criar e destruir durante o frame

Criar e destruir objetos é seguro a qualquer momento, mesmo dentro de um `onStep`. A mudança vale no fim da etapa (o `flush` da cena), como no GameMaker. No mesmo `flush`, a cena entrega os eventos do `EventBus` emitidos até ali.

## Ordem de aplicação dos valores de um objeto

Quando um objeto nasce de um prefab, os valores chegam nesta ordem (o último vence):

1. construtor da classe;
2. `onCreate` (os padrões em C++);
3. os valores do prefab (primeiro os do `base`, depois os dele);
4. os campos da entidade no LDtk;
5. `onPrefabApplied` (o objeto reage aos valores finais);
6. edições ao vivo no inspetor.

## Casos de uso

- **Mover algo a velocidade constante:** em `onStep`, `transform.position.x += speed * dt;`.
- **Reagir a um valor vindo do LDtk:** leia o atributo em `onPrefabApplied`, não em `onCreate`. No `onCreate` o campo da entidade ainda não foi aplicado.
- **Ler a posição de outro objeto depois que todos se moveram:** faça em `onLateStep`.
