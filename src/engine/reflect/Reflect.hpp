#pragma once

// Mush2D reflection: a tiny registry of classes and their editable ("exported") attributes, read by
// prefabs, the F1 inspector and prefab_schema.json.
//
// HOW TO USE (this is all you need to write):
//
//     MUSH2D_ENUM(Element, Fire, Ice, Combined, None)   // declares `enum class Element` and its names
//
//     class Slime : public EnemyBase {
//         MUSH2D_OBJECT(Slime, EnemyBase)                // registers the class and its C++ base
//     public:
//         MUSH2D_EXPORT(float, speed, 1.0f)              // a normal member `float speed = 1.0f;`
//         MUSH2D_EXPORT(float, patrolDistance, 2.0f)     //   that the editor can also see and change
//         MUSH2D_EXPORT(Element, weakness, Element::Fire)
//
//         void onStep(Scene2D& scene, float dt) override { transform.position.x += speed * dt; }
//     };
//
//     struct Glow : Component {
//         MUSH2D_COMPONENT(Glow)                         // same idea for components
//         MUSH2D_EXPORT(float, radius, 3.0f)
//     };
//
// Rules:
// - MUSH2D_OBJECT / MUSH2D_COMPONENT go first in the class body. They leave the class in `public:`.
// - The class needs a constructor without arguments to be created by name (prefabs). Without one it is
//   still registered, but createObject / createComponent stay empty.
// - Exported types: float, int, bool, std::string, glm::vec2, Color (glm::vec4), MUSH2D_ENUM enums,
//   PrefabRef (prefab name), EntityRef (LDtk iid) and AssetRef ("atlas/frame" or "atlas/animation").
//   PrefabRef, EntityRef and AssetRef behave like std::string.
// - MUSH2D_ENUM goes at namespace scope (not inside a class). Values are numbered 0, 1, 2... (no `= 5`).
// - A class in src/game/objects/*.hpp is registered just by existing (the build includes every header).
//
// A new attribute type = one branch in propTypeOf, toJson and fromJson below (plus an inspector widget).

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

class GameObject;
class Component;

using Color = glm::vec4;

// String-like reference types: they only tell the editor what the text points to.
struct PrefabRef : std::string {
    using std::string::string;
    PrefabRef(std::string s = {}) : std::string(std::move(s)) {}
};
struct EntityRef : std::string {
    using std::string::string;
    EntityRef(std::string s = {}) : std::string(std::move(s)) {}
};
struct AssetRef : std::string {
    using std::string::string;
    AssetRef(std::string s = {}) : std::string(std::move(s)) {}
};

enum class PropType { Float, Int, Bool, String, Vec2, Color, Enum, PrefabRef, EntityRef, AssetRef };
const char* propTypeName(PropType type);

struct PropertyInfo {
    std::string name;
    PropType type = PropType::Float;
    std::string enumName; // PropType::Enum only
    nlohmann::json defaultValue;
    // obj is the GameObject* (or Component*) passed as void*, never a pointer to the derived class.
    std::function<nlohmann::json(const void*)> get;
    // false + error when the JSON has the wrong type; the member is left unchanged then.
    std::function<bool(void*, const nlohmann::json&, std::string& error)> set;
};

struct TypeInfo {
    std::string name, base; // base is "" for GameObject and Component
    bool isComponent = false;
    std::function<std::unique_ptr<GameObject>()> createObject;
    std::function<std::unique_ptr<Component>()> createComponent;
    std::vector<PropertyInfo> props; // own props only, in declaration order
};

class TypeRegistry {
public:
    const TypeInfo* find(const std::string& name) const;
    // Every prop of the type, base classes first (GameObject -> EnemyBase -> Slime). Empty if unknown.
    std::vector<const PropertyInfo*> allProps(const std::string& name) const;
    const std::vector<std::string>* enumValues(const std::string& name) const;

    const std::map<std::string, TypeInfo>& types() const { return types_; }
    const std::map<std::string, std::vector<std::string>>& enums() const { return enums_; }

    // Used by the macros.
    TypeInfo& add(const std::string& name); // get or create
    void addEnum(const std::string& name, std::vector<std::string> values);

private:
    std::map<std::string, TypeInfo> types_; // std::map: stable references and a sorted schema
    std::map<std::string, std::vector<std::string>> enums_;
};

TypeRegistry& typeRegistry();

const TypeInfo& typeOf(const GameObject& object);
const TypeInfo& typeOf(const Component& component);

// {"version": 1, "types": {name: {base, component, props: [{name, type, default, enum?}]}}, "enums": {...}}
nlohmann::json exportSchema();

// --- Internals used by the macros ----------------------------------------------------------------------
namespace reflect {

// "Fire, Ice, None" -> {"Fire", "Ice", "None"}
std::vector<std::string> splitEnumNames(const char* list);

template<class T>
concept RegisteredEnum = std::is_enum_v<T> && requires(T e) { mush2dEnumName(e); mush2dEnumValues(e); };

template<class T>
PropType propTypeOf() {
    if constexpr (std::is_same_v<T, float>) return PropType::Float;
    else if constexpr (std::is_same_v<T, int>) return PropType::Int;
    else if constexpr (std::is_same_v<T, bool>) return PropType::Bool;
    else if constexpr (std::is_same_v<T, PrefabRef>) return PropType::PrefabRef;
    else if constexpr (std::is_same_v<T, EntityRef>) return PropType::EntityRef;
    else if constexpr (std::is_same_v<T, AssetRef>) return PropType::AssetRef;
    else if constexpr (std::is_same_v<T, std::string>) return PropType::String;
    else if constexpr (std::is_same_v<T, glm::vec2>) return PropType::Vec2;
    else if constexpr (std::is_same_v<T, glm::vec4>) return PropType::Color;
    else if constexpr (RegisteredEnum<T>) return PropType::Enum;
    else static_assert(sizeof(T) == 0, "MUSH2D_EXPORT: unsupported type (enums need MUSH2D_ENUM)");
}

template<class T>
nlohmann::json toJson(const T& v) {
    if constexpr (std::is_same_v<T, glm::vec2>) return {v.x, v.y};
    else if constexpr (std::is_same_v<T, glm::vec4>) return {v.x, v.y, v.z, v.w};
    else if constexpr (std::is_base_of_v<std::string, T>) return static_cast<const std::string&>(v);
    else if constexpr (RegisteredEnum<T>) {
        const auto& names = mush2dEnumValues(v);
        const auto i = static_cast<std::size_t>(v);
        return i < names.size() ? nlohmann::json(names[i]) : nlohmann::json(nullptr);
    } else return v;
}

template<class V>
bool vecFromJson(const nlohmann::json& j, V& out) {
    if (!j.is_array() || j.size() != std::size_t(V::length())) return false;
    for (int i = 0; i < V::length(); ++i) {
        if (!j[i].is_number()) return false;
        out[i] = j[i].get<float>();
    }
    return true;
}

template<class T>
bool fromJson(const nlohmann::json& j, T& out, std::string& error) {
    const PropType type = propTypeOf<T>();
    auto wrong = [&] {
        error = std::string("expected ") + propTypeName(type) + ", got " + j.dump();
        return false;
    };
    if constexpr (std::is_same_v<T, float>) {
        if (!j.is_number()) return wrong();
        out = j.get<float>();
    } else if constexpr (std::is_same_v<T, int>) {
        if (!j.is_number_integer()) return wrong();
        out = j.get<int>();
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!j.is_boolean()) return wrong();
        out = j.get<bool>();
    } else if constexpr (std::is_base_of_v<std::string, T>) {
        if (!j.is_string()) return wrong();
        out = T(j.get<std::string>());
    } else if constexpr (std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec4>) {
        T v;
        if (!vecFromJson(j, v)) return wrong();
        out = v;
    } else if constexpr (RegisteredEnum<T>) {
        if (!j.is_string()) return wrong();
        const auto& names = mush2dEnumValues(out);
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (names[i] == j.get<std::string>()) {
                out = static_cast<T>(i);
                return true;
            }
        }
        error = "unknown " + std::string(mush2dEnumName(out)) + " value " + j.dump();
        return false;
    }
    return true;
}

// The object as its own class, from the GameObject* / Component* passed as void*.
template<class C>
C* cast(void* obj) {
    if constexpr (std::is_base_of_v<GameObject, C>) return static_cast<C*>(static_cast<GameObject*>(obj));
    else return static_cast<C*>(static_cast<Component*>(obj));
}
template<class C>
const C* cast(const void* obj) { return cast<C>(const_cast<void*>(obj)); }

// Adds (or replaces) a prop read and written through get(const C&) -> T and set(C&, T).
template<class C, class T, class Get, class Set>
void addAccessor(TypeInfo& type, const std::string& name, const T& defaultValue, Get get, Set set) {
    PropertyInfo p;
    p.name = name;
    p.type = propTypeOf<T>();
    if constexpr (RegisteredEnum<T>) {
        p.enumName = mush2dEnumName(defaultValue);
        typeRegistry().addEnum(p.enumName, mush2dEnumValues(defaultValue));
    }
    p.defaultValue = toJson(defaultValue);
    p.get = [get](const void* obj) { return toJson<T>(get(*cast<C>(obj))); };
    p.set = [set](void* obj, const nlohmann::json& j, std::string& error) {
        T value{};
        if (!fromJson(j, value, error)) return false;
        set(*cast<C>(obj), std::move(value));
        return true;
    };
    for (PropertyInfo& existing : type.props) {
        if (existing.name == name) {
            existing = std::move(p);
            return;
        }
    }
    type.props.push_back(std::move(p));
}

template<class C, class T>
void addMember(TypeInfo& type, const char* name, T C::*member, const T& defaultValue) {
    addAccessor<C, T>(type, name, defaultValue,
        [member](const C& c) { return c.*member; },
        [member](C& c, T v) { c.*member = std::move(v); });
}

template<class C>
TypeInfo& registerClass(const char* name, const char* base) {
    TypeInfo& type = typeRegistry().add(name);
    type.base = base;
    constexpr bool creatable = std::is_default_constructible_v<C> && !std::is_abstract_v<C>;
    if constexpr (std::is_base_of_v<Component, C>) {
        type.isComponent = true;
        if constexpr (creatable) type.createComponent = [] { return std::unique_ptr<Component>(std::make_unique<C>()); };
    } else {
        if constexpr (creatable) type.createObject = [] { return std::unique_ptr<GameObject>(std::make_unique<C>()); };
    }
    return type;
}

} // namespace reflect

// --- The macros ------------------------------------------------------------------------------------------
// How they work: every macro adds an `inline static const bool` whose initializer runs once at program start
// and writes into typeRegistry(). Member access happens inside static member functions, where the class is
// complete. Order inside a class is declaration order, so props keep the order they are written in.

#define MUSH2D_REFLECT_CLASS_(Class, BaseName, BaseCheck)                                           \
public:                                                                                             \
    using ReflectSelf = Class;                                                                      \
    static TypeInfo& reflectStaticType() {                                                          \
        static_assert(std::is_base_of_v<BaseCheck, Class>, #Class " must inherit from " #BaseCheck); \
        static TypeInfo& type = reflect::registerClass<Class>(#Class, BaseName);                    \
        return type;                                                                                \
    }                                                                                               \
    const TypeInfo& reflectType() const override { return reflectStaticType(); }                    \
    inline static const bool reflectClassRegistered_ = (reflectStaticType(), true);

// Registers a GameObject class and its C++ base (GameObject or another registered class).
#define MUSH2D_OBJECT(Class, Base) MUSH2D_REFLECT_CLASS_(Class, #Base, Base)

// Registers a Component class.
#define MUSH2D_COMPONENT(Class) MUSH2D_REFLECT_CLASS_(Class, "Component", Component)

// Declares `Type Name = default;` and adds it to the class's props. The default may contain commas.
#define MUSH2D_EXPORT(Type, Name, ...)                                                       \
    Type Name = __VA_ARGS__;                                                                 \
    static void reflectProp_##Name##_(TypeInfo& type) {                                      \
        reflect::addMember<ReflectSelf, Type>(type, #Name, &ReflectSelf::Name, Type(__VA_ARGS__)); \
    }                                                                                        \
    inline static const bool reflectPropRegistered_##Name##_ = (reflectProp_##Name##_(reflectStaticType()), true);

// Declares `enum class Enum { values... }` and registers the value names. Namespace scope only.
#define MUSH2D_ENUM(Enum, ...)                                                                  \
    enum class Enum { __VA_ARGS__ };                                                            \
    inline const char* mush2dEnumName(Enum) { return #Enum; }                                   \
    inline const std::vector<std::string>& mush2dEnumValues(Enum) {                             \
        static const std::vector<std::string> names = reflect::splitEnumNames(#__VA_ARGS__);    \
        return names;                                                                           \
    }                                                                                           \
    inline const bool mush2dEnumRegistered_##Enum##_ =                                          \
        (typeRegistry().addEnum(#Enum, mush2dEnumValues(Enum{})), true);
