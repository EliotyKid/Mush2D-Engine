# gpu

`src/engine/gpu/` conversa direto com a placa de vídeo pelo Vulkan 1.3. Você não precisa tocar nisso para fazer jogos.

## O que faz

Cria o dispositivo Vulkan, a troca de imagens da tela (swapchain) e a memória da GPU.

## Tipos principais

| Tipo | Papel |
|---|---|
| `GpuContext` | Instância, dispositivo e filas do Vulkan (com vk-bootstrap). Entregue ao jogo em `EngineServices::gpu`. |
| `Swapchain` | As imagens da janela; recriadas quando a janela muda de tamanho. |
| `Buffer`, `Image` | Buffers e imagens da GPU, alocados pelo VMA. |
| `vma.cpp` | Compila a biblioteca VMA (Vulkan Memory Allocator). |

## Casos de uso

- **O jogo não abre e reclama de Vulkan:** confira `vulkaninfo --summary` (`apiVersion` >= 1.3) e o driver da placa.
- **Carregar uma textura nova em código:** use a `MaterialLibrary` (veja [render](render.md)), não estes tipos diretamente.

## Arquivos

`src/engine/gpu/GpuContext.*`, `Swapchain.*`, `Buffer.*`, `Image.*`, `vma.cpp`.
