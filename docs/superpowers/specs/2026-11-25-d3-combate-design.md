# D3: Combate e "peso"

Fase D3 do [ROADMAP](../../ROADMAP.md). Base: `main` em `2494304`, com o D2 feito (Iki, estados, efeitos, recursos, estacas de gelo).

## Intenção

O Iki ganha o combate de gelo: golpe em sequência, projétil e escudo com aparo. Cada acerto precisa ter **peso** (hit stop, tremor, flash, partículas e som), no espírito do Ori. Tudo que o D3 cria serve aos inimigos do D4 e ao chefe do D11 só por prefab e estados, sem refatoração.

O D3 também traz o primeiro áudio da engine (efeitos e música por região) e o caminho de partículas com transparência que ficou pendente no D2.

## Decisões do autor

1. **Golpe de gelo estilo Ori:** 3 cortes encadeados para frente. Atacar no ar segura um pouco o Iki no ar.
2. **Defesa = escudo segurado com aparo perfeito.**
   - Segurando o botão, o escudo bloqueia o dano de frente; cada bloqueio gasta gelo.
   - Um golpe que chega nos primeiros 0,15 s vira aparo: não gasta gelo, atordoa o atacante e reflete projéteis.
3. **Propostas aprovadas sem mudança:** teclas, sequência, projétil, escudo, dano, peso, partículas com transparência, áudio, música por região, sons provisórios gerados e bonecos de treino. Estão detalhadas abaixo.
4. **Abordagem 1:**
   - componentes `Hitbox`, `Hurtbox` e `Damageable` na engine;
   - os ataques do Iki ficam na máquina `"action"`;
   - hit stop por escala de tempo global da cena;
   - tremor e flash como ações de efeito;
   - serviço de áudio com miniaudio.
5. Manual e documentação são atualizados a cada entrega (regra permanente do autor).

## Objetivos

1. `Hitbox`, `Hurtbox` e `Damageable` reaproveitáveis por qualquer entidade via prefab.
2. `DamageInfo` com quantidade, `Element`, empurrão e origem. Multiplicadores por elemento no prefab.
3. Combate do Iki na máquina `"action"`: sequência de 3 cortes, projétil de gelo e escudo com aparo.
4. Peso: hit stop global, tremor de câmera, flash no alvo e partículas de impacto, todos como ações de efeito.
5. Partículas com transparência (desbotam até alfa 0).
6. Áudio:
   - serviço com miniaudio;
   - ação `sound` nos efeitos;
   - música por região com transição suave;
   - sons provisórios gerados por script.
7. Dois bonecos de treino na sala `IceTest`, um fraco e um resistente a gelo; um deles atira.
8. Manual, README, GUIA e ROADMAP atualizados.

## Não-objetivos

- Inimigos com IA: só no D4. Os bonecos não andam.
- Efeitos de status (congelado, queimando), combos com fogo e ataques combinados.
- Áudio posicional, música adaptativa, mixer e volumes por canal na interface (D9).
- Arte final de golpe, escudo e projétil. São usados quadros provisórios e partículas.
- Câmera dinâmica: só no D6. O tremor do D3 é um deslocamento simples.

## Anti-objetivos

- Acerto sem peso: dano que só subtrai números, sem pausa, som ou reação visível.
- Combate que trava o movimento: o Iki tem que poder pular, cair e virar durante a sequência, como no Ori.
- Lógica de acerto escrita no código de cada ataque ou de cada inimigo.
- Hit stop que congela também o áudio ou a interface.

## Comportamento

### Teclas

`X` golpe, `C` projétil, `V` escudo (segurar). O `Z` continua sendo o pulo. As teclas entram no `InputState` (`attack`, `shoot`, `guard`) e no `GlfwInput`.

### Sequência de golpes (estilo Ori)

- `X` inicia o corte 1. Um novo `X` dentro de `comboWindow` (0,35 s) depois do fim do corte atual encadeia o próximo, até 3. Um aperto durante o corte fica guardado e encadeia ao fim dele.
- **Dano:** cortes 1 e 2 dão 1; o corte 3 dá 2. O empurrão do corte 3 é maior (`finisherKnockback`).
- Cada corte avança o Iki um pouco no chão (`attackStep`). O Iki pode se mover, pular e cair durante a sequência, porque a máquina `"move"` continua rodando. A velocidade no chão durante o corte é reduzida por `attackMoveFactor` (0,4).
- **No ar:** enquanto um corte está ativo, a gravidade é multiplicada por `airAttackGravityFactor` (0,25) e a queda é limitada a `airAttackMaxFall`. Só há **uma sequência por salto**: a próxima no ar exige tocar o chão, agarrar parede ou teto, ou levar dano.
- Golpe não gasta energia.
- Cada corte acerta cada alvo **uma vez só**. A hitbox guarda os alvos já atingidos e os limpa quando o corte acaba.
- Escudo e projétil cancelam a sequência. A sequência não roda na parede nem no teto.

### Projétil de gelo

- `C` lança um estilhaço reto na direção do olhar, a `shardSpeed` (14 u/s), por até `shardRange`. Custa `shardCost` (15) de gelo; sem energia, não sai e toca o som de falha.
- Dá 1 de dano de gelo. Quebra ao acertar uma hurtbox ou uma parede, soltando cacos.
- Os projéteis vêm de um pool (`ProjectilePool`, serviço da cena) e são reaproveitados.
- Tem um intervalo mínimo entre tiros: `shootCooldown`, 0,25 s.

### Escudo e aparo

- Segurar `V` no chão ergue o escudo. No ar, nada acontece.
- Com o escudo, o Iki anda a 30% de `runSpeed` (`guardMoveFactor`) e não pula. Soltar `V` abaixa o escudo.
- **Bloqueio:** dano que vem de frente (a `direction` do `DamageInfo` aponta contra o olhar do Iki) é anulado e custa `blockCost` (10) de gelo. Há um pequeno empurrão (`blockPush`). Dano de trás passa normalmente.
- **Aparo:** se o golpe de frente chega até `parryWindow` (0,15 s) depois de erguer o escudo:
  - não custa gelo;
  - o atacante recebe `Stunned` por `parryStun` (1 s);
  - um projétil é refletido: volta com a mesma velocidade, passa a pertencer ao Iki e acerta quem atirou;
  - os efeitos são mais fortes: hit stop maior, flash e som próprio.
- **Escudo quebrado:** se faltar gelo para um bloqueio, o golpe passa, o escudo cai e o Iki fica atordoado por `guardBreakStun` (0,5 s), sem escudo nem ataque.
- Para erguer de novo, é preciso soltar e apertar `V`.

### Dano, fraquezas e resistências

- `DamageInfo` ganha:
  - `float knockback`;
  - `GameObjectId source`;
  - `bool parryable = true`;
  - `bool projectile = false`.
- `Damageable` (componente) recebe o acerto, aplica o multiplicador do elemento (`multipliers`: `{"Ice": 2.0, "Fire": 0.5}`) e arredonda para o inteiro mais próximo, com mínimo de 1 se o multiplicador for maior que 0.
  - Multiplicador 0 = imune: sem dano, mas com efeito de "imune".
  - O dano é subtraído do `Resources` do dono e emite `DamageTaken{target, info, finalAmount}`.
  - Tem invencibilidade própria (`invulnerableTime`, padrão 0,2 s para alvos e o já existente no Iki).
- O Iki recebe dano pelo `Damageable` também. O `IkiObject::hurt` passa a ser chamado por ele, e o escudo decide antes.

### Peso (todos ações de efeito novas)

- **`hitstop`** `{"hitstop": 0.06}`:
  - congela a simulação da cena por esse tempo, com escala de tempo 0;
  - vários pedidos ficam com o maior, sem somar;
  - o áudio, a interface e o painel F1 continuam.
- **`shake`** `{"shake": {"amplitude": 0.15, "duration": 0.15}}`:
  - deslocamento aleatório da câmera que decai até zero;
  - vários tremores somam, com limite de amplitude.
- **`flash`** `{"flash": {"color": [1,1,1], "duration": 0.08}}`: tinge o sprite do dono com a cor e volta.
- **`sound`** `{"sound": "hit_ice", "volume": 1.0, "pitch": [0.95, 1.05]}`: toca um efeito da biblioteca de sons, com tom aleatório no intervalo.
- **Gatilhos novos:**
  - `hit:dealt` no atacante;
  - `hit:taken`, `hit:blocked`, `hit:parried`, `hit:immune` no alvo;
  - não há `hit:parry`: quem apara é o Iki, que é o alvo, então o gatilho dele é `hit:parried` (revisão final).

  Força = dano final.
- **Valores iniciais:**
  - acerto comum: hitstop 0,05, shake 0,1;
  - corte 3: hitstop 0,09, shake 0,18;
  - aparo: hitstop 0,15, shake 0,25, flash azul.

### Partículas com transparência

As partículas não aditivas passam a ser desenhadas num passe com mistura alfa, depois do resolve e antes do aditivo. Assim desbotam até alfa 0. Os efeitos do D2 voltam a terminar em alfa 0. A nota do ROADMAP sobre alfa < 0,5 é removida.

### Áudio

- **Serviço `Audio`** (engine), com miniaudio por FetchContent ou header único. Ele faz três coisas:
  - toca efeitos curtos (`.wav`);
  - toca uma faixa de música por vez, em loop, com transição cruzada de `musicFade` (1 s);
  - mantém volume geral, de efeitos e de música como números exportados no painel.
- **Biblioteca de sons:** `assets/audio/sfx/<nome>.wav` e `assets/audio/music/<nome>.ogg` ou `.wav`, carregados por nome, como os efeitos. Nome desconhecido avisa uma vez.
- **Música por região:** o `.region.json` ganha `"music": "<nome>"`. Ao trocar de região, a música faz a transição; mesma música, nada muda.
- **Sem dispositivo de áudio:** inclui os testes e o modo headless. O serviço vira silencioso e não falha.
- **Sons provisórios:**
  - `tools/gen_placeholder_sfx.py` gera os `.wav` com síntese simples (ruído, senoide, envelope): `slash_1..3`, `hit_ice`, `hit_block`, `parry`, `shard_throw`, `shard_break`, `guard_up`, `guard_break`, `no_energy`, `step`, `land`, `jump`;
  - duas faixas de música provisórias curtas em loop (`forest_theme`, `cave_theme`);
  - os arquivos gerados são commitados.

### Bonecos de treino

- O prefab `training_dummy` é um `GameObject`. Tem `Hurtbox`, `Damageable`, `Resources` com vida 10, `SpriteAnimator`, `EffectTriggers` e os efeitos de acerto.
- Quando a vida chega a 0, ele "quebra": some e volta com vida cheia depois de `respawnTime` (3 s). Não usa o `WorldState`.
- **`training_dummy_weak`** herda dele com `multipliers {"Ice": 2.0}`; **`training_dummy_resist`** herda com `{"Ice": 0.5}`.
- **`training_dummy_shooter`** herda do fraco e, a cada `shootInterval` (2,5 s), atira um projétil lento (5 u/s) de dano 1 na direção do Iki, se ele estiver a menos de 10 u. O projétil é aparável e refletível. A lógica fica num componente `DummyShooter`, sem máquina de estados.
- Os bonecos entram na sala `IceTest` pelo script `tools/add_ice_test_room.py`. São só acréscimos: dois bonecos no chão e o atirador perto da porta, longe do caminho de escalada.

## Desenho

### `Hitbox`, `Hurtbox`, `Damageable` (`src/engine/world/Combat.hpp/.cpp`)

- **`Hitbox` (componente):**
  - exports `size`, `offset`, `damage` (int), `element` (Element), `knockback`, `team` (string, `"player"` ou `"enemy"`), `parryable` e `active`;
  - `void activate()`, `void deactivate()` e `bool active() const`;
  - enquanto ativo, consulta a cada passo as hurtboxes sobrepostas na grade espacial da cena (`SpatialGrid`); acerta as de outro `team`, uma vez cada até `deactivate()`;
  - emite `HitEvent{attacker, target, DamageInfo}`;
  - a direção do `DamageInfo` vai do atacante ao alvo.
- **`Hurtbox` (componente):** exports `size`, `offset` e `team`; é registrada na grade espacial.
- **`Damageable` (componente):** recebe `HitEvent` cujo alvo é o dono e segue esta ordem:
  1. pergunta ao dono se ele bloqueia, pelo callback opcional `std::function<HitResult(const DamageInfo&)> guard`. O Iki usa isso para o escudo;
  2. aplica os multiplicadores;
  3. tira vida do `Resources`;
  4. emite `DamageTaken` e o gatilho de efeito correspondente;
  5. aplica o empurrão: o dono pode ouvir `DamageTaken`, e o Iki chama `hurt`.
- **Prefab:** hitbox e hurtbox ficam como componentes de nós filhos ou do próprio objeto. As hitboxes de cada corte do Iki são filhos nomeados (`Slash1`, `Slash2`, `Slash3`), ativados pelos eventos de quadro `hit_on` e `hit_off` (e por tempo se a animação não os tiver, campo `activeTime`).
- **`Stunned` (componente simples):** `start(seconds)`, `active()`. Inimigos do D4 e os bonecos o consultam.
- **Projétil (`Projectile`, componente):** exports `speed`, `range`, `team` e o `DamageInfo` base. Move em linha reta, colide com o terreno pelo `CollisionWorld` e usa uma `Hitbox` própria. Quebra no primeiro acerto ou parede e emite `move:break` para os efeitos. `reflect(newTeam)` inverte a direção e troca o time.
- **`ProjectilePool` (serviço):** `spawn(prefabName, pos, dir, team)`. Reaproveita objetos desativados do mesmo prefab.

### Iki: máquina `"action"`

- **Estados:** `none`, `slash` (com o número do corte), `shoot`, `guard`, `guard_break`. A máquina é registrada como `"action"`, aparece no F1 e emite `StateChanged`.
- **Regras com a máquina `"move"`:**
  - `wall`, `ceiling`, `hurt` e `dead` forçam `"action"` para `none` e bloqueiam novas ações;
  - `guard` só entra se `move` for `idle` ou `run`;
  - o `slash` no ar aplica a gravidade reduzida pelo método de física do Iki (consultado por `applyGravity`).
- **Animações provisórias do `kaa_player`:**
  - `attack_prepare`, `attack_stay` e `attack_leave` para os cortes;
  - `push` para o escudo;
  - o `SpriteAnimator` escolhe pela máquina `"action"` quando ela não está em `none`. Esse é o mapeamento `animations` com prefixo `action:`.
- Os eventos `hit_on` e `hit_off` entram em `animationEvents` dessas animações.

### Hit stop e tempo

- `Scene2D` ganha `void hitstop(float seconds)` e `float timeScale() const`. Durante o hit stop, `step` não avança objetos, componentes nem física, mas continua contando o próprio temporizador em tempo real.
- O `Engine` continua chamando a interface e o áudio normalmente.

### Tremor

- `Camera2D` ganha um deslocamento de tremor somado na hora de montar a vista, com `addShake(amplitude, duration)`.
- O D6 substitui o seguimento de câmera, mas mantém `addShake`.

### Partículas com transparência

- O `SpriteBatch` passa a separar as partículas não aditivas num terceiro grupo, desenhado num passe com mistura alfa sobre a imagem resolvida (sem iluminação diferida, cor já pronta).
- A ordem dos passes fica: opaco → luz → resolve → **partículas alfa** → aditivo → bloom.
- Zero erros de validação do Vulkan.

## Como cresce até o jogo final

- **Inimigos (D4) e chefe (D11):** recebem `Hitbox`, `Hurtbox`, `Damageable`, `Stunned` e projéteis pelo prefab. Os estados de ataque ficam na própria `StateMachine` deles, ativando hitboxes pelos eventos de quadro.
- **Fogo (pós-demo):** um segundo conjunto de cortes e projéteis com `Element::Fire`. Ataques combinados gastam com `spendBoth`.
- **Efeitos de status:** viram componentes (`Frozen`, `Burning`) aplicados pelo `Damageable` quando o `DamageInfo` ganhar um campo de status.
- **Combos:** a sequência é um array de cortes no prefab do Iki (`combo: [...]`), então variações e talismãs mudam dados.
- **Áudio:** posicional e música adaptativa entram no serviço `Audio` sem mudar a ação `sound` nem o campo `music` da região.
- **Peso:** novas ações de efeito (vinheta, zoom de câmera no D6) entram na mesma tabela.

## Testes (`mush2d_tests`)

- **Hitbox e hurtbox:**
  - `hitbox_hits_other_team_once_per_activation`;
  - `hitbox_ignores_same_team`;
  - `hitbox_reactivation_hits_again`.
- **Dano:**
  - `damageable_applies_multipliers_and_rounding`: 1 × 2 = 2; 1 × 0,5 = 1 (mínimo 1); 2 × 0,5 = 1;
  - `damageable_immune_zero_multiplier`;
  - `damageable_invulnerable_window`.
- **Sequência:**
  - `iki_combo_three_slashes_chain_within_window`;
  - `iki_combo_resets_after_window`;
  - `iki_combo_third_slash_damage_and_knockback`;
  - `iki_air_attack_reduced_gravity_once_per_jump`.
- **Escudo e aparo:**
  - `iki_guard_blocks_front_costs_ice`;
  - `iki_guard_back_hit_passes`;
  - `iki_parry_window_stuns_and_free`;
  - `iki_guard_break_without_energy`.
- **Projétil:**
  - `projectile_breaks_on_wall_and_hit`;
  - `projectile_pool_reuses`;
  - `projectile_reflect_on_parry_changes_team`;
  - `iki_shoot_costs_and_cooldown`.
- **Efeitos e tempo:**
  - `hitstop_freezes_scene_step_keeps_realtime`;
  - `hitstop_requests_take_max`;
  - `camera_shake_decays_to_zero`;
  - `effect_actions_shake_flash_hitstop_sound`.
- **Áudio:**
  - `audio_service_silent_without_device`;
  - `audio_unknown_sound_warns_once`;
  - `region_music_field_and_crossfade_request`.
- **Bonecos:**
  - `training_dummy_breaks_and_respawns`;
  - `dummy_shooter_fires_toward_player_in_range`;
  - `showcase_ice_test_dummies_load`.
- **Prefabs:** `game_prefabs_load_without_warnings` passa a cobrir os prefabs e efeitos novos.

## Critérios de pronto

1. Na `IceTest`, a sequência de 3 cortes no boneco fraco tira 2, 2 e 4. No resistente, tira 1, 1 e 1. Cada acerto tem hit stop, tremor, flash, partículas e som.
2. A sequência no ar segura o Iki por um instante e só uma vez por salto.
3. O projétil custa gelo, quebra na parede e no boneco, e não sai sem energia, com o som de falha.
4. O escudo bloqueia o projétil do atirador de frente, gastando gelo. O aparo no tempo certo reflete o projétil e atordoa o atirador. Sem gelo, o escudo quebra.
5. As partículas desbotam até sumir, sem cortar no meio.
6. A música troca suavemente entre as regiões `forest` e `cave`. O hit stop não congela o áudio.
7. Sem dispositivo de áudio (testes), nada falha.
8. Zero erros de validação do Vulkan. Sandbox, Test e o D2 inteiro continuam funcionando.
9. Documentação:
   - manual: página nova "Combate" em "Fazendo o jogo"; "Áudio" em "Sistemas"; `game-juice.md` com as ações novas; `iki.md` com a máquina `"action"`; `objetos.md` com os bonecos; `nav` atualizado;
   - README com as teclas `X`, `C` e `V`;
   - GUIA;
   - ROADMAP: D3 marcado e a nota de partículas removida.

## Abordagens consideradas

1. **Componentes `Hitbox`, `Hurtbox` e `Damageable` na engine; ataques na máquina `"action"`; hit stop global; tremor e flash como ações de efeito; serviço de áudio.** Escolhida pelo autor, porque serve a inimigos e chefe só por prefab.
2. **Sobreposição testada no código de cada ataque.** Descartada: repetiria a lógica em cada inimigo.
3. **Física de verdade para golpes.** Descartada: peso desnecessário, já que o jogo usa o próprio controlador e triggers.

## Deixado para o construtor

- Valores finos de juice e de tempo (os citados são pontos de partida, todos exportados).
- Forma exata da consulta de sobreposição na `SpatialGrid`.
- Síntese dos sons provisórios.
- Posição exata dos bonecos na `IceTest`.
- Organização interna do serviço `Audio` e do passe de partículas alfa.

## Decisões do Claude (após a revisão do construtor; o autor pode corrigir)

1. **Onde as hurtboxes ficam:**
   - O serviço de cena `CombatWorld` tem a própria `SpatialGrid` só de hurtboxes. Cada `Hurtbox` se registra ao ser anexada ou ativada e sai ao ser desanexada, desativada ou quando o dono é destruído.
   - A hitbox consulta essa grade, e a grade de colisão da cena não muda.
   - Hurtboxes em nós filhos funcionam, porque cada filho é um objeto com id próprio.
2. **Ordem no `Damageable`:**
   1. callback `guard` do dono (bloqueio ou aparo);
   2. invencibilidade;
   3. multiplicadores;
   4. subtrair do `Resources`;
   5. `DamageTaken`.
   - Golpe bloqueado ou aparado não dá dano nem invencibilidade. Durante a invencibilidade não há aparo, porque o golpe é ignorado antes.
   - O `IkiObject::hurt` deixa de subtrair vida e vira só a reação: empurrão, estado `hurt` ou `dead` e piscar. Ele é chamado ao receber `DamageTaken`.
   - O Iki usa a invencibilidade do próprio `Damageable` (`invulnerableTime` dele = o valor do Iki) e não tem multiplicadores (padrão 1).
3. **Aparo:**
   - Avaliado quando o `HitEvent` é processado. A janela conta a partir de cada aperto de `V`: soltar e apertar de novo reinicia o escudo e a janela.
   - Golpe com `parryable = false` vira bloqueio comum.
   - O `Stunned` vai para `info.source`, se esse objeto tiver o componente `Stunned`; se não tiver, nada acontece.
   - Num projétil aparado, o projétil é refletido **e** quem atirou fica atordoado.
4. **Projétil refletido:**
   - Passa a ter `source` = Iki e `team` = `"player"`. Por isso o `hit:dealt` e o hit stop do acerto são do Iki.
   - Mantém o dano original e não pode ser refletido de novo (`reflected = true`).
   - Prefabs: `ice_shard` (Iki) e `dummy_bolt` (atirador).
   - `ProjectilePool::spawn(prefabName, pos, dir, team, source)`; velocidade e dano vêm do prefab.
5. **Tempo durante o hit stop:**
   - Congelam: os temporizadores de jogo (combo, aparo, intervalo de tiro, invencibilidade, partículas, efeitos, flash), porque todos usam o tempo da cena. Assim o hit stop do aparo não consome a janela de aparo.
   - Continua: a leitura das teclas. Apertos durante o congelamento ficam guardados (buffer de pulo, `X` da sequência) e valem quando o tempo volta.
6. **Sequência:**
   - A duração de cada corte é `slashDuration` (0,22 s), exportada. A hitbox fica ativa entre os eventos `hit_on` e `hit_off` ou, sem eles, entre `activeStart` 0,05 e `activeEnd` 0,15 s. A animação é só visual.
   - O próximo corte começa no aperto (ou na hora, se o aperto ficou guardado durante o corte anterior).
   - `attackStep` = 0,3 u.
   - Zera a sequência: dano, parede, teto, escudo e tiro. Pousar não zera.
   - "Uma por salto" conta quando um corte **começa** no ar.
7. **Frente e trás:**
   - Para projéteis, a direção do `DamageInfo` é a da velocidade.
   - "De frente" olha só o sinal de x. Golpe puramente vertical (|x| ≈ 0) conta como de frente.
8. **Áudio:**
   - miniaudio por FetchContent com versão fixa, como as outras dependências.
   - Música e efeitos são `.wav`, sem OGG.
   - O serviço `Audio` pertence ao `Engine` e é exposto à cena como serviço.
   - A ação `sound` é global (sem posição).
9. **Gatilhos e bonecos:**
   - `hit:taken` só dispara com dano maior que 0; com multiplicador 0, dispara `hit:immune`.
   - Os bonecos têm `invulnerableTime` 0, para a sequência 2-2-4 funcionar. A hitbox já evita acerto duplo por corte.
10. **Quebra do boneco:** um componente `TrainingDummy` cuida dela. Ao zerar a vida, desativa a `Hurtbox` (sai da grade de combate) e esconde o sprite. Depois de `respawnTime`, enche a vida e reativa.
11. **Bonecos sem animação e sem empurrão:** os bonecos não têm `SpriteAnimator` até haver arte; o sprite é um frame fixo. Eles também ignoram o `knockback` do golpe e ficam parados. O atirador só atira no player vivo e na mesma sala dele, porque a sala vizinha fica carregada e alcançável pela porta.
