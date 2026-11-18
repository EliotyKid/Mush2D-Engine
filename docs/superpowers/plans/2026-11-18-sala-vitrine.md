# Sala Vitrine — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transformar o Mush2D numa engine Vulkan 1.3 com SpriteBatch bindless, iluminação deferred 2D (normal map, emissão, névoa por profundidade), bloom/HDR, níveis LDtk e painel ImGui, provada numa sala vitrine de metroidvania.

**Architecture:** `src/engine/` vira a biblioteca estática `mush2d_engine`, sem nenhum include de `src/game/`. O jogo implementa a interface `IGame`. O renderer desenha todos os sprites opacos num G-buffer com uma única draw call instanciada. Depois vêm as passadas em tela cheia: luz e névoa, forward aditivo, bloom e composição. Todos os render targets também são registrados no array bindless, então as passadas de pós só recebem índices por push constant.

**Tech Stack:** C++20, CMake ≥ 3.24 (FetchContent), Vulkan 1.3, vk-bootstrap, VMA, GLFW, glm, nlohmann_json, Dear ImGui, stb_image, glslc, Python 3 + Pillow (só para gerar a arte provisória).

**Spec:** [docs/superpowers/specs/2026-11-18-sala-vitrine-design.md](../specs/2026-11-18-sala-vitrine-design.md)

## Global Constraints

- C++20. CMake ≥ 3.24. Vulkan 1.3 com `dynamicRendering`, `synchronization2` e descriptor indexing (`descriptorBindingPartiallyBound`, `descriptorBindingSampledImageUpdateAfterBind`, `runtimeDescriptorArray`, `shaderSampledImageArrayNonUniformIndexing`).
- Dependências por FetchContent com tag fixa. A única instalação manual é o Vulkan SDK e o compilador. `stb_image.h` continua em `external/`.
- `src/engine/**` nunca inclui `src/game/**`.
- Nada de `VkRenderPass`, `VkFramebuffer` nem `vkAllocateMemory` direto (usar VMA).
- 2 frames em voo. Validation layers ligadas em Debug, com zero erros.
- Array bindless com capacidade 1024.
- Texturas neutras:
  - normal (128,128,255,255);
  - emissão (0,0,0,255);
  - máscara (0,255,0,255): R = rim, G = AO.
- Formatos do G-buffer:
  - albedo `R8G8B8A8_SRGB`;
  - normal `R8G8B8A8_UNORM` (RG = normal, B = rim, A = AO);
  - emissão `R16G16B16A16_SFLOAT`;
  - profundidade `R16_SFLOAT`.
- HDR `R16G16B16A16_SFLOAT`.
- Recorte por alpha no G-buffer: `discard` se alpha < 0.5.
- Pixel art: escala inteira `N = max(1, floor(larguraTela / 400))`, sampler NEAREST e snap a 1/`pixelsPerUnit` (32).
- Até 64 luzes. Bloom com 5 níveis, começando em meia resolução.
- Assets resolvidos a partir do diretório do executável. Em Debug, o `.look.json` é salvo em `MUSH2D_SOURCE_DIR/assets/levels/`.
- Gameplay só em C++.
- Testes: executável `mush2d_tests` (asserts, sem framework), registrado no CTest.

## Review Focus

1. **Janela minimizada (extent 0×0):** deve pausar o render sem crash nem erro de validação. Teste manual na Task 2, passo 4.
2. **Atlas sem `_n/_e/_m`, ou com tamanho diferente do atlas:** sem os mapas usa os neutros; com tamanho diferente, loga um aviso e usa o neutro. Teste `material_mismatched_size_falls_back` na Task 4.
3. **`.look.json` ausente, corrompido, ou referenciando `iid`/camada que não existe mais:** o jogo abre com os valores do LDtk, loga um aviso e ignora as entradas órfãs. Teste `look_ignores_unknown_and_corrupt` na Task 9.
4. **Nível LDtk com camada `AutoLayer`, tileset ausente ou entidade de tipo desconhecido:** a camada é carregada (`autoLayerTiles`), a camada sem tileset é pulada com aviso e a entidade desconhecida é ignorada com aviso. Teste `ldtk_tolerates_unknowns` na Task 7.
5. **Mais de 64 luzes, ou mais instâncias que a capacidade do buffer:** a luz excedente é descartada com um aviso por frame-limite (sem spam). O buffer de instâncias cresce, realocando no frame seguinte. Teste `light_list_clamps_to_64` na Task 5.

---

## Estrutura de arquivos

```
CMakeLists.txt                 lib mush2d_engine + exe Mush2DEngine + exe mush2d_tests
CMakePresets.json              presets debug / release (Ninja)
.gitignore
cmake/Dependencies.cmake       FetchContent de tudo
cmake/Shaders.cmake            glslc -> build/shaders/*.spv
src/main.cpp                   cria Engine + ShowcaseGame
src/engine/core/Engine.hpp/.cpp        loop, tempo, IGame
src/engine/core/IGame.hpp
src/engine/core/Paths.hpp/.cpp         assetPath(), sourceAssetPath()
src/engine/gpu/GpuContext.hpp/.cpp     vk-bootstrap, VMA, filas, frames em voo, upload imediato
src/engine/gpu/Swapchain.hpp/.cpp
src/engine/gpu/Image.hpp/.cpp          imagem + view VMA, transições (barrier2)
src/engine/render/Bindless.hpp/.cpp    array 1024 de texture2D + 2 samplers
src/engine/render/Materials.hpp/.cpp   resolução _n/_e/_m, carregamento de texturas
src/engine/render/SpriteBatch.hpp/.cpp instâncias por frame
src/engine/render/RenderTargets.hpp/.cpp  G-buffer, HDR, luz (meia res), cadeia de bloom
src/engine/render/SceneRenderer.hpp/.cpp  passes, FrameSettings, LightList
src/engine/render/GpuTimers.hpp/.cpp
src/engine/render/PixelScale.hpp       computePixelScale()
src/engine/world/Ldtk.hpp/.cpp         parser + mergeIntGrid()
src/engine/world/LevelLoader.hpp/.cpp  Ldtk -> Scene2D/SpriteBatch/luzes, fábricas
src/engine/tools/LookFile.hpp/.cpp     .look.json load/save/apply
src/engine/tools/LookPanel.hpp/.cpp    ImGui
src/game/ShowcaseGame.hpp/.cpp         IGame do metroidvania (absorve a lógica de jogo de EngineApp)
shaders/sprite.vert, gbuffer.frag, fullscreen.vert, light.frag,
        additive.frag, bloom_down.frag, bloom_up.frag, composite.frag
tests/tests.cpp
tools/gen_placeholders.py
assets/tilesets/, assets/parallax/, assets/levels/showcase.ldtk
```

Removidos: `src/app/EngineApp.*`, `src/engine/graphics/Renderer2D.*`, `shaders/triangle.*`, `build/` (do git).

---

### Task 1: Fundação do build

**Files:**
- Create: `.gitignore`, `CMakePresets.json`, `cmake/Dependencies.cmake`, `cmake/Shaders.cmake`, `src/engine/core/Paths.hpp/.cpp`, `tests/tests.cpp`
- Modify: `CMakeLists.txt`, `README.md`
- Remove from git: `build/`, `shaders/*.spv`

**Interfaces:**
- Produces:
  - `std::filesystem::path assetPath(std::string_view relative)`: retorna `<dir do exe>/relative`.
  - `std::filesystem::path sourceAssetPath(std::string_view relative)`: retorna `MUSH2D_SOURCE_DIR/relative` em Debug e `assetPath` em Release.
  - Alvos CMake `mush2d_engine` (STATIC), `Mush2DEngine` e `mush2d_tests`.
  - Macro de teste `CHECK(expr)` e `int main()` em `tests/tests.cpp`, que roda funções registradas com `TEST(name)`.

- [ ] **Step 1:** `.gitignore` com `build/`, `out/`, `*.spv`, `.vs/`, `.vscode/`, `.idea/`. Rodar `git rm -r --cached build shaders/*.spv`.
- [ ] **Step 2:** `cmake/Dependencies.cmake`: FetchContent de `vk-bootstrap`, `VulkanMemoryAllocator`, `glfw` (com `GLFW_BUILD_DOCS/TESTS/EXAMPLES OFF`), `glm`, `nlohmann_json` e `imgui` (sem CMake próprio: criar alvo `imgui` com `imgui*.cpp` + `backends/imgui_impl_glfw.cpp` + `backends/imgui_impl_vulkan.cpp`). Cada um com `GIT_TAG` de release fixa e `GIT_SHALLOW TRUE`. Trocar `find_package(glfw3/nlohmann_json)` por esses alvos. Manter `find_package(Vulkan REQUIRED COMPONENTS glslc)`.
- [ ] **Step 3:** `cmake/Shaders.cmake`: função `mush2d_compile_shaders(target)` que compila cada `shaders/*.{vert,frag}` com `Vulkan::glslc --target-env=vulkan1.3` para `${CMAKE_BINARY_DIR}/shaders/<nome>.spv`, como dependência do alvo. Pós-build: copiar `assets/` para o diretório do exe.
- [ ] **Step 4:** `CMakeLists.txt`: `mush2d_engine` com `src/engine/**` e `target_compile_definitions(... MUSH2D_SOURCE_DIR="${CMAKE_SOURCE_DIR}")` em Debug. Um único include root: `src/`. Os includes viram `"engine/..."` e `"game/..."`; remover a lista de diretórios atual. `Mush2DEngine` = `src/main.cpp` + `src/game/**`. `enable_testing()` + `add_test(mush2d_tests ...)`.
- [ ] **Step 5:** Escrever `tests/tests.cpp` com o primeiro teste:

```cpp
TEST(asset_path_is_next_to_executable) {
    auto p = assetPath("assets/levels/x.ldtk");
    CHECK(p.is_absolute());
    CHECK(p.filename() == "x.ldtk");
}
```

- [ ] **Step 6:** `CMakePresets.json` com presets `debug` e `release` (Ninja, `binaryDir: build/${presetName}`). Rodar `cmake --preset debug && cmake --build build/debug && ctest --test-dir build/debug`. Esperado: build ok e 1 teste PASS. Nesta task o jogo ainda usa o código antigo e deve continuar abrindo. Ajustar `readFile`/`createTextureResource` para usar `assetPath`.
- [ ] **Step 7:** `README.md`: pré-requisitos (Arch: `pacman -S vulkan-devel cmake ninja gcc`; Windows: Vulkan SDK, Visual Studio 2022 ou MSVC Build Tools, CMake, Ninja) e os 3 comandos do passo 6. No notebook, rodar `vulkaninfo --summary` e confirmar `apiVersion` ≥ 1.3. Se for menor, **parar e avisar o autor**.
- [ ] **Step 8:** Commit `build: FetchContent deps, presets, shader compile, tests target`.

### Task 2: Contexto GPU

**Files:**
- Create: `src/engine/gpu/GpuContext.*`, `src/engine/gpu/Swapchain.*`, `src/engine/gpu/Image.*`
- Modify: `src/app/EngineApp.*` (passa a usar GpuContext; removido na Task 3)

**Interfaces:**
- Produces:
  - `class GpuContext { void init(GLFWwindow*); void shutdown(); VkDevice device(); VmaAllocator allocator(); VkQueue graphicsQueue(); uint32_t graphicsFamily(); FrameContext& beginFrame(); /* retorna nullptr-equivalente se a swapchain foi recriada ou o extent é 0 */ void endFrame(); void immediateSubmit(std::function<void(VkCommandBuffer)>); Swapchain& swapchain(); uint32_t frameIndex(); }`
  - `struct FrameContext { VkCommandBuffer cmd; VkFence fence; VkSemaphore acquire; VkSemaphore renderDone; uint32_t imageIndex; }`
  - `beginFrame` retorna `FrameContext*`; `nullptr` = pular o frame.
  - `struct GpuImage { VkImage image; VkImageView view; VmaAllocation alloc; VkFormat format; VkExtent2D extent; }`, mais `GpuImage createImage(GpuContext&, VkExtent2D, VkFormat, VkImageUsageFlags)`, `void destroyImage(GpuContext&, GpuImage&)` e `void transition(VkCommandBuffer, VkImage, VkImageLayout from, VkImageLayout to)` (barrier2, máscaras ALL_COMMANDS conservadoras).
  - Callback `std::function<void(VkExtent2D)> onResize` em `GpuContext`, para recriar os render targets.

- [ ] **Step 1:** vk-bootstrap: instance 1.3 (`request_validation_layers` em Debug, `use_default_debug_messenger`), device com as features de 1.3 e de descriptor indexing listadas em Global Constraints. VMA com `vulkanApiVersion = VK_API_VERSION_1_3`.
- [ ] **Step 2:** Swapchain via `vkb::SwapchainBuilder`: formato `B8G8R8A8_UNORM` (a composição grava valores já em sRGB), FIFO. Uso: `COLOR_ATTACHMENT | TRANSFER_DST`. Um semáforo `renderDone` **por imagem da swapchain**, não por frame.
- [ ] **Step 3:** `beginFrame`/`endFrame`: espera a fence, adquire a imagem, trata `OUT_OF_DATE`/`SUBOPTIMAL` recriando a swapchain e chamando `onResize`. Com extent 0×0 (minimizada), `glfwWaitEvents` e retorna nullptr. Submissão com `vkQueueSubmit2`.
- [ ] **Step 4:** Portar o `EngineApp` atual para o GpuContext. O render antigo continua, adaptado com `vkCmdBeginRendering` no lugar do render pass, ou simplesmente um clear temporário, se o Renderer2D antigo atrapalhar (ele é removido na Task 5). Verificação manual: abrir, redimensionar, minimizar e restaurar, fechar → zero mensagens de validação.
- [ ] **Step 5:** Commit `gpu: vk-bootstrap + VMA context, 2 frames in flight, swapchain resize`.

### Task 3: Separação engine × jogo

**Files:**
- Create: `src/engine/core/IGame.hpp`, `src/engine/core/Engine.*`, `src/game/ShowcaseGame.*`
- Modify: `src/main.cpp`, `src/engine/graphics/Renderer2D.cpp` (remover include de `game/assets/GameAssets.hpp`), `src/engine/world/Scene2D.hpp` (`GameAssets*` → ponteiro opaco `void* userAssets`, ou remover se só o jogo usa)
- Delete: `src/app/EngineApp.*`

**Interfaces:**
- Produces:

```cpp
struct EngineServices { GpuContext& gpu; SceneRenderer& renderer; GlfwInput& input; Camera2D& camera; Scene2D& scene; };
class IGame {
public:
    virtual ~IGame() = default;
    virtual void init(EngineServices&) = 0;
    virtual void update(EngineServices&, float dt) = 0;
    virtual void onImGui(EngineServices&) {}
    virtual void shutdown(EngineServices&) {}
};
class Engine { public: void run(IGame& game); };
```

- `SceneRenderer` só passa a existir na Task 5. Até lá, `EngineServices` guarda o renderer antigo.
- [ ] **Step 1:** Mover para `ShowcaseGame` a lógica de jogo de `EngineApp`: troca/recarga de cena, gatilhos (`processTriggerOverlaps`), player, input global de câmera e `GameAssets`. Mover também o carregamento de atlas, que deixa de ser hardcoded no engine e passa a ser feito pelo jogo via API do renderer.
- [ ] **Step 2:** Verificação: `grep -rn '"game/' src/engine` não retorna nada. Build ok. As cenas Sandbox e Test rodam como antes (N troca de cena e R recarrega, como hoje).
- [ ] **Step 3:** Commit `refactor: split engine from game behind IGame`.

### Task 4: Bindless e materiais

**Files:**
- Create: `src/engine/render/Bindless.*`, `src/engine/render/Materials.*`
- Test: `tests/tests.cpp`

**Interfaces:**
- Consumes: `GpuContext`, `GpuImage`.
- Produces:
  - `class Bindless { void init(GpuContext&); uint32_t add(VkImageView view); void set(uint32_t index, VkImageView view); VkDescriptorSetLayout layout(); VkDescriptorSet set(); }`
    - binding 0: `texture2D[1024]` (`PARTIALLY_BOUND | UPDATE_AFTER_BIND`);
    - binding 1: `sampler[2]` (0 = NEAREST, 1 = LINEAR, clamp-to-edge).
  - `struct Material { uint32_t albedo, normal, emissive, mask; }`.
  - `struct MaterialPaths { std::filesystem::path albedo; std::optional<std::filesystem::path> normal, emissive, mask; }`.
  - `MaterialPaths resolveMaterialPaths(const std::filesystem::path& albedo)`: procura `<stem>_n.png`, `<stem>_e.png` e `<stem>_m.png` ao lado do albedo.
  - `class MaterialLibrary { void init(GpuContext&, Bindless&); /* cria os 3 neutros 1x1 */ Material load(const std::filesystem::path& albedoPng); uint32_t loadTexture(const std::filesystem::path&, bool srgb); }`: albedo em `R8G8B8A8_SRGB`, os demais em `UNORM`.
  - Função pura testável `bool mapMatchesAlbedo(int aw, int ah, int mw, int mh)`.

- [ ] **Step 1: Testes que falham**

```cpp
TEST(material_paths_found_when_present) {
    auto dir = std::filesystem::temp_directory_path() / "mush2d_mat";
    std::filesystem::create_directories(dir);
    for (auto n : {"a.png", "a_n.png", "a_e.png"}) std::ofstream(dir / n) << "x";
    auto m = resolveMaterialPaths(dir / "a.png");
    CHECK(m.normal && m.normal->filename() == "a_n.png");
    CHECK(m.emissive && m.emissive->filename() == "a_e.png");
    CHECK(!m.mask);
}
TEST(material_mismatched_size_falls_back) {
    CHECK(mapMatchesAlbedo(256, 128, 256, 128));
    CHECK(!mapMatchesAlbedo(256, 128, 128, 128));
}
```

- [ ] **Step 2:** `ctest --test-dir build/debug` → FAIL (símbolos indefinidos).
- [ ] **Step 3:** Implementar. O upload usa um staging buffer VMA + `immediateSubmit` + `vkCmdCopyBufferToImage`. Mapa com tamanho diferente do albedo → log de aviso e índice neutro.
- [ ] **Step 4:** `ctest` → PASS.
- [ ] **Step 5:** Commit `render: bindless texture array and material maps`.

### Task 5: SpriteBatch, G-buffer, luz e névoa

**Files:**
- Create: `src/engine/render/SpriteBatch.*`, `RenderTargets.*`, `SceneRenderer.*`, `PixelScale.hpp`, `shaders/sprite.vert`, `shaders/gbuffer.frag`, `shaders/fullscreen.vert`, `shaders/light.frag`, `shaders/composite.frag` (versão inicial: só tonemap)
- Modify: `src/engine/graphics/Camera2D.*`, `src/engine/world/ObjectData.hpp`
- Delete: `src/engine/graphics/Renderer2D.*`, `shaders/triangle.*`
- Test: `tests/tests.cpp`

**Interfaces:**
- Consumes: `Bindless`, `MaterialLibrary`, `Material`, `GpuContext`.
- Produces:

```cpp
// std430, 96 bytes. Mesma ordem no GLSL.
struct SpriteInstance {
    glm::vec2 position; glm::vec2 size;      // size.x < 0 = espelhado
    glm::vec4 uvRect;   glm::vec4 color;
    float rotation; float depth; glm::vec2 parallax;  // parallax 1 = camada de jogo
    uint32_t albedo, normal, emissive, mask;
    uint32_t flags; uint32_t _pad[3];        // bit0 NEAREST, bit1 ADDITIVE
};
struct GpuLight {                             // std430, 48 bytes
    glm::vec4 posHeightRadius;                // x, y, altura, raio
    glm::vec4 colorIntensity;
    glm::vec4 depthFlicker;                   // profundidade, amplitude, velocidade, 0
};
struct FrameSettings {
    glm::vec3 ambientColor{0.25f, 0.3f, 0.45f}; float ambientIntensity = 1.0f;
    glm::vec3 fogColor{0.55f, 0.65f, 0.8f};     float fogDensity = 0.08f;
    float fogGradientStart = 0, fogGradientEnd = 10, fogGradientStrength = 0;
    float exposure = 1.0f;
    float bloomThreshold = 1.0f, bloomKnee = 0.5f, bloomIntensity = 0.6f;
    glm::vec3 lift{0}, gamma{1}, gain{1}; float saturation = 1, temperature = 0;
    bool pixelArt = true; bool pixelateLight = false; int pixelateFactor = 2;
    bool lightHalfRes = false;
};
int computePixelScale(int viewportWidth);     // max(1, viewportWidth / 400)
class SpriteBatch { void clear(); void add(const SpriteInstance&); /* ordena por (depth desc, order) */ };
class LightList { void clear(); bool add(const GpuLight&); /* false acima de 64 */ size_t size() const; };
class SceneRenderer {
    void init(GpuContext&, Bindless&); void resize(VkExtent2D);
    SpriteBatch& sprites(); LightList& lights(); FrameSettings& settings();
    void render(FrameContext&, const Camera2D&, float time);
};
```

- `SpriteData` ganha `float depth = 0` e `glm::vec2 parallax{1, 1}`. Antes de renderizar, `Scene2D` é convertida em instâncias por uma função `appendSceneSprites(const Scene2D&, SpriteBatch&, const AtlasLookup&)`, que substitui `Renderer2D::renderScene`. `AtlasLookup` = `std::function<std::pair<Material, glm::vec4 uvRect>(uint32_t atlasIndex, const std::string& frame)>`, fornecido pelo jogo.
- `Camera2D`: em pixel art, a meia-altura ortográfica = `viewportHeight / (2 * computePixelScale(w) * 32)`. O `size` livre só vale no modo HD.

- [ ] **Step 1: Testes que falham**

```cpp
TEST(pixel_scale_integer) {
    CHECK(computePixelScale(1280) == 3);
    CHECK(computePixelScale(1920) == 4);
    CHECK(computePixelScale(800) == 2);
    CHECK(computePixelScale(300) == 1);
}
TEST(light_list_clamps_to_64) {
    LightList l; for (int i = 0; i < 64; ++i) CHECK(l.add({}));
    CHECK(!l.add({})); CHECK(l.size() == 64);
}
TEST(sprite_instance_layout) { CHECK(sizeof(SpriteInstance) == 96); CHECK(sizeof(GpuLight) == 48); }
```

- [ ] **Step 2:** `ctest` → FAIL.
- [ ] **Step 3: Recursos.**
  - **Buffers por frame:** buffers de armazenamento de instâncias e de luzes, host-visible via VMA. O de instâncias cresce ×2 quando estoura, recriado no próximo uso daquele frame. Mais um UBO `FrameUniforms {mat4 viewProj; vec2 cameraPos; vec2 viewport; float pixelsPerUnit; float time; uint lightCount; uint pixelArt; + campos de FrameSettings}`. Todos no set 1.
  - **Bindless:** set 0 = `Bindless`.
  - **Render targets:** criados por `RenderTargets` (os formatos de Global Constraints, mais luz em meia resolução quando `lightHalfRes || pixelateLight`, com tamanho `extent / (pixelateLight ? pixelateFactor : 2)`). Cada um é registrado no Bindless e re-registrado com `Bindless::set` no resize.
- [ ] **Step 4: `sprite.vert`.** Quad gerado por `gl_VertexIndex` (6 vértices, sem vertex buffer), instância via `gl_InstanceIndex + pushConstant.firstInstance`. Algoritmo de posição:

```glsl
vec2 local = (corner - 0.5) * abs(inst.size);
local = rot(inst.rotation) * local;
vec2 rel = inst.position - u.cameraPos * inst.parallax;  // parallax: offset da câmera por camada
if (u.pixelArt != 0) rel = round(rel * u.pixelsPerUnit) / u.pixelsPerUnit;
gl_Position = u.viewProj * vec4(rel + local, 0, 1);       // viewProj sem translação de câmera
```

- [ ] **Step 5: `gbuffer.frag`.**
  - albedo × cor; `discard` se alpha < 0.5;
  - normal = `texture(normal)*2-1`, com `x` invertido se `size.x < 0`; grava `vec4(n.xy*0.5+0.5, mask.r, mask.g)`;
  - emissão; profundidade = `inst.depth`;
  - o sampler vem de `flags & 1`; índices com `nonuniformEXT`;
  - pipeline com dynamic rendering, 4 color attachments, sem blend.
- [ ] **Step 6: `light.frag`** (tela cheia, grava HDR ou o target reduzido). Algoritmo (fixado pelo spec):

```glsl
vec3 albedo = tex(gAlbedo).rgb; vec4 nm = tex(gNormal); float depth = tex(gDepth).r;
vec3 N = normalize(vec3(nm.xy * 2 - 1, 1));
vec3 lit = u.ambientColor * u.ambientIntensity * nm.a;              // AO
for (uint i = 0; i < u.lightCount; ++i) {
    GpuLight L = lights[i];
    vec3 toL = vec3(L.posHeightRadius.xy - worldPos, L.posHeightRadius.z + abs(L.depthFlicker.x - depth));
    float d = length(toL.xy); float att = clamp(1 - d / L.posHeightRadius.w, 0, 1); att *= att;
    float flick = 1 + L.depthFlicker.y * sin(u.time * L.depthFlicker.z + float(i) * 1.7);
    vec3 Ldir = normalize(toL);
    float diff = max(dot(N, Ldir), 0);
    float rim = max(dot(N, vec3(-Ldir.xy, 0)), 0) * nm.b;           // luz por trás
    lit += L.colorIntensity.rgb * L.colorIntensity.a * att * flick * (diff + rim);
}
vec3 color = albedo * lit + tex(gEmissive).rgb;
float fd = max(depth, 0);
float fog = 1 - exp(-u.fogDensity * fd);
fog = clamp(fog + u.fogGradientStrength * smoothstep(u.fogGradientEnd, u.fogGradientStart, worldPos.y), 0, 1);
out = vec4(mix(color, u.fogColor, fog), 1);
```

`worldPos` é reconstruído a partir do UV da tela, da câmera e do parallax 1. Isso é aproximado nas camadas de fundo, o que é aceitável. Pixel sem albedo (alpha 0 no G-buffer) → cor de céu = `u.fogColor`.
- [ ] **Step 7: `composite.frag`** (versão inicial): amostra a luz (NEAREST se `pixelateLight`, senão LINEAR), multiplica por `exposure`, aplica tonemap AgX e grava na swapchain. ImGui ainda não entra.
- [ ] **Step 8:** Remover `Renderer2D`. `ShowcaseGame` passa a preencher `sprites()` via `appendSceneSprites` e adiciona uma luz de teste. `ctest` → PASS. Verificação manual: as cenas Sandbox e Test aparecem iluminadas pela luz ambiente e pela de teste, sem validação. Com o normal neutro, o resultado é igual ao albedo × luz.
- [ ] **Step 9:** Commit `render: bindless sprite batch, g-buffer, deferred 2D lighting and fog`.

### Task 6: Forward aditivo, bloom e composição final

**Files:**
- Create: `shaders/additive.frag`, `shaders/bloom_down.frag`, `shaders/bloom_up.frag`, `src/engine/render/GpuTimers.*`
- Modify: `SceneRenderer.*`, `RenderTargets.*`, `shaders/composite.frag`

**Interfaces:**
- Produces: `class GpuTimers { void init(GpuContext&); void begin(VkCommandBuffer, const char* pass); void end(VkCommandBuffer); std::vector<std::pair<std::string, float>> lastFrameMs() const; }`, usado pelo painel na Task 9. As passadas se chamam `gbuffer`, `light`, `additive`, `bloom` e `composite`.

- [ ] **Step 1:** Forward aditivo: instâncias com `flags & 2` saem do G-buffer e vão para uma segunda draw call sobre o HDR, com blend `ONE, ONE`, sem luz, usando `albedo.rgb * color.rgb * color.a + emissão`. O SpriteBatch mantém dois intervalos (opaco e aditivo).
- [ ] **Step 2:** Bloom: 5 mips `R16G16B16A16_SFLOAT`, do nível 0 em meia resolução até 1/32. `bloom_down`: no primeiro passo, soft threshold (`threshold`, `knee`) e filtro de 13 amostras. `bloom_up`: tent 3×3 com blend aditivo no nível de cima.
- [ ] **Step 3:** `composite.frag` final:
  1. `hdr + bloom0 * bloomIntensity`;
  2. `* exposure`;
  3. temperatura (shift R/B);
  4. AgX;
  5. lift/gamma/gain: `pow(max(c * gain + lift * (1 - c), 0), 1 / gamma)`;
  6. saturação em torno da luminância;
  7. sRGB encode.
- [ ] **Step 4:** GpuTimers com query pool de timestamps por frame em voo. Os resultados são lidos com 2 frames de atraso, sem stall.
- [ ] **Step 5:** Verificação manual: um sprite aditivo de teste brilha e gera bloom, e os tempos aparecem num log por segundo. Zero erros de validação.
- [ ] **Step 6:** Commit `render: additive pass, bloom, grading composite, gpu timers`.

### Task 7: Carregador LDtk

**Files:**
- Create: `src/engine/world/Ldtk.*`, `src/engine/world/LevelLoader.*`
- Test: `tests/tests.cpp`, `tests/data/mini.ldtk`

**Interfaces:**
- Produces:

```cpp
struct IntRect { int x, y, w, h; };                    // em células
std::vector<IntRect> mergeIntGrid(const std::vector<int>& csv, int cw, int ch, int solidValue);
struct LdtkTile { glm::ivec2 px, src; int flip; };
struct LdtkEntity { std::string identifier, iid; glm::vec2 px; glm::vec2 size; nlohmann::json fields; };
struct LdtkLayer {
    std::string identifier, type; int gridSize; glm::ivec2 cells;
    float parallaxX, parallaxY; float depth;   // ver regra abaixo
    std::string tilesetPath;                   // relativo ao .ldtk; vazio se não houver
    std::vector<int> intGrid; std::vector<LdtkTile> tiles; std::vector<LdtkEntity> entities;
};
struct LdtkLevel { std::string identifier; glm::ivec2 pxSize; std::vector<LdtkLayer> layers; /* frente -> fundo, como no LDtk */ };
LdtkLevel loadLdtkLevel(const std::filesystem::path& ldtk, const std::string& levelIdentifier);
float parseLayerDepth(const std::string& identifier, int orderFromFront, int frontIndexOfGameplay);
float ldtkParallaxToScroll(float ldtkFactor);           // conversão para a convenção da engine (1 = jogo)

using EntityFactory = std::function<void(const LdtkEntity&, glm::vec2 worldPos, EngineServices&)>;
class LevelLoader {
    void registerEntity(std::string identifier, EntityFactory);
    void load(const LdtkLevel&, EngineServices&, MaterialLibrary&);
    // tiles -> SpriteInstance estáticas guardadas no loader e reenviadas a cada frame por appendStatic(SpriteBatch&)
    // IntGrid "Collision" -> objetos com ColliderData estático na Scene2D
};
```

- **Regra da profundidade (decisão do plano: camadas LDtk não têm campos customizados):** o identificador termina com `_d<número>`, como `Mountains_d12` ou `Foreground_d-2`. Sem esse sufixo, a profundidade é `(índice da camada de jogo - índice da camada) * -1`, e a camada de jogo é a que se chama `Collision` ou a primeira de Tiles depois dela.
- **Conversão de coordenadas:** o LDtk tem Y para baixo. `world = (px.x / 32, (level.pxSize.y - px.y) / 32)`.
- **Parallax:** verificar o sinal e a semântica de `parallaxFactorX` na documentação do LDtk antes de implementar. `ldtkParallaxToScroll` converte para "1 = move com a câmera; menor = fundo". O teste fixa a conversão escolhida.

- [ ] **Step 1: Testes que falham**

```cpp
TEST(intgrid_merges_rects) {
    // 4x2: linha 0 = 1 1 0 1, linha 1 = 1 1 0 0
    auto r = mergeIntGrid({1,1,0,1, 1,1,0,0}, 4, 2, 1);
    CHECK(r.size() == 2);
    CHECK((r[0].x == 0 && r[0].y == 0 && r[0].w == 2 && r[0].h == 2));
    CHECK((r[1].x == 3 && r[1].y == 0 && r[1].w == 1 && r[1].h == 1));
}
TEST(layer_depth_from_suffix) {
    CHECK(parseLayerDepth("Mountains_d12", 0, 0) == 12.0f);
    CHECK(parseLayerDepth("Foreground_d-2", 0, 0) == -2.0f);
}
TEST(ldtk_parses_mini) {
    auto lv = loadLdtkLevel(MUSH2D_TEST_DATA "/mini.ldtk", "Level_0");
    CHECK(lv.layers.size() == 3);                        // Entities, Collision, BG_d6
    CHECK(lv.layers[0].entities[0].identifier == "PlayerStart");
    CHECK(lv.layers[2].depth == 6.0f);
}
TEST(ldtk_tolerates_unknowns) {
    // mini2.ldtk: AutoLayer com 2 autoLayerTiles, camada Tiles sem tileset, entidade "Mystery"
    auto lv = loadLdtkLevel(MUSH2D_TEST_DATA "/mini2.ldtk", "Level_0");
    CHECK(lv.layers.size() == 2);                        // camada sem tileset foi pulada
    CHECK(lv.layers[1].tiles.size() == 2);               // AutoLayer
    CHECK(lv.layers[0].entities[0].identifier == "Mystery");  // mantida; LevelLoader só avisa ao carregar
}
```

`MUSH2D_TEST_DATA` é uma definição de compilação do alvo `mush2d_tests` = `${CMAKE_SOURCE_DIR}/tests/data`. Além disso, crie `tests/data/mini2.ldtk`.

`mergeIntGrid`: guloso, linha por linha. Estende a largura enquanto a célula for sólida e ainda não usada, depois estende a altura enquanto a faixa inteira continuar sólida e livre. A ordem do resultado é pela varredura da célula inicial. `mini.ldtk` é escrito à mão com o mínimo de campos reais do formato: `levels[].layerInstances[]` com `__identifier`, `__type`, `__gridSize`, `__cWid`, `__cHei`, `__tilesetRelPath`, `intGridCsv`, `gridTiles`, `autoLayerTiles` e `entityInstances`, mais `defs.layers[]` com `parallaxFactorX/Y`.
- [ ] **Step 2:** `ctest` → FAIL.
- [ ] **Step 3:** Implementar. `AutoLayer` usa `autoLayerTiles`. Camada sem tileset → aviso e pula. Entidade sem fábrica → aviso e ignora.
- [ ] **Step 4:** `ctest` → PASS.
- [ ] **Step 5:** Commit `world: LDtk level loader with intgrid merge and entity factories`.

### Task 8: Arte provisória, sala e player de plataforma

**Files:**
- Create: `tools/gen_placeholders.py`, `assets/tilesets/placeholder{,_n,_m}.png`, `assets/parallax/{sky,mountains,forest,trees,foreground}.png`, `assets/props/crystals{,_e}.png`, `assets/atlases/characters_n.png`, `assets/levels/showcase.ldtk`
- Modify: `src/game/objects/PlayerObject.hpp`, `src/engine/platform/InputState.hpp`, `src/engine/platform/GlfwInput.cpp`, `src/game/ShowcaseGame.*`, `src/engine/graphics/Camera2D.*`

**Interfaces:**
- Consumes: `LevelLoader::registerEntity`, `LightList`, `SpriteData{depth, parallax}`, `MaterialLibrary::load`.
- Produces:
  - `InputState::jump` (tecla Z; seta para cima também pula no modo plataforma);
  - `PlayerObject::Config::MovementMode movementMode = TopDown | Platformer`;
  - `Camera2D::bounds` preenchido a partir do tamanho do nível.

- [ ] **Step 1: `gen_placeholders.py`** (Python 3, só Pillow; `python tools/gen_placeholders.py` regenera tudo de forma determinística, `random.seed(1)`):
  - **Tileset** 32 px, 8×4 tiles: pedra, grama em cima, bordas e cantos. Pixel art com 3 a 4 tons.
  - **Parallax:** 5 imagens 1024×512 em silhuetas:
    - céu: gradiente vertical, sem transparência;
    - montanhas, floresta e árvores: alpha recortado (0 ou 255);
    - primeiro plano: galhos escuros nas bordas.
  - **Cristais:** atlas 128×32 com 4 cristais; o `_e` tem as mesmas formas em ciano e magenta fortes.
  - **Normal maps** (tileset, cristais, `characters.png`): altura = distância ao pixel transparente mais próximo, saturada em 3 px, normalizada + 0.3 × luminância. Normal por Sobel, com força 2. Codificar `(n*0.5+0.5)*255`, alpha copiado.
  - **Máscara do tileset:** R = 255 nas bordas superiores (rim), G = AO mais escura nos cantos internos.
  - **`showcase.ldtk`** (gerado pelo script em JSON válido do LDtk 1.5, abrível no editor). Desvio do spec, que dizia "montada à mão": gerar pelo script é reprodutível, e depois o autor edita no LDtk normalmente.
    - 1 nível de 60×20 tiles;
    - camadas, da frente para o fundo: `Entities`, `Foreground_d-2`, `Collision` (IntGrid), `Ground` (tiles do tileset, coerentes com o IntGrid), `Trees_d3`, `Forest_d8`, `Mountains_d16`, `Sky_d40`;
    - parallax via `parallaxFactorX/Y` crescente com a profundidade;
    - cada imagem de parallax é um tileset de grade 32 coberto por `gridTiles` (as camadas se repetem horizontalmente para cobrir os 60 tiles);
    - entidades: `PlayerStart`, 5 `Light` (cor, intensidade, raio, altura, profundidade, flicker; uma delas na camada `Forest_d8`, na profundidade 8) e 3 `Crystal`.
- [ ] **Step 2:** Rodar o script e abrir `showcase.ldtk` no LDtk para confirmar que carrega. Commitar as imagens geradas.
- [ ] **Step 3:** `PlayerObject` modo `Platformer`. Os valores existentes de `Config` são mantidos e estes são novos:
  - `gravity = -30`, `jumpSpeed = 11`, `jumpCutFactor = 0.5` (soltar o pulo com velocidade Y > 0 multiplica a velocidade Y por 0.5), `maxFallSpeed = -18`;
  - chão detectado por `Collision2D::collidesAtPosition(scene, id, pos + (0, -0.02))`;
  - movimento resolvido eixo a eixo: X, depois Y, zerando a velocidade do eixo que colidir;
  - o modo `TopDown` atual continua como padrão.
- [ ] **Step 4:** `ShowcaseGame` vira o padrão ao iniciar. A tecla N continua trocando para Sandbox e Test. O jogo registra estas fábricas:
  - `PlayerStart` cria o `PlayerObject` (Platformer, material `characters` com `_n`) e uma luz própria presa a ele (quente, raio 6, altura 1.5, profundidade 0);
  - `Light` adiciona à lista de luzes do nível;
  - `Crystal` cria um sprite com o material `crystals` (com `_e`) e uma instância aditiva de halo.

  A câmera segue o player, com `bounds` = tamanho do nível.
- [ ] **Step 5:** Verificação manual:
  - a sala aparece com as 7 camadas;
  - a névoa aumenta com a profundidade;
  - o player anda, pula (pulo variável) e colide;
  - a luz do player revela o relevo do normal map nos tiles;
  - os cristais brilham com bloom.
- [ ] **Step 6:** Commit `game: placeholder art, showcase LDtk room, platformer player`.

### Task 9: Painel ImGui e `.look.json`

**Files:**
- Create: `src/engine/tools/LookFile.*`, `src/engine/tools/LookPanel.*`
- Modify: `src/engine/core/Engine.cpp` (ImGui init/frame/render), `SceneRenderer.*` (passada ImGui na swapchain após a composição), `src/game/ShowcaseGame.cpp`
- Test: `tests/tests.cpp`

**Interfaces:**
- Consumes: `FrameSettings`, `GpuTimers::lastFrameMs()`, as luzes do `LevelLoader` por `iid`, as camadas por identificador, `sourceAssetPath`.
- Produces:

```cpp
struct LightOverride { glm::vec3 color; float intensity, radius, height, depth, flicker; };
struct LayerOverride { glm::vec2 parallax; float depth; };
struct LookFile {
    FrameSettings settings;
    std::map<std::string, LayerOverride> layers;   // por identificador LDtk
    std::map<std::string, LightOverride> lights;   // por iid LDtk
};
std::optional<LookFile> parseLook(const std::string& json, const FrameSettings& defaults);  // nullopt se corrompido
std::string serializeLook(const LookFile&);
LookFile loadLookOrDefault(const std::filesystem::path&, const FrameSettings& defaults);    // aviso + defaults
void saveLook(const std::filesystem::path&, const LookFile&);
class LookPanel { void draw(LookFile&, std::vector<EditableLight>&, std::vector<EditableLayer>&, const GpuTimers&, bool& saveRequested); };
```

Chaves JSON: `ambient`, `fog` (`color`, `density`, `gradientStart`, `gradientEnd`, `gradientStrength`), `bloom` (`threshold`, `knee`, `intensity`), `exposure`, `grading` (`lift`, `gamma`, `gain`, `saturation`, `temperature`), `pixelArt` (`enabled`, `pixelateLight`, `pixelateFactor`), `lightHalfRes`, `layers` e `lights`. Chave ausente → valor padrão.

- [ ] **Step 1: Testes que falham**

```cpp
TEST(look_roundtrip) {
    LookFile a; a.settings.fogDensity = 0.2f; a.lights["abc"] = {{1,0.5f,0}, 3, 6, 1.5f, 0, 0.1f};
    auto b = parseLook(serializeLook(a), FrameSettings{});
    CHECK(b && b->settings.fogDensity == 0.2f && b->lights.at("abc").radius == 6);
}
TEST(look_ignores_unknown_and_corrupt) {
    CHECK(!parseLook("{ not json", FrameSettings{}));
    auto l = parseLook(R"({"exposure":1.5,"futureKey":3})", FrameSettings{});
    CHECK(l && l->settings.exposure == 1.5f && l->settings.fogDensity == FrameSettings{}.fogDensity);
}
```

Um `iid` ou uma camada que não existe mais no nível é ignorado com aviso na aplicação (`applyLook`), não no parse.
- [ ] **Step 2:** `ctest` → FAIL. Implementar. `ctest` → PASS.
- [ ] **Step 3:** ImGui:
  - `ImGui_ImplGlfw_InitForVulkan` + `ImGui_ImplVulkan_Init` com `UseDynamicRendering = true` e o formato da swapchain em `PipelineRenderingCreateInfo`;
  - descriptor pool próprio;
  - F1 alterna a visibilidade; com o painel aberto, a entrada de teclado do jogo é ignorada quando `io.WantCaptureKeyboard`.
- [ ] **Step 4:** `LookPanel`, com as seções:
  - Ambiente;
  - Névoa;
  - Bloom e exposição;
  - Grading;
  - Pixel art (snap, pixelar luz e fator, luz em meia resolução);
  - Camadas (parallax e profundidade);
  - Luzes (lista por `iid`, com cor e sliders);
  - Desempenho (fps e ms por passada);
  - botão "Salvar" → `saveLook(sourceAssetPath("assets/levels/showcase.look.json"))`.

  Toda mudança vale no frame seguinte.
- [ ] **Step 5:** Verificação manual: mudar a névoa e uma luz, salvar, fechar, reabrir → os ajustes persistem e o arquivo aparece no `git status` do código-fonte. Alternar o pixel art (snap e NEAREST/LINEAR) → muda sem recompilar.
- [ ] **Step 6:** Commit `tools: imgui look panel with persisted .look.json overrides`.

### Task 10: Desempenho e fechamento

**Files:**
- Modify: `README.md`, `docs/ROADMAP.md`, `assets/levels/showcase.look.json`

- [ ] **Step 1:** Release no PC Arch (RTX 2060 Super), 1920×1080, painel de desempenho: soma das passadas < 3 ms. Anotar os números no README.
- [ ] **Step 2:** Release no notebook (Intel UHD), 1280×720, com `lightHalfRes = true` salvo como padrão do notebook: 60 fps estáveis durante 60 s andando pela sala. Se não atingir, reduzir nesta ordem: bloom para 4 níveis, `fogGradient` desligado, e então medir de novo. Anotar o resultado.
- [ ] **Step 3:** Conferir os critérios de pronto 1 a 6 do spec. Em `grep -rn '"game/' src/engine`, resultado vazio. Zero erros de validação ao abrir, redimensionar, minimizar e fechar nas duas máquinas.
- [ ] **Step 4:** Marcar os itens da Fase 0 em `docs/ROADMAP.md`. Commit `docs: showcase milestone complete, perf numbers`.
