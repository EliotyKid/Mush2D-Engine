# Comunicação entre objetos

Há três jeitos de um objeto falar com outro.

## Direto: achar o objeto

```cpp
GameObject* p = findPlayer(scene); // engine/world/IPlayer.hpp
if (p) { /* usa p */ }
```

Funções de busca da cena:

| Função | Devolve |
|---|---|
| `scene.find(id)` | o `GameObject*` com esse id, ou `nullptr` |
| `scene.findObject(id)` | o mesmo, mas `nullptr` também para um objeto que vai ser destruído (o jeito do jogo) |
| `scene.findObjectAs<T>(id)` | o objeto com esse id, se for do tipo `T`, ou `nullptr` |
| `scene.findFirstObjectOfType<T>()` | o primeiro objeto do tipo `T`, ou `nullptr` |
| `scene.findAllObjectsOfType<T>()` | todos os objetos do tipo `T` |
| `scene.findObjectsByTag("tag")` | todos os objetos com a tag |

!!! tip "Guarde o id, não o ponteiro"
    Um objeto pode ser destruído entre um frame e outro. Guarde o `GameObjectId` e procure com `findObjectAs` a cada uso, como a alavanca faz com o player.

## Por tag

```cpp
for (GameObject* g : scene.findObjectsByTag("enemy")) { /* ... */ }
```

Dê tags no `onCreate` com `addTag("enemy")`.

!!! note "O player é achado pela tag `player`"
    Não pergunte pelo tipo (`PlayerObject`): o player pode ser de outro tipo (o Iki). Para saber se `other` é o player, use `other.hasTag("player")`. Para chamar um método dele, use a interface `IPlayer` (`respawnAt`, `downPressedThisFrame`, ...): `dynamic_cast<IPlayer*>(&other)`, ou `scene.findObjectAs<IPlayer>(id)`. Veja [World](../sistemas/world.md).

## Por evento

O `EventBus` da cena (`scene.events()`) manda mensagens tipadas. É o equivalente a um user event global.

```cpp
struct PlayerHit { int damage; };

// quem escuta guarda a assinatura; ao destruí-la, para de escutar
EventBus::Subscription sub = scene.events().subscribe<PlayerHit>([](const PlayerHit& e) { /* ... */ });

// quem avisa
scene.events().emit(PlayerHit{1});
```

!!! warning "A assinatura precisa ser guardada"
    `subscribe` devolve um `EventBus::Subscription`. Se você não guardar o valor (num membro da classe, por exemplo), ele é destruído na hora e a assinatura acaba.

Os eventos são **adiados**: `emit` põe na fila, e a cena entrega no fim de cada `flush`, na ordem em que foram emitidos. Eventos emitidos dentro de um handler vão para a rodada seguinte (até 8 rodadas por entrega).

A própria cena emite `ObjectDestroyed` (com o `id`) para cada objeto liberado.

## Pela memória do mundo

Para estado que precisa sobreviver quando a sala é descarregada, use o `WorldState`, indexado pelo id da entidade no LDtk. A alavanca e a porta conversam assim:

```cpp
state->set(door, "open", 1.0);              // a alavanca abre
bool aberta = state->get(entityIid, "open", 0.0) != 0.0;   // a porta lê
```

## Casos de uso

- **Uma alavanca que abre uma porta:** campo `door` (EntityRef) na alavanca, estado no `WorldState`. Veja [Objeto novo](objeto-novo.md).
- **Todos os inimigos fogem quando o player pega um item:** o item emite um evento; cada inimigo assina no `onCreate` e guarda a `Subscription` num membro.
- **Um objeto que segue o player:** guarde o id do player e, em `onStep`, use `scene.findObject(playerId)`, que devolve `nullptr` se ele não existir mais.
