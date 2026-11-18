# Mush2D: Modelo de entidades robusto

Data: 2026-11-19
Status: aprovado pelo autor ("corrija os problemas e implemente as suas sugestões"). As escolhas de como construir foram delegadas ao Claude.

## Intenção

O sistema de entidades orientado a objetos (`GameObject` + `Scene2D`) precisa ficar completo e robusto. Ele vai servir de base para a Fase 1 (várias salas, câmeras), para as cutscenes e para jogos de gêneros diferentes. O autor ainda não sabe quais entidades os jogos terão, então o modelo deve ser genérico.

## Problemas atuais (todos corrigidos neste trabalho)

1. **Criar entidades durante `step` é comportamento indefinido.** `createObject` faz `push_back` no vetor que está sendo iterado.
2. **Não existe destruição individual.** Só `clear()` remove objetos. `active = false` deixa o objeto vivo para sempre.
3. **IDs frágeis.** Os IDs `uint32` são buscados por varredura linear e reiniciam em 1 a cada `clear()`, então uma referência antiga pode apontar para outra entidade.
4. **Componentes fixos dentro de `GameObject`.** Não há como anexar comportamento ou dados novos sem mexer na base.
5. **Faltam peças:** hierarquia pai/filho, eventos entre entidades e ordem de atualização controlada.
6. **Colisão O(n²).**

## Objetivos

- Herança continua sendo o modelo principal. Entidades são classes derivadas de `GameObject` com métodos virtuais, como hoje, e o código de jogo existente continua funcionando com mudanças mecânicas.
- Handles seguros: `GameObjectId` com índice e geração, busca O(1) e handle velho retornando `nullptr`.
- Criação e destruição seguras a qualquer momento, inclusive dentro de callbacks, aplicadas em pontos definidos do frame.
- Hierarquia pai/filho com transform local e de mundo. Destruir o pai destrói os filhos.
- Componentes anexáveis (`Component`), com ciclo de vida próprio. O primeiro componente de engine é `LightComponent`, uma luz presa a uma entidade.
- Barramento de eventos tipado na cena, com entrega diferida e segura.
- Ordem de atualização por entidade (`updateOrder`).
- Broadphase em grade uniforme para colisão e consultas espaciais.

## Não-objetivos

- ECS orientado a dados (arquétipos, SoA).
- Serialização de entidades.
- Prefabs.
- Scripting.
- Física rígida.

Ficam para quando houver necessidade.

## Anti-objetivos

- Quebrar a sala vitrine ou as cenas Sandbox/Test.
- Exigir reescrita das classes de jogo existentes.
- Perder desempenho na vitrine. A soma de CPU por frame não pode piorar perceptivelmente.

## Abordagens consideradas

1. **Reforçar a herança e adicionar componentes opcionais.** ← **escolhida.** Mantém o estilo que o autor já usa (classes com `onStep`), corrige a robustez e ganha composição onde ajuda.
2. **Migrar para ECS (EnTT ou próprio).** É o mais rápido em escala massiva, mas muda o jeito de programar o jogo inteiro, e o autor pediu orientação a objetos.
3. **Só componentes, sem herança (estilo Unity).** É flexível, mas também reescreve todas as entidades existentes.

## Desenho

### Identidade e armazenamento (`Scene2D`)

- `GameObjectId` vira `struct { uint32_t index; uint32_t generation; }`, com `==`, `!=`, `operator bool` (válido se generation ≠ 0) e `std::hash`. `InvalidGameObjectId = {}`.
- A cena guarda um *slot map*: `std::vector<Slot>`, em que `Slot = { std::unique_ptr<GameObject>; uint32_t generation; }`, mais uma lista livre de índices. Destruir incrementa a geração do slot. `find(id)` é O(1) e retorna `nullptr` se a geração não bate.
- `clear()` destrói tudo, mas **não** reinicia as gerações. Handles de antes do `clear` continuam inválidos.

### Ciclo de vida e flush

- `createObject<T>(args...)` cria o objeto e atribui o id na hora. O objeto entra numa lista *pendente* e passa a existir para `find`, mas só começa a receber `onStep` depois do próximo flush. `onCreate` é chamado imediatamente, como hoje.
- `destroy(id)` marca o objeto e todos os descendentes como `pendingDestroy`. No flush, chama `onDestroy` (filhos antes do pai), remove dos componentes, da grade e do pai, e libera o slot. Destruir duas vezes não faz nada.
- **Pontos de flush:** no início de `step`, depois de `step` e depois de `lateStep`. O jogo também pode chamar `scene.flush()`.
- **Iteração segura:** `step` e `lateStep` iteram uma lista ordenada de objetos vivos, montada no flush. Objetos marcados para destruição no meio do frame não recebem mais callbacks naquele frame.
- `active` continua existindo, com o sentido de "pausado", e é diferente de destruído.

### Hierarquia e transforms

- `transform` passa a ser **local**. Em objeto sem pai, local e mundo são iguais, então o código atual não muda de comportamento.
- `setParent(child, parent, keepWorld = true)`, `parent()`, `children()`.
- `worldTransform()` compõe posição, rotação e escala do pai: `world.pos = parent.pos + rotate(parent.rot) * (parent.scale * local.pos)`, `rot = parent.rot + local.rot`, `scale = parent.scale * local.scale`. É calculado sob demanda com cache invalidado por *dirty flag*, propagada aos filhos quando `transform` muda. Como `transform` é um campo público, a invalidação acontece no flush e no início de cada `step`, recalculando o mundo de todos uma vez por frame em ordem de hierarquia.
- O renderer (`appendSceneSprites`), a colisão e a câmera usam o transform de mundo.
- Ciclo na hierarquia (definir como pai um próprio descendente) é recusado com aviso.

### Componentes

- `class Component { public: virtual ~Component(); virtual void onAttach(GameObject&, Scene2D&) {} virtual void onStep(GameObject&, Scene2D&, float dt) {} virtual void onDetach(GameObject&, Scene2D&) {} bool enabled = true; };`
- Em `GameObject`: `T& addComponent<T>(args...)`, `T* getComponent<T>()` (busca linear com `dynamic_cast` em poucos componentes, o que é aceitável) e `removeComponent<T>()`, que é diferido.
- Componentes recebem `onStep` logo após o `onStep` do dono, em ordem de inclusão.
- `sprite`, `collider`, `health` e `trigger` continuam como dados embutidos. São usados por sistemas quentes (renderer e colisão) e mudá-los quebraria o código de jogo sem ganho. Ficam registrados como candidatos a virar componentes no futuro.
- **`LightComponent`** (engine): guarda `GpuLight` com posição *local*. A cada frame, o renderer coleta as luzes de componentes habilitados com a posição de mundo do dono e adiciona à `LightList`. É a base para luz presa a tocha, projétil etc.

### Eventos

- `scene.events()`: `EventBus`.
- `Subscription subscribe<E>(std::function<void(const E&)>)`: a `Subscription` é RAII e cancela a inscrição no destrutor (move-only).
- `emit<E>(E)` enfileira o evento. `dispatch()` entrega na ordem de emissão e roda nos pontos de flush. Eventos emitidos durante a entrega vão para a próxima rodada, com limite de 8 rodadas por flush e aviso se o limite for atingido.
- Eventos padrão da engine: `ObjectDestroyed { GameObjectId id; }`. Os eventos de trigger continuam nos virtuais `onTriggerEnter`/`onTriggerExit`.

### Ordem de atualização

- `int updateOrder = 0` em `GameObject`. A lista viva é ordenada de forma estável por `updateOrder` e depois pela ordem de criação. Mudanças só valem no próximo flush.

### Colisão (broadphase)

- `SpatialGrid` com células de 2×2 unidades: `insert(id, AABB)`, `update(id, AABB)`, `remove(id)` e `query(AABB, out vector<GameObjectId>)` sem duplicatas.
- A cena mantém a grade sincronizada para objetos com collider habilitado. O AABB de mundo é atualizado no início de cada `step` e quando `collidesAtPosition` move o objeto.
- `Collision2D::collidesAtPosition` e a varredura de triggers do jogo passam a consultar a grade.
- `Collision2D::queryAABB(scene, box)` fica disponível para o jogo.

## Testes (`mush2d_tests`)

- `spawn_during_step_is_safe`: um objeto que cria outro no `onStep` em todo frame, rodando 100 frames sem crash. O filho recebe o primeiro `onStep` só no frame seguinte.
- `destroy_during_step_is_deferred`: destruir a si mesmo e a outro no `onStep`. O `onDestroy` acontece uma vez, e `find` retorna `nullptr` depois do flush.
- `stale_handle_returns_null`: destruir e criar outro objeto que reusa o slot. O id antigo resolve para `nullptr` e o novo resolve.
- `clear_invalidates_old_handles`.
- `child_world_transform`: pai em (2,0) com rotação 90° e escala 2, filho local em (1,0) → mundo (2,2). Mover o pai atualiza o filho.
- `destroy_parent_destroys_children`.
- `reparent_cycle_rejected`.
- `component_lifecycle_and_step_order`.
- `event_bus_deferred_dispatch_and_unsubscribe`.
- `update_order_respected`.
- `spatial_grid_matches_bruteforce`: 500 AABBs aleatórios (seed fixa), 200 consultas. Os resultados da grade são iguais aos da força bruta.

## Critérios de pronto

1. Todos os testes acima passam.
2. A sala vitrine, Sandbox e Test rodam como antes: mesma aparência em captura de tela, player anda e pula, TAB/F5 funcionam. Zero erros de validação.
3. O código de jogo existente compila. As mudanças nele devem ser mecânicas (tipo do id, `transform` local).
4. Nenhum `#include` de `src/game` em `src/engine`.
