# Painel F1 e inspetor

`F1` abre duas janelas ImGui: o **painel de visual e desempenho** e a janela **"Cena (F1)"**. `MUSH2D_SHOW_PANEL=1` já abre o jogo com elas visíveis.

## Painel de visual e desempenho

Edita o **look** da região atual. Seções:

| Seção | O que ajusta |
|---|---|
| Ambiente | cor e intensidade da luz ambiente |
| Nevoa | densidade e gradiente (início, fim, força) |
| Bloom e exposicao | limiar, joelho, intensidade do bloom, exposição |
| Grading | saturação, temperatura |
| Pixel art | modo (pixel art nítido, pixel art suave, HD), pixelar luz e fator, luz em meia resolução |
| Fundo da regiao | camadas de parallax: profundidade, repetir em X |
| Luzes | intensidade, raio, altura e flicker de cada luz |
| Camera | `maxShake`, o limite do tremor da câmera (só em tempo de execução, não é salvo) |
| Desempenho | fps e ms de GPU de cada passada (aberta por padrão) |

"Salvar" grava a região atual em `assets/regions/<nome>.region.json` (look e camadas de fundo).

!!! warning "Salve no Debug"
    No Release, "Salvar" grava ao lado do executável, e a cópia de assets do próximo build sobrescreve esse arquivo. Para persistir, use o Debug, que grava no código-fonte.

## Janela "Cena (F1)"

A árvore de objetos ao vivo: objeto, filhos e componentes. Tem um filtro por nome ou tag e uma caixa "pausar o jogo".

- Os atributos editados valem na hora.
- Alguns (atlas, frame, variante, vida máxima, política de persistência) só aparecem depois de salvar e recarregar.
- Um valor que veio de um campo do LDtk mostra o aviso de que ele não é salvo no prefab: mude no LDtk.

### Salvar

| Botão | Efeito |
|---|---|
| "Salvar no prefab" | Grava no `.prefab.json` de origem só os atributos editados que diferem da base. |
| "Salvar como novo prefab" | Cria `<nome>.prefab.json` com `base` = o prefab atual e só os atributos editados que diferem dele. |

Valores vindos do LDtk nunca vão para o prefab. Salvar ou editar um prefab recarrega os objetos dele, e edições não salvas se perdem.

Um objeto sem prefab de origem (criado em código ou filho inline) só pode usar "Salvar como novo prefab". Se várias instâncias do mesmo prefab forem editadas de forma diferente, cada uma grava a própria diferença no mesmo arquivo, e a última gravação vence.

## Casos de uso

### Ajustar a velocidade do slime jogando

1. `F1`, marque "pausar o jogo" se quiser.
2. Filtre por `Slime`, abra o objeto e mude `moveSpeed`.
3. Desmarque a pausa e teste.
4. Gostou: "Salvar no prefab". Todos os slimes passam a usar o valor.

### Criar uma variação a partir de uma instância

Edite a cor e a velocidade de um slime e use "Salvar como novo prefab" com o nome `slime_azul`. Depois use a entidade `SlimeAzul` no LDtk, ou o campo `prefab` = `slime_azul`.

### Deixar a caverna mais escura

Ande até `Cave1`, abra `F1`, baixe a intensidade em "Ambiente", aumente a densidade em "Nevoa" e clique em "Salvar" (no Debug).
