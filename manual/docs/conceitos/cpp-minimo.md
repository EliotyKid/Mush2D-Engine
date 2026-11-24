# C++ mínimo

Você só precisa disto para começar a ler o código:

```cpp
#include "engine/world/GameObject.hpp"   // "usa o código deste outro arquivo" (como importar)

class Slime : public GameObject {         // "Slime é um GameObject" (herança = objeto filho no GameMaker)
public:                                   // o que vem abaixo pode ser usado de fora da classe
    float speed = 2.0f;                   // variável da instância, com valor inicial. "f" = número float

    void onStep(Scene2D& scene, float dt) override {   // "override" = estou substituindo o evento do pai
        transform.position.x += speed * dt;            // dt = segundos desde o frame anterior
    }
private:                                  // só a própria classe enxerga
    int hp = 3;                           // int = número inteiro
};
```

| Sinal | O que significa |
|---|---|
| `;` | fim de uma instrução (obrigatório) |
| `{ }` | bloco: o corpo de uma função, de um `if` ou de uma classe |
| `//` | comentário, ignorado pelo compilador |
| `.` | acessa algo de um objeto: `transform.position` |
| `->` | o mesmo que `.`, mas através de um **ponteiro** (`player->transform`) |
| `&` no tipo (`Scene2D& scene`) | "referência": é o próprio objeto, não uma cópia |
| `*` no tipo (`Player* p`) | ponteiro: um endereço que pode ser `nullptr` (vazio). Sempre teste com `if (p)` |
| `auto` | "o compilador descobre o tipo sozinho" |
| `std::string`, `std::vector<T>` | texto e lista (array dinâmico) |
| `glm::vec2` | um par (x, y) de floats |
| `glm::vec4` | quatro floats, usado para cor RGBA |
| `a ? b : c` | "se a, então b, senão c" |
| `const` depois de uma função | a função não altera o objeto |
| `template<typename T>` | função que funciona para vários tipos. Exemplo: `createObject<Slime>()` |
| `dynamic_cast<T*>(&x)` | "x é um T?" Devolve `nullptr` se não for. Como `other.object_index == obj_player` |
| `#pragma once` | "inclua este arquivo uma vez só" (primeira linha de todo `.hpp`) |
| `.hpp` / `.cpp` | declaração ("o que existe") / implementação ("como funciona"). Objetos pequenos ficam só no `.hpp` |

## Compilar

Compilar transforma o texto em `.exe`. Veja os comandos em [Primeiros passos](../primeiros-passos.md#compilar-e-rodar).

Se der erro, o compilador mostra `arquivo(linha): error ...`. A linha indicada é onde ele percebeu o problema, que às vezes vem de uma linha acima.

## Os macros da engine

| Macro | Para quê |
|---|---|
| `MUSH2D_OBJECT(Classe, Pai)` | Registra a classe pelo nome, para o prefab (`"type": "Classe"`) saber qual criar. |
| `MUSH2D_EXPORT(tipo, nome, padrão)` | Declara uma variável que o prefab, o LDtk e o inspetor podem mudar. |
| `MUSH2D_COMPONENT(Classe)` | O mesmo que `MUSH2D_OBJECT`, para componentes. |
| `MUSH2D_ENUM(Nome, A, B, C)` | Um enum que o prefab e o inspetor entendem pelo nome. |

Veja [reflect](../sistemas/reflect.md) para os detalhes.
