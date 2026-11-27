# GameMaker → Mush2D

| GameMaker | Mush2D | Onde fica |
|---|---|---|
| **Object** (obj_player) | uma **classe** que herda de `GameObject` | `examples/iki_demo/src/iki_demo/objects/*.hpp` |
| **Instance** | um objeto criado na cena com `scene.createObject<T>()` | — |
| **Create Event** | `onCreate(Scene2D& scene)` | dentro da classe |
| **Step Event** | `onStep(Scene2D& scene, float dt)` | dentro da classe |
| **End Step** | `onLateStep(...)` | dentro da classe |
| **Collision Event** (com trigger) | `onTriggerEnter` / `onTriggerExit`. Só entre o player e um objeto trigger | dentro da classe |
| **Destroy Event** | `onDestroy(...)` | dentro da classe |
| `instance_create_layer` | `scene.createObject<MinhaClasse>(...)` | qualquer lugar |
| `instance_destroy` | `scene.destroy(id)` | qualquer lugar |
| `id` da instância | `GameObjectId id` | campo do objeto |
| `noone` | `InvalidGameObjectId` | — |
| `x, y` | `transform.position` (em **unidades**; 1 unidade = 32 px) | campo do objeto |
| `image_xscale` | `transform.scale.x` (negativo = espelhado) | campo do objeto |
| `image_blend` | `sprite->color` (RGBA de 0 a 1) | campo do objeto |
| `sprite_index` | `sprite` (atlas + nome do frame) | campo do objeto |
| `depth` | `sprite->layer` e `sprite->orderInLayer` | campo do objeto |
| Máscara de colisão | `collider` (tamanho, `isTrigger`) | campo do objeto |
| `event_inherited()` | `ClassePai::onCreate(scene)` | dentro da classe |
| `keyboard_check` / `_pressed` / `_released` | `down` / `pressed` / `released` de um botão do `InputState` | `src/engine/platform/InputState.hpp` |
| **Room** | **sala do LDtk** (nível) | `assets/levels/showcase.ldtk` |
| Room Editor | **LDtk** (editor externo) | — |
| Tile layer / Instance layer | camadas de tiles / de entidades do LDtk | — |
| Variáveis de criação da instância | **campos** da entidade no LDtk | — |
| `persistent` | `persistent = true` | campo do objeto |
| Sprite editor | Aseprite/GameMaker → atlas `.png` + `.json` | `assets/kaa/` |
| Variável global | membro do jogo (`IkiDemoGame`) ou `WorldState` | — |
| User event | evento do `EventBus` (`scene.events()`) | veja [Comunicação](../fazendo-o-jogo/comunicacao.md) |
| Objeto com valores editáveis | **prefab** (`assets/prefabs/<nome>.prefab.json`) | `MUSH2D_EXPORT` |
| Debugger de instâncias | janela "Cena (F1)" | veja [Painel F1](../fazendo-o-jogo/inspetor.md) |
| Luz | entidade `Light` no LDtk, ou `LightComponent` | — |

## Como uma entidade vira objeto

Cada entidade do LDtk cria o **prefab** de mesmo nome em `snake_case` (`Slime` → `assets/prefabs/slime.prefab.json`). O prefab diz a classe C++ (`"type"`) e os valores. Os campos da entidade com o nome de um atributo exportado (`MUSH2D_EXPORT`) sobrescrevem o valor.

Só as entidades especiais (`PlayerStart`, `Light`, `Terrain`) têm "fábrica" em código.

Detalhes em [Prefabs](../fazendo-o-jogo/prefabs.md).
