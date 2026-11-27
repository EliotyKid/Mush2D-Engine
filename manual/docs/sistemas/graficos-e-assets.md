# graphics e assets

`src/engine/graphics/` tem a câmera e os catálogos de nomes. `src/engine/assets/` lê os atlas de sprites.

## O que faz

- A **câmera** segue um alvo com suavidade, respeita os limites da sala, faz zoom e tem três modos de pixel: pixel art nítido (escala inteira fixa), pixel art suave (zoom livre entre 1x e 8x) e HD.
- Os **atlas** são pares `.png` + `.json` no formato do Aseprite (JSON hash): frames, animações (frameTags) e pivô.
- O **`AssetRegistry`** acha sozinho todo atlas dentro de `assets/` e dá a ele o nome do arquivo (`assets/kaa/characters/kaa_animals.json` → `kaa_animals`). Prefabs e `SpriteAnimator` usam esses nomes.
- Os **catálogos** dão nomes de jogo (`GameSpriteId::Player...`) a frames e animações, para o código antigo.

## Tipos principais

| Tipo | Papel |
|---|---|
| `Camera2D` | `position`, `size` (altura visível no modo HD), `pixelArt`, `pixelSmooth`, `pixelsPerUnit` (32), `followEnabled`, `targetId`, `followOffset`, `followSmoothness`, `boundsEnabled` e limites. `updateFollow`, `clampToBounds`, `halfExtent`. Tremor: `addShake(amplitude, duração)` soma um deslocamento aleatório que decai em linha reta até 0, com limite `maxShake` (0,5 u; a câmera não é refletida, então o painel Look do F1 tem uma seção "Camera" com esse número, que não é salvo); `updateShake(dt, viewport)` avança em tempo real (o `Engine` chama a cada quadro, o hit stop não congela) e prende a vista tremida nos limites, como `clampToBounds`; `shakeOffset()` é somado à `position` só na hora de montar a vista. O `Engine` registra a câmera como serviço da cena, para a ação de efeito `shake`. |
| `AssetRegistry` | `scan(pasta, primeiros)`, `atlas(nome)`, `atlasIndex(nome)`, `resolve("atlas/frame")`. Serviço da cena. |
| `TextureAtlas` | Um atlas: `hasFrame`, `getFrame`, `hasAnimation`, `getAnimation`, `getAllFrameNames`, `getAllAnimationNames`. |
| `AtlasFrame`, `AtlasAnimation`, `AtlasAnimationFrame` | Um frame (retângulo, pivô), uma animação (frames, `looping`) e a duração de cada frame (`durationMs`). |
| `AtlasLoader` | Lê o `.json` do Aseprite para um `TextureAtlas`. |
| `SpriteCatalog<Id>`, `AnimationCatalog<Id>` | `registerSprite` / `registerAnimation` e `get(id)`: ligam um id do jogo a atlas + frame/animação. |

### Atlas atuais

Os primeiros índices são fixos (`GameAtlasId`): `characters`, `enemies`, `environment`, `kaa_player`, `kaa_animals`, `kaa_props`, `crystals`. Os outros atlas achados em `assets/` vêm depois, em ordem alfabética.

### Texturas extras

Ao lado de cada `.png`, a engine procura `<nome>_n.png` (normal map), `<nome>_e.png` (emissão) e `<nome>_m.png` (máscara). Um mapa que falta, ou com tamanho diferente, usa uma textura neutra.

## Casos de uso

### Usar uma arte nova num prefab

1. Exporte do Aseprite um atlas (`.png` + `.json`, formato hash, com as tags de animação) para dentro de `assets/`, por exemplo `assets/kaa/characters/morcego.json`.
2. No prefab: `"sprite": { "atlas": "morcego", "animation": "voo" }` e, para trocar de animação por estado, `"animations": { "idle": "voo" }`.
3. Para luz com relevo, ponha `morcego_n.png` ao lado.

Nenhum código: o `AssetRegistry` acha o atlas pelo nome do arquivo.

### Uma animação nova para código antigo

Em `GameAnimationId.hpp`, adicione o id. Em `GameAssets.cpp`, `registerAnimation(GameAnimationId::Novo, GameAtlasId::KaaPlayer, "nome_no_atlas");`.

### Câmera mais solta

Diminua `followSmoothness` (segue mais devagar) ou mude `followOffset` para mostrar mais à frente do player.

## Arquivos

`src/engine/graphics/Camera2D.*`, `SpriteCatalog.hpp`, `AnimationCatalog.hpp`; `src/engine/assets/AssetRegistry.*`, `AtlasLoader.*`, `TextureAtlas.*`, `AtlasTypes.hpp`; `examples/iki_demo/src/iki_demo/assets/` (ids e registro do jogo).
