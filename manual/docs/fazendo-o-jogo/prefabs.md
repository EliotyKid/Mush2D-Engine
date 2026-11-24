# Prefabs

Um **prefab** é um arquivo `assets/prefabs/<nome>.prefab.json` que descreve um objeto: a classe C++, os atributos, o sprite, o collider, as animações, os componentes e os filhos. Ele pode herdar de outro com `"base"`.

## Como o jogo escolhe o prefab

Cada entidade do LDtk cria o prefab com o nome dela em `snake_case` (`Slime` → `slime.prefab.json`, `SlimeFast` → `slime_fast.prefab.json`). Um campo `prefab` na entidade escolhe outro prefab.

Um campo da entidade com o nome de um atributo sobrescreve o valor do prefab. Os outros campos vão para `extra()` do objeto.

Só `PlayerStart`, `Light` e `Terrain` têm fábrica em código.

## Formato

```json
{ "version": 1, "base": "enemy_base", "type": "Slime",
  "props": { "speed": 2.5 }, "extra": { "lootChance": 0.3 },
  "sprite": { "atlas": "kaa_animals", "frame": "fox_idle 0", "animation": "fox_idle", "layer": 1,
              "color": [1, 1, 1, 1], "depth": 0, "additive": false },
  "collider": { "size": [0.8, 0.8], "offset": [0, 0], "isTrigger": false },
  "animations": { "idle": "slime_idle", "walk": "slime_walk" },
  "components": [ { "type": "LightComponent", "props": { } } ],
  "children": [ { "name": "Glow", "prefab": "glow_small", "position": [0, 0.5] },
                { "name": "Hitbox", "type": "GameObject", "collider": { "isTrigger": true } } ] }
```

| Chave | O que é |
|---|---|
| `version` | sempre `1` |
| `type` | a classe C++ registrada com `MUSH2D_OBJECT` |
| `base` | prefab do qual este herda |
| `props` | atributos `MUSH2D_EXPORT` da classe |
| `extra` | dados livres que a classe lê de `extra()` |
| `sprite` | atlas, frame, animação, camada, cor, profundidade, aditivo |
| `collider` | tamanho, offset, trigger |
| `animations` | nome do estado → animação do atlas (vira um `SpriteAnimator`) |
| `components` | componentes com os seus `props` |
| `children` | objetos filhos: outro prefab ou um objeto inline |

## Herança

`props`, `extra`, `sprite`, `collider` e `animations` são mesclados chave a chave sobre o `base`. `components` são mesclados por `type`, e `children` por `name`. `"remove": true` num item tira o item herdado.

Um ciclo de `base` é recusado com aviso. Um filho cujo prefab é um dos prefabs em que ele está também é recusado.

## Ordem dos valores

O último vence: construtor → `onCreate` → prefab (`base` primeiro) → campos do LDtk → `onPrefabApplied` → edições ao vivo. Só as chaves presentes no JSON são aplicadas.

## Erros

Problemas nunca param o jogo. Um atributo desconhecido, um tipo errado ou um `base` que não existe vira um aviso (arquivo e campo) no stderr, e o atributo fica com o valor do C++.

## Prefabs atuais

`slime`, `slime_fast` (herda do slime: mais rápido, avermelhado), `door`, `lever`, `item`, `checkpoint`, `critter`, `prop`, `crystal` e `crystal_halo` (filho aditivo do cristal). Veja [Objetos do jogo](../objetos.md).

No Debug, `assets/prefabs/prefab_schema.json` lista todas as classes e atributos. Salvar um prefab recarrega as salas, e o estado do mundo fica.

## Casos de uso

### Uma variação de um objeto, sem recompilar

O `slime_fast.prefab.json` é um slime mais rápido e avermelhado:

```json
{ "version": 1, "base": "slime",
  "props": { "moveSpeed": 3.0, "patrolDistance": 3.0 },
  "sprite": { "color": [1.0, 0.55, 0.55, 1.0] } }
```

Para usar, crie a entidade `SlimeFast` no LDtk, ou ponha o campo `prefab` = `slime_fast` numa entidade `Slime`.

### Mudar um valor só numa instância

Na entidade do LDtk, crie um campo com o nome do atributo (por exemplo `moveSpeed`, tipo Float). Só essa instância muda.

### Um objeto com um filho brilhante

O `crystal` tem um filho `Halo` que usa o prefab `crystal_halo`, um sprite aditivo maior:

```json
{ "version": 1, "type": "CrystalObject",
  "sprite": { "atlas": "crystals", "depth": 0.1 },
  "children": [ { "name": "Halo", "prefab": "crystal_halo" } ] }
```

### Criar o prefab pelo jogo

Ajuste os valores ao vivo no [inspetor](inspetor.md) e use "Salvar como novo prefab".
