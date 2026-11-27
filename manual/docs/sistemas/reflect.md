# reflect

`src/engine/reflect/` é um registro pequeno de classes e dos seus atributos editáveis ("exportados"). Prefabs, o inspetor F1 e o `prefab_schema.json` leem dele.

## O que faz

Os macros registram a classe pelo nome e cada atributo com tipo e valor padrão. Com isso a engine consegue:

- criar um objeto pelo nome que está no prefab (`"type": "SlimeEnemy"`);
- aplicar valores do prefab e dos campos do LDtk aos atributos;
- mostrar e editar os atributos no inspetor;
- gerar `assets/prefabs/prefab_schema.json` no Debug, com todas as classes, atributos e enums.

## Os macros

```cpp
MUSH2D_ENUM(Element, Fire, Ice, Combined, None)   // declara `enum class Element` e os nomes

class Slime : public EnemyBase {
    MUSH2D_OBJECT(Slime, EnemyBase)                // registra a classe e a base C++
public:
    MUSH2D_EXPORT(float, speed, 1.0f)              // um membro normal `float speed = 1.0f;`
    MUSH2D_EXPORT(Element, weakness, Element::Fire) //   que o editor vê e muda
};

struct Glow : Component {
    MUSH2D_COMPONENT(Glow)                         // o mesmo para componentes
    MUSH2D_EXPORT(float, radius, 3.0f)
};
```

### Regras

- `MUSH2D_OBJECT` / `MUSH2D_COMPONENT` vêm primeiro no corpo da classe e deixam a classe em `public:`.
- A classe precisa de um construtor sem argumentos para ser criada por prefab.
- `MUSH2D_ENUM` fica fora da classe. Os valores são numerados 0, 1, 2... (sem `= 5`).
- Uma classe em `examples/iki_demo/src/iki_demo/objects/*.hpp` é registrada só por existir (o build inclui todo header dessa pasta).

### Tipos de atributo

| Tipo C++ | Tipo no schema | No prefab / LDtk |
|---|---|---|
| `float` | `Float` | número |
| `int` | `Int` | número inteiro |
| `bool` | `Bool` | `true` / `false` |
| `std::string` | `String` | texto |
| `glm::vec2` | `Vec2` | `[x, y]` |
| `Color` (`glm::vec4`) | `Color` | `[r, g, b, a]` |
| enum de `MUSH2D_ENUM` | `Enum` | o nome do valor, como `"Platformer"` |
| `PrefabRef` | `PrefabRef` | nome de um prefab |
| `EntityRef` | `EntityRef` | iid de uma entidade do LDtk (campo Entity ref) |
| `AssetRef` | `AssetRef` | `"atlas/frame"` ou `"atlas/animation"` |

`PrefabRef`, `EntityRef` e `AssetRef` se comportam como `std::string`; só dizem ao editor para onde o texto aponta.

## Tipos principais

| Tipo | Papel |
|---|---|
| `TypeInfo` | Uma classe registrada: nome, base, se é componente, atributos, como criar. |
| `PropType` | `Float`, `Int`, `Bool`, `String`, `Vec2`, `Color`, `Enum`, `PrefabRef`, `EntityRef`, `AssetRef`. |
| `exportSchema()` | Gera o JSON do `prefab_schema.json`. |

## Casos de uso

### Deixar uma variável editável no prefab e no LDtk

Troque `float pulo = 8.0f;` por `MUSH2D_EXPORT(float, pulo, 8.0f)`. Pronto: o prefab aceita `"props": { "pulo": 10 }`, um campo `pulo` na entidade do LDtk muda a instância, e o inspetor mostra o valor.

### Um modo escolhido por nome

```cpp
MUSH2D_ENUM(Humor, Calmo, Bravo)
// na classe:
MUSH2D_EXPORT(Humor, humor, Humor::Calmo)
```

No prefab: `"props": { "humor": "Bravo" }`.

### Conferir os nomes certos

Abra `assets/prefabs/prefab_schema.json` depois de rodar o Debug. Ele lista cada classe, a base e os atributos com tipo e padrão.

### Um tipo de atributo novo

Um ramo em `propTypeOf`, `toJson` e `fromJson` em `Reflect.hpp`, mais um widget no inspetor (veja [tools](tools.md)).

## Arquivos

`src/engine/reflect/Reflect.hpp/.cpp`, `SchemaExport.cpp`.
