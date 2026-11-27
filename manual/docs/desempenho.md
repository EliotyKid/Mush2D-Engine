# Desempenho

## Números atuais

Release, `MUSH2D_PERF_LOG`, 30 s parado na sala vitrine. Média por passada, em ms de GPU. O vsync prende o fps ao monitor.

| Máquina | gbuffer | light | resolve | alpha | additive | bloom | composite | soma | frame | fps |
|---|---|---|---|---|---|---|---|---|---|---|
| Intel UHD, 1280×720, luz em resolução cheia | 3,16 | 1,66 | 1,30 | — | 0,09 | 0,56 | 0,41 | 7,19 | 16,67 ms | 60 |
| Intel UHD, 1280×720, luz em meia resolução (`lightHalfRes`) | 3,49 | 0,60 | 0,89 | — | 0,11 | 0,65 | 0,48 | 6,23 | 16,67 ms | 60 |
| RTX 2060 Super (Arch), 1920×1080 | | | | | | | | a medir | | |

"—": medido antes do passe `alpha` (partículas não aditivas, D3). Esse passe só desenha quando há partículas vivas; ainda não medido.

Orçamento de 60 fps: 16,6 ms. Nos primeiros segundos o fps oscila (~49-59) enquanto a GPU integrada sobe o clock; depois fica em 60. Meta no PC Arch: soma < 3 ms.

Cada coluna é uma passada do renderer. Veja [render e shaders](sistemas/render.md) para o que cada uma faz.

## Como medir

1. Compile em Release: `cmake --preset release` e `cmake --build build/release`.
2. Rode com o log ligado por 30 segundos e não mexa no player:

    ```powershell
    $env:MUSH2D_PERF_LOG = "30"; build/release/Mush2D.exe; Remove-Item Env:MUSH2D_PERF_LOG
    ```

3. Leia no terminal os ms por passada a cada segundo e as linhas `room <nome> load_ms=` com o tempo de CPU de cada carga de sala.
4. Ignore os primeiros segundos, enquanto a GPU sobe o clock.

!!! tip "Luz em meia resolução"
    A caixa "luz em meia resolucao" do painel F1 (`lightHalfRes` no look) calcula a luz em meia resolução. Na Intel UHD ele cortou a passada `light` de 1,66 para 0,60 ms.
