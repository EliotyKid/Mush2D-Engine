# D2: Iki se move (estados, poderes de movimento e game juice)

Fase D2 do [ROADMAP](../../ROADMAP.md). Base: `main` em `d8435d7` (D1 feito: prefabs, reflexão, inspetor).

## Intenção

Ikigai é um metroidvania em pixel art com render e sensação de controle no nível do Ori. O D2 cria o Iki jogável: o corpo dele, os estados, o primeiro poder de travessia (estacas de gelo) e os recursos (vida e energia). Tudo que vem depois (combate, inimigos, chefe) usa a mesma máquina de estados e o mesmo modelo de habilidades e recursos.

O autor pediu explicitamente **todos os conceitos de game juice** que cabem no movimento. O controle tem que ser bom de sentir, não só correto.

Os sistemas crescem até o jogo final sem grande refatoração: interface do tamanho do jogo final, implementação do tamanho da demo.

## Decisões do autor

1. **Estacas de gelo = agarrar.** Em parede ou teto de gelo marcado, o Iki crava as estacas e fica preso. Na parede: sobe, desce e salta para longe. No teto: anda pendurado. Não há estaca arremessada.
2. **Energia: uma barra por elemento** (gelo e fogo), cada uma com recarga própria. Ataques combinados (D3+) gastam das duas.
3. **Travessia não gasta energia.** Escalar, agarrar e o futuro pulo duplo de fogo são livres. Energia é para combate e projéteis.
4. As propostas de comportamento abaixo (agarrar, teto, desbloqueio, pulo, vida, HUD, estados, dash, topdown) foram aprovadas como estão.
5. **Abordagem 1:** um `IkiObject` novo, máquina de estados genérica na engine, habilidades e recursos como componentes, juice em componentes reaproveitáveis. O `PlayerObject` antigo não muda.
6. **Sempre atualizar o manual (`manual/docs/`) e a documentação** (README, `docs/GUIA.md`, ROADMAP) quando algo novo entra.

## Objetivos

1. `StateMachine` genérica na engine, usada pelo Iki e reaproveitável por inimigos e NPCs.
2. `IkiObject` só de plataforma, com estados: parado, correndo, pulando, caindo, na parede, no teto, ferido e morto.
3. Habilidade `IceStakesAbility` como componente, desbloqueada pela flag global `ability.ice.stakes` no `WorldState`.
4. Superfície "gelo escalável" marcada no LDtk (IntGrid e `Terrain`).
5. `Resources`: vida inteira (5 na demo) e duas energias com recarga.
6. Pulo com coyote time, buffer, altura variável, gravidade menor no topo, queda mais rápida e correção de quina.
7. Animação por estado com eventos por quadro.
8. Game juice: squash e stretch, aceleração e desaceleração, inclinação, partículas e quadro de antecipação.
9. HUD mínimo de teste: vida e duas barras.
10. Uma sala de teste onde o Iki escala uma parede de gelo até uma área antes inacessível.
11. Manual e documentação atualizados.

## Não-objetivos

- Combate, dano vindo de inimigos, hit stop e tremor de câmera: D3 e D6.
- Áudio: D3.
- Pulo duplo de fogo, dash do Iki e outras habilidades. Só a interface para elas existe.
- HUD final, menus e remapeamento de controles: D9.
- Arte nova do Iki. Os estados sem animação própria usam quadros provisórios do `kaa_player`.

## Anti-objetivos

- Controle "duro": parar e virar instantâneos, pulo que ignora um aperto um pouco cedo ou um pouco tarde.
- Escalar como gerenciamento de recursos, ou com tempo limite.
- Mudar o comportamento das cenas Sandbox e Test ou do `PlayerObject` topdown.
- Uma máquina de estados que precise ser reescrita quando o ataque (D3) ou os inimigos (D4) chegarem.

## Comportamento

### Mover e pular

- **Correr:** acelera até `runSpeed` com `groundAccel` e para com `groundDecel`. No ar, usa `airAccel` e `airDecel`, que são menores. Virar no chão com velocidade alta solta poeira de virada.
- **Pulo:** só a tecla `Z`. No Iki, a seta para cima não pula mais, porque ela agarra o teto e sobe a parede. É uma decisão do Claude; o modo topdown antigo não muda.
- **Coyote time:** o pulo ainda vale até `coyoteTime` (0,1 s) depois de sair de uma borda sem pular.
- **Buffer de pulo:** um pulo apertado até `jumpBuffer` (0,12 s) antes de tocar o chão (ou antes de agarrar uma parede) acontece no toque.
- **Altura variável:** soltar `Z` subindo multiplica a velocidade vertical por `jumpCutFactor`. Isso já existe e é mantido.
- **Topo do pulo:** com `|vy| < apexThreshold` e `Z` segurado, a gravidade vira `gravity * apexGravityFactor` (0,5), para um instante de flutuação.
- **Queda:** descendo, a gravidade vira `gravity * fallGravityFactor` (1,6), limitada a `maxFallSpeed`.
- **Correção de quina:** subindo, se a cabeça bate num teto e deslocar o Iki até `cornerCorrection` (0,2 u) para o lado deixa a subida livre, ele é deslocado e continua subindo.
- **Quadro de antecipação:** o pulo sai no mesmo quadro do aperto, porque atrasar a física atrapalha o controle. A antecipação é só visual: um squash curto antes do stretch.

### Estacas de gelo

Só valem com a habilidade desbloqueada e numa superfície com `surface == IceClimb`. Sem a habilidade, essa superfície é uma parede ou teto comum.

- **Agarrar a parede:** no ar, encostado numa parede de gelo e segurando a direção dela, o Iki entra em `WallCling`. Não precisa de botão, e não há limite de tempo.
- **Na parede:** `↑` sobe e `↓` desce, a `wallClimbSpeed`. Sem direção, ele fica parado. Soltar a direção da parede não solta o Iki; só o pulo, `↓` no pé da parede ou o fim da superfície soltam.
- **Salto da parede:** `Z` dá velocidade `wallJumpVelocity` (para longe e para cima). Durante `wallJumpLock` (0,15 s) a entrada horizontal é ignorada, para ele não voltar colado.
- **Topo da parede:** se a superfície de gelo acaba subindo, o Iki recebe um pequeno impulso para cima e para frente (`ledgeHop`) e sobe para a beirada.
- **Teto:** com um teto de gelo logo acima, segurar `↑` no ar prende o Iki em `CeilingHang`. Ele anda pendurado com `←`/`→` a `ceilingMoveSpeed`, e solta com `↓` ou `Z` (cai, não pula). Se o teto de gelo acaba, ele solta.
- **Juice:** cristais de gelo saem ao cravar. Enquanto ele se move na parede ou no teto, saem farpas a cada `climbParticleInterval`. Um squash curto acontece ao agarrar.

### Recursos

- `Resources`: `maxHealth` 5, `health`, `maxIce` e `maxFire` 100, `ice` e `fire`, `regenPerSecond` 10, `regenDelay` 1 s (a contagem recomeça a cada gasto, separada por elemento).
- API:
  - `damage(int)`, `heal(int)`;
  - `bool spend(Element, float)`: falso se não há energia suficiente, e não gasta nada;
  - `bool spendBoth(float ice, float fire)`;
  - `float energy(Element)`.
- Eventos no `EventBus`: `HealthChanged`, `EnergyChanged`, `Died`.
- Vida em 0 → estado `Dead` → aciona a morte que já existe (fade, restauração da sala, checkpoint). Ao renascer, vida e energias voltam ao máximo.
- **Debug:** `J` causa 1 de dano e `L` gasta 30 de gelo e 30 de fogo, só no Debug. Assim dá para ver o estado `Hurt`, o HUD e a recarga sem ter combate.

### Desbloqueio

- O `WorldState` ganha flags globais: `bool flag(name)` e `setFlag(name, bool)`, guardadas sob um iid reservado (`"_global"`). A restauração da sala na morte só mexe nos iids da sala, então a flag não se perde ao morrer. Isso vale porque o save (D10) serializa o `WorldState` inteiro.
- `ItemObject` com `kind` que começa com `ability.` chama `setFlag(kind, true)` ao ser pego. Na sala de teste, um item `ability.ice.stakes` fica antes da parede de gelo.
- Cada habilidade é um componente com `MUSH2D_EXPORT(std::string, unlockFlag, ...)` e `bool unlocked(scene)`. Assim, o pulo duplo de fogo será um componente novo com outra flag, sem mudar o Iki.

## Desenho

### `StateMachine` (`src/engine/world/StateMachine.hpp`)

```cpp
template <class Owner>
struct State {
    virtual ~State() = default;
    virtual const char* name() const = 0;   // also the animation key
    virtual void enter(Owner&) {}
    virtual void exit(Owner&) {}
    virtual const char* update(Owner&, float dt) = 0; // nullptr = stay; otherwise next state name
};

template <class Owner>
class StateMachine {
public:
    void add(std::unique_ptr<State<Owner>>);
    void start(Owner&, const char* name);
    void update(Owner&, float dt);           // runs update, applies at most one transition per call
    void change(Owner&, const char* name);   // forced (damage, death)
    const char* current() const;
    float timeInState() const;
    std::function<void(const char* from, const char* to)> onChange; // animator, debug
};
```

- Nome desconhecido em `change` ou `start`: aviso uma vez em stderr, e o estado fica.
- Estados são classes pequenas no jogo (`src/game/iki/states/*.hpp`). Os estados compartilham a física através de métodos do `IkiObject` (`applyGravity`, `runHorizontal`, `tryJump`), não por herança entre estados.
- O inspetor F1 mostra o estado atual e o tempo nele, só leitura.

### `IkiObject` (`src/game/objects/IkiObject.hpp/.cpp`)

- `MUSH2D_OBJECT(IkiObject, GameObject)`, prefab `assets/prefabs/iki.prefab.json`. A entidade `PlayerStart` passa a criar o Iki no `IkigaiGame`. Sandbox e Test continuam com o `PlayerObject`.
- Todos os números de movimento são `MUSH2D_EXPORT` e podem ser ajustados no inspetor e salvos no prefab:
  - `runSpeed`, `groundAccel`, `groundDecel`, `airAccel`, `airDecel`;
  - `gravity`, `jumpSpeed`, `jumpCutFactor`, `apexThreshold`, `apexGravityFactor`, `fallGravityFactor`, `maxFallSpeed`;
  - `coyoteTime`, `jumpBuffer`, `cornerCorrection`;
  - `wallClimbSpeed`, `wallJumpVelocity`, `wallJumpLock`, `ledgeHop`, `ceilingMoveSpeed`;
  - `hurtDuration`, `hurtKnockback`, `invulnerableTime`.
- Usa o `CharacterController` existente (rampas, plataformas atravessáveis, descer com `↓`+pulo).
- A lógica de tempo do pulo (coyote e buffer) fica numa struct pura `JumpAssist` (em `src/engine/world/`), testável sem cena: `void tick(dt, grounded, jumpPressed)` e `bool consumeJump()`.

### Superfície de gelo escalável

- `Segment` ganha `uint8_t surface` (`Surface::Default`, `Surface::IceClimb`).
- **IntGrid valor 9 "IceClimb":** colide como o valor 1 (sólido); as faces geradas recebem `surface = IceClimb`. A mesclagem de faces colineares só une faces da mesma superfície.
- **`Terrain`:** a entidade ganha o campo booleano `iceClimb`, cujos segmentos recebem a superfície.
- `CharacterController::MoveResult` ganha `wallLeft`/`wallRight` e `ceilingSurface`/`wallSurface`. Uma consulta separada `probeWall(body, side, query)` e `probeCeiling(body, query)` devolve a superfície encontrada a até 0,05 u. Assim, o Iki detecta a parede mesmo parado ao lado dela.
- **LDtk:** o valor 9 com cor azul-gelo é adicionado à definição da camada `Collision` em `showcase.ldtk`. Uma sala nova, `IceTest`, é criada por script, sem tocar nas salas existentes (veja "Decisão do autor sobre o LDtk").

### Animação por estado e eventos por quadro

- O Iki usa o `SpriteAnimator` do D1. Ao mudar de estado, chama `setState(nomeDoEstado)`. O `animations` do prefab liga estado e animação:
  - idle → `idle`, run → `run`, jump → `jump`, fall → `fall`;
  - wall → `push` (provisório), ceiling → `balance_back` (provisório);
  - hurt → `attack_leave` (provisório), dead → `fall` (provisório).
- **Eventos por quadro:** o prefab ganha `"animationEvents": {"run": {"3": "step", "11": "step"}}`. O `SpriteAnimator` emite `AnimationEvent{object, animation, name}` no `EventBus` quando entra no quadro. O Iki ouve `step` e solta poeira de passo. O D3 usará `hit` do mesmo jeito.
- As animações vêm das tags do Aseprite que já existem nos atlas (`frameTags`).

### Game juice

| Efeito | Onde | Parâmetros (exportados) |
|---|---|---|
| Squash e stretch | componente `SquashStretch` (engine), que mexe só em `SpriteData::drawSize` e volta à escala 1 com mola | `stiffness`, `damping`; disparos: `jumpStretch`, `landSquash` (proporcional à velocidade de queda), `clingSquash` |
| Inclinação | o Iki gira o sprite levemente pela velocidade horizontal | `leanMaxDegrees` (4) |
| Aceleração e desaceleração | física do Iki | veja a lista do `IkiObject` |
| Gravidade no topo, queda rápida, correção de quina | física do Iki | veja "Mover e pular" |
| Partículas | componente `ParticleEmitter` (engine) | veja abaixo |
| Antecipação | squash curto de 2 quadros no pulo saindo do chão | `jumpAnticipation` |

**`ParticleEmitter` (`src/engine/world/Particles.hpp/.cpp`):**
- O emissor é o mínimo que o D3 vai expandir. Simulação na CPU, num pool fixo por emissor (`maxParticles`).
- O desenho usa o caminho de sprites que já existe: cada partícula é um `SpriteData` com um quadro de atlas ou com um pixel branco tingido.
- Configuração nomeada no prefab (`"particles": {"land_dust": {...}}`), com os campos `count`, `speed` [min,max], `angle` [min,max], `lifetime` [min,max], `gravity`, `drag`, `size` [início,fim], `color` [início,fim] (alpha incluído), `frame` (opcional), `additive` e `layer`.
- API: `burst(name, position, direction=+1)` espelha os ângulos quando o Iki olha para a esquerda.
- Usos no D2: `land_dust`, `run_dust` (no evento `step`), `turn_dust`, `jump_dust`, `ice_stake` (ao agarrar) e `ice_shards` (ao escalar).
- Sem luz nem emissão por padrão, porque o player não deve emitir luz. `additive` existe para o D3.

### HUD de teste

- Desenhado com o `ImGui::GetForegroundDrawList()` no canto superior esquerdo: 5 corações (círculos cheios ou vazios), uma barra azul de gelo e uma barra laranja de fogo.
- Aparece sempre no `IkigaiGame`. Será trocado pela UI do D9; por isso fica num arquivo só (`src/game/ui/DebugHud.cpp`).

### Sala de teste `IceTest`

- Ligada a uma sala existente por uma passagem. A posição exata é do construtor, desde que fique fora do caminho principal da vitrine.
- Contém: o item `ability.ice.stakes`, uma parede de gelo de ~8 u, um teto de gelo de ~6 u sobre um vão sem chão e uma plataforma alta antes inacessível, com um `Checkpoint`.
- Sem a habilidade, não há como subir.

## Como cresce até o jogo final

- **Estados novos** (ataque, defesa, dash, nado, morte especial) entram como novas classes `State<IkiObject>`, sem mudar a máquina. Inimigos (D4) e NPCs (D7) usam `StateMachine<Inimigo>` da mesma forma.
- **Habilidades novas** entram como componentes com `unlockFlag`. Pulo duplo de fogo: `FireDoubleJump` consulta `JumpAssist` e o estado no ar. Talismãs (pós-demo) modificam números exportados das habilidades e dos `Resources`.
- **Recursos:** `Element` (`Ice`, `Fire`, e outros no futuro) já é o índice das energias. Combinações chamam `spendBoth`. Talismãs mudam `max*` e `regen*`.
- **Superfícies:** `Surface` ganha valores (gelo derretível, fogo queimável, água congelável) no D5 sem mudar `Segment`.
- **Partículas:** o D3 acrescenta emissores contínuos, colisão simples, luz por partícula e GPU se o perfil pedir, mantendo `burst(name, ...)`.
- **Eventos de animação:** o D3 usa `hit` e `whoosh`; o D8 (cutscenes) usa o mesmo canal.
- **HUD:** o D9 substitui o `DebugHud`; os eventos `HealthChanged` e `EnergyChanged` continuam sendo a fonte.
- **Flags globais do `WorldState`** servem a diálogos (D7), cutscenes (D8) e save (D10).

## Testes (`mush2d_tests`)

- `state_machine_transitions`: start, transição por `update`, `change` forçado, `onChange`, `timeInState`, nome desconhecido avisa e mantém o estado.
- `jump_assist_coyote`, `jump_assist_buffer`, `jump_assist_no_double_consume`.
- `intgrid_ice_climb_surface`: o valor 9 gera faces sólidas com `IceClimb`, e a mesclagem não une gelo com sólido comum.
- `terrain_ice_climb_field`.
- `probe_wall_and_ceiling_surface`.
- `iki_wall_cling_requires_ability`: com a flag desligada, o Iki não agarra; com a flag ligada, agarra, sobe, e o salto da parede trava a entrada horizontal pelo tempo configurado.
- `iki_ceiling_hang_moves_and_releases`.
- `iki_corner_correction`.
- `iki_apex_and_fall_gravity`.
- `resources_spend_regen_delay`: `spend` falso não gasta, a recarga espera `regenDelay` por elemento, `spendBoth` é atômico.
- `resources_death_event`.
- `world_state_global_flag_survives_room_restore`.
- `item_ability_kind_sets_flag`.
- `animation_frame_event_emitted_once_per_entry`.
- `particles_burst_lifetime_and_pool_limit`.
- `squash_stretch_returns_to_rest`.

Simulações do Iki rodam headless, no estilo dos testes do slime e do checkpoint.

## Critérios de pronto

1. Na sala `IceTest`, sem o item, não há como chegar à plataforma alta. Depois de pegar o item, o Iki escala a parede de gelo, atravessa o teto pendurado e chega à plataforma. Verificado em execução.
2. Os estados aparecem corretos no inspetor F1 durante esse percurso.
3. Coyote, buffer, altura variável, topo, queda rápida e correção de quina são verificados por teste e sentidos em execução.
4. Todos os efeitos de juice da tabela são visíveis em execução, e o player não emite luz.
5. HUD: `J` tira um coração e mostra `Hurt`; `L` baixa as barras, que recarregam depois de 1 s. A vida em 0 aciona a morte, e o Iki renasce no checkpoint com tudo cheio.
6. Sandbox e Test continuam iguais, com o `PlayerObject` topdown.
7. Todos os testes passam e há zero erros de validação do Vulkan.
8. Manual atualizado:
   - página nova "Iki: estados e habilidades" em "Fazendo o jogo";
   - página nova "Game juice";
   - `objetos.md` (Iki, item de habilidade);
   - `sistemas/world.md` (StateMachine, JumpAssist, Surface, Particles, SquashStretch);
   - `mundo-e-salas.md` / `colisao-e-rampas.md` (valor 9, `iceClimb`);
   - `nav` do `mkdocs.yml`.
9. README (controles `Z`, `↑`/`↓` na parede, `J`/`L` de debug), `docs/GUIA.md` e ROADMAP (D2 marcado, decisão de energia registrada como resolvida) atualizados.

## Abordagens consideradas

1. **`IkiObject` novo + `StateMachine` genérica + habilidades, recursos e juice como componentes.** Escolhida pelo autor. Deixa o topdown antigo intacto, e cada sistema serve a fases futuras.
2. **Reformar o `PlayerObject`.** Descartada: o modo topdown e o modo plataforma continuariam misturados, com risco alto para as cenas antigas.
3. **Estados descritos em JSON.** Descartada: cada estado precisa de lógica em C++, e os números ajustáveis já ficam no prefab.

## Decisão do autor sobre o LDtk

- O valor 9 da IntGrid e a sala `IceTest` entram no `showcase.ldtk`. Isso é uma exceção explícita à regra do D1 de não alterar esse arquivo.
  - As salas, entidades e camadas existentes não mudam: só há acréscimos.
  - O script que faz os acréscimos fica em `tools/` e é commitado.
  - Depois, o autor abre e salva o arquivo no LDtk uma vez para confirmar o formato.

## Decisões do Claude (após a revisão do construtor; o autor pode corrigir)

1. **Inclinação:** usa `transform.rotation`, que o `SpriteBatch` já desenha. Não há mudança no renderizador. A rotação é aplicada em torno da origem do objeto (os pés).
2. **`SquashStretch` e `SpriteAnimator`:** o animador escreve `drawSize` e `offset` pelo quadro. O `SquashStretch` roda depois dele, no mesmo passo, e multiplica `drawSize` pelo fator atual. Também ajusta `offset` para o sprite amassar e esticar ancorado nos pés. Ele nunca mexe em `transform.scale`. O `IkiObject` chama os dois em ordem fixa no `update`, em vez de depender da ordem dos componentes.
3. **Morte:**
   - `Resources` chega a 0 → emite `Died` → o Iki entra em `Dead` (sem controle, animação de morte).
   - O `IkigaiGame` ouve `Died` e chama o `killPlayer` existente (fade, restauração da sala, checkpoint).
   - O renascimento **reaproveita o mesmo objeto**, como acontece hoje: move o Iki e chama `IkiObject::respawn()`, que enche `Resources`, zera a invulnerabilidade, a velocidade e os temporizadores de pulo e força `change(*this, "idle")`.
   - Um hot reload do prefab recria o Iki do zero (decisão 8 do D1), então não sobra estado velho.
4. **O jogador é encontrado por tag, não por tipo.** O Iki e o `PlayerObject` recebem a tag `"player"` (o `PlayerObject` só ganha essa tag no construtor).
   - Passam a usar `hasTag("player")` em vez de `dynamic_cast<PlayerObject*>`: `Checkpoint`, `Item`, `Lever`, `Teleport` e os pontos do `IkigaiGame` (câmera, morte, teletransporte, respawn).
   - O que precisa de métodos específicos usa uma interface pequena `IPlayer` na engine: `respawnAt(pos)`, `velocity()`, `downPressedThisFrame()`. Ela é implementada pelos dois objetos.
   - O pulo no slime vai para o Iki usando a mesma API do `SlimeEnemy` que o `PlayerObject` usa.
   - Isso segue o princípio do ROADMAP de que sistemas conversam por eventos e tags.
5. **Prioridades entre estados:**
   - No ar, junto a uma parede de gelo e segurando a direção dela → `WallCling`. Isso vale mesmo com `↑` segurado.
   - `↑` sem empurrar uma parede de gelo, com teto de gelo ao alcance → `CeilingHang`.
   - Subindo na parede até um teto de gelo (canto interno), a transição `WallCling` → `CeilingHang` é direta. Do teto, andar até uma parede de gelo e segurar a direção dela dá `CeilingHang` → `WallCling`.
   - Um `Z` no buffer, ao agarrar, vira salto da parede na hora.
   - `Z` em `CeilingHang` solta o Iki (cai) e consome o buffer.
   - Depois de soltar a parede com `↓` ou no fim da superfície, há um coyote de parede de `coyoteTime`, durante o qual `Z` ainda dá o salto da parede.
6. **Dano:** `struct DamageInfo { int amount; glm::vec2 direction; Element element = Element::None; }` já existe no D2; o D3 acrescenta campos. Regras:
   - `IkiObject::hurt(const DamageInfo&)` empurra no sentido de `direction`. O `J` de debug usa o sentido oposto ao que o Iki olha.
   - O dano interrompe parede, teto e pulo, e entra em `Hurt` por `hurtDuration`.
   - Durante `invulnerableTime`, dano novo é ignorado, e o sprite pisca (juice).
7. **Eventos de animação:** o índice é relativo à animação (o primeiro quadro da tag é 0). O evento dispara sempre que o quadro passa a ser o atual, inclusive a cada volta do loop. "Uma vez por entrada" quer dizer uma vez cada vez que o quadro entra, nunca repetido enquanto ele fica na tela.
8. **Entrada injetável:** o Iki lê `applyInput(const InputState&)`, como o `PlayerObject` nos testes atuais (`stepPlayer`). As teclas `J` e `L` ficam no `IkigaiGame`, não no Iki.

## Deixado para o construtor

- Valores iniciais exatos dos parâmetros de juice e de movimento (os citados são pontos de partida).
- Forma exata da sala `IceTest` e onde ela se liga.
- Organização interna dos estados e de quais métodos do `IkiObject` eles usam.
- Formato interno do pool de partículas.
