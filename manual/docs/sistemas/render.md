# render e shaders

`src/engine/render/` e `shaders/` desenham o frame: luz com normal maps, névoa por profundidade, brilhos aditivos, bloom e cor. Você não precisa mexer aqui para fazer um jogo; o look se ajusta pelo [painel F1](../fazendo-o-jogo/inspetor.md).

## O que faz

O caminho de um frame:

1. **`SpriteBatch`** junta todos os sprites do frame numa lista: opacos primeiro, depois aditivos; cada grupo de trás para a frente (profundidade maior antes).
2. **G-buffer** (`gbuffer.frag`): desenha cor, normal map, emissão e profundidade de cada sprite.
3. **Luz** (`light.frag`): calcula quanta luz chega em cada pixel, usando o normal map. Pode rodar em meia resolução (`lightHalfRes`).
4. **Resolve** (`resolve.frag`): junta cor × luz + emissão e aplica a névoa por profundidade.
5. **Aditivo** (`additive.frag`): brilhos, como os halos dos cristais.
6. **Bloom** (`bloom_down.frag`, `bloom_up.frag`): o "brilho que vaza", em 5 níveis (1/2 a 1/32 da tela).
7. **Composição** (`composite.frag`): bloom, exposição, temperatura, tonemap AgX, lift/gamma/gain, saturação e sRGB. Depois o painel F1 por cima.

Cada passada tem um nome nos números de [Desempenho](../desempenho.md): `gbuffer`, `light`, `resolve`, `additive`, `bloom`, `composite`.

## Tipos principais

| Tipo | Papel |
|---|---|
| `SceneRenderer` | Comanda as passadas acima a cada frame. |
| `FrameSettings` | O look: luz ambiente (cor, intensidade), névoa (cor, densidade, gradiente), exposição, bloom (limiar, joelho, intensidade), grading (lift, gamma, gain, saturação, temperatura), `pixelMode`, `pixelateLight`, `pixelateFactor`, `lightHalfRes`, `fade` (escurecer na morte). É o que o `.region.json` guarda. |
| `GpuLight` | Uma luz: `posHeightRadius` (x, y, altura, raio), `colorIntensity` (r, g, b, intensidade), `depthFlicker` (profundidade, amplitude da tremulação, velocidade, 0). |
| `LightList` | As luzes do frame. |
| `LightComponent` | Uma luz presa a um objeto (`light` é um `GpuLight`, com x, y relativos ao dono). Ainda não exportada para prefabs. |
| `SpriteBatch`, `SpriteInstance` | A lista de sprites do frame. A cor de cada sprite é `color * tint`. O campo `flash` (cor e quantidade num `uint`, `packUnorm4x8`) mistura o sprite para essa cor sem luz: o `gbuffer.frag` tira a cor iluminada e põe o flash na emissão, e o `additive.frag` mistura direto. |
| `Bindless`, `MaterialLibrary` | Carregam as texturas: `.png` mais `_n` (normal), `_e` (emissão), `_m` (máscara). |
| `RenderTargets` | As imagens intermediárias (G-buffer, HDR, luz, bloom). |
| `PixelScale` | Pixel art: maior escala inteira que ainda mostra pelo menos 400 pixels de arte na largura. |
| `GpuTimers` | Mede o tempo de GPU de cada passada (painel F1 e `MUSH2D_PERF_LOG`). |

## Casos de uso

### Um objeto que brilha e ilumina

Como o cristal: sprite com `_e.png` (emissão), um filho com `additive` ligado e um `LightComponent`:

```cpp
LightComponent* light = getComponent<LightComponent>();
if (!light) light = &addComponent<LightComponent>();
// x, y, altura, raio | cor, intensidade | profundidade, amplitude, velocidade, 0
light->light = GpuLight{{0.0f, 0.0f, 0.6f, 3.0f}, {cor, 1.4f}, {0.0f, 0.06f, 2.0f, 0.0f}};
```

### Uma tocha que tremula

Entidade `Light` no LDtk com `flicker` > 0 (veja [Objetos do jogo](../objetos.md#light)).

### Testar o bloom

Rode com `MUSH2D_DEBUG_ADDITIVE=1`: aparece um sprite aditivo brilhante ao lado do player. Mexa em "Bloom e exposicao" no painel F1.

### Ganhar desempenho numa GPU fraca

Ligue "luz em meia resolucao" no painel F1 e salve a região.

## Arquivos

`src/engine/render/`: `SceneRenderer.*`, `SpriteBatch.*`, `Materials.*`, `Bindless.*`, `RenderTargets.*`, `LightComponent.*`, `PixelScale.hpp`, `GpuTimers.*`. `shaders/`: `sprite.vert`, `fullscreen.vert`, `gbuffer.frag`, `light.frag`, `resolve.frag`, `additive.frag`, `bloom_down.frag`, `bloom_up.frag`, `composite.frag`, `common.glsl`, `sprite_sample.glsl`.
