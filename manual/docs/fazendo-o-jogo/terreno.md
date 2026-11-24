# Terreno

A entidade `Terrain` faz chão livre em qualquer ângulo. Ela fica na camada `Terrain_d0` (grade de 4 px).

## Campos

| Campo | Efeito |
|---|---|
| posição da entidade | o primeiro ponto da linha |
| `points` (Points) | os pontos seguintes, desenhados como caminho |
| `closed` | polígono fechado. Desligado: superfície preenchida para baixo até `thickness` |
| `thickness` | espessura em unidades (padrão 3; 32 px = 1 u) |
| `oneWay` | atravessável: só os trechos virados para cima colidem, como um galho |
| `style` | `forest` ou `cave` (texturas em `assets/terrain/<style>/`). Um estilo sem texturas usa `forest` e avisa uma vez |

A engine gera a colisão (`CollisionWorld`) e o visual (borda, lados e preenchimento, por `TerrainMesh`) a partir da linha.

## Casos de uso

### Um morro

Crie uma entidade `Terrain`, desenhe os pontos subindo e descendo, deixe `closed` desligado e `thickness` grande o bastante para cobrir até o chão de baixo.

### Um galho atravessável

Desenhe uma linha curta, ligue `oneWay` e deixe `thickness` pequeno. É o galho sobre a descida de `Forest2`.

### Uma rocha flutuante

Desenhe o contorno e ligue `closed`. O polígono inteiro colide e é preenchido.

### Terreno de caverna

Ponha `style` = `cave`. Usa as texturas de `assets/terrain/cave/`.
