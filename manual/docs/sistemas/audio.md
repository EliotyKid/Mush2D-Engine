# audio

`src/engine/audio/` toca o som do jogo com a [miniaudio](https://miniaud.io) (0.11.26, baixada pelo CMake como as outras dependências). Ela faz três coisas:

- toca efeitos curtos;
- toca uma música por vez, em loop, com transição cruzada;
- guarda os volumes geral, de efeitos e de música.

Só `.wav`. Sem OGG, MP3 nem FLAC.

## O que faz

O `Engine` é dono do `Audio`. Ele chama `init()`, procura os sons em `assets/audio` e registra o serviço na cena (`scene.service<Audio>()`). O jogo também o recebe em `EngineServices::audio`.

A cada quadro, o `Engine` chama `update(dt)` com o tempo real. Por isso, o hit stop congela a cena, mas não o som: a música e os efeitos seguem tocando.

Sem dispositivo de áudio (testes, máquina sem placa de som, CI), `init()` devolve falso e avisa uma vez. Daí em diante tudo fica em silêncio e nada falha. Os nomes, a música atual e a transição continuam sendo acompanhados.

## Arquivos

| Pasta | Conteúdo |
|---|---|
| `assets/audio/sfx/<nome>.wav` | Efeitos, tocados por nome (`playSound("step")`). |
| `assets/audio/music/<nome>.wav` | Músicas, tocadas por nome (`playMusic("forest_theme")`), sempre em loop. |

Os sons de agora são provisórios. Eles são gerados por `tools/gen_placeholder_sfx.py`, com ruído, senos e envelopes. Para gerar de novo:

```
py tools/gen_placeholder_sfx.py
```

O script usa uma semente fixa: rodar de novo grava os mesmos bytes. Os `.wav` gerados ficam no git. Para trocar um som provisório por um de verdade, grave um `.wav` com o mesmo nome por cima.

Efeitos: `slash_1`, `slash_2`, `slash_3`, `hit_ice`, `hit_block`, `parry`, `shard_throw`, `shard_break`, `guard_up`, `guard_break`, `no_energy`, `step`, `land` e `jump`. Músicas: `forest_theme` e `cave_theme` (8 s cada).

## Tipos principais

| Membro | Papel |
|---|---|
| `bool init()` | Abre o dispositivo padrão. Falso sem dispositivo: fica em silêncio. |
| `scan(root)` | Procura os `.wav` em `root/sfx` e `root/music`. |
| `playSound(nome, volume = 1, tom = 1)` | Toca um efeito. O volume é multiplicado por `sfxVolume`. Tom 1 é o som como foi gravado; 2 é uma oitava acima. |
| `playMusic(nome)` | Troca para a música em `musicFade` segundos: a nova sobe enquanto a antiga desce. O mesmo nome não faz nada (a música não reinicia). `""` só desce a atual. |
| `update(dtReal)` | Avança a transição, aplica os volumes e libera os efeitos que acabaram. |
| `currentMusic()` | O nome pedido por último. |
| `musicMix()` | De 0 a 1: quanto a música atual já subiu. |
| `knows(nome)` | Se o nome existe em `sfx` ou `music`. |
| `masterVolume`, `sfxVolume`, `musicVolume` | Volumes, de 0 a 1 (1 por padrão). |
| `musicFade` | Duração da transição cruzada, em segundos (1 por padrão). |

Os quatro números ficam no [painel F1](../fazendo-o-jogo/inspetor.md), seção "Audio". Eles valem só durante a execução e não são salvos.

## Avisos

- Um nome desconhecido avisa uma vez no stderr, com o nome e o caminho esperado: `Audio: unknown sound 'pasos' (.../assets/audio/sfx/pasos.wav)`.
- Um arquivo que não abre como `.wav` (corrompido, ou outro formato com a extensão errada) avisa uma vez, com o nome e o caminho, e não toca.
- Os dois casos nunca lançam exceção.

## Casos de uso

### Um som num efeito

Use a ação `sound` num `.effect.json` ou na seção `effects` de um prefab. Veja [Game juice](../fazendo-o-jogo/game-juice.md#efeitos-reaproveitaveis):

```json
{"sound": "step", "volume": 0.5, "pitch": [0.9, 1.1]}
```

### Música de uma região

Ponha `"music": "<nome>"` no `.region.json`. Veja [Mundo e salas](../fazendo-o-jogo/mundo-e-salas.md#regioes).

### Tocar som pelo código

```cpp
#include "engine/audio/Audio.hpp"

if (Audio* audio = scene.service<Audio>()) audio->playSound("parry", 0.8f);
```
