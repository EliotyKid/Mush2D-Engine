#include "engine/reflect/Reflect.hpp"

#include "engine/world/GameObject.hpp"

#include <set>
#include <stdexcept>

namespace {

SpriteData& spriteOf(GameObject& o) { return o.sprite ? *o.sprite : o.sprite.emplace(); }
ColliderData& colliderOf(GameObject& o) { return o.collider ? *o.collider : o.collider.emplace(); }
const SpriteData& spriteOr(const GameObject& o) { static const SpriteData none; return o.sprite ? *o.sprite : none; }
const ColliderData& colliderOr(const GameObject& o) { static const ColliderData none; return o.collider ? *o.collider : none; }

// GameObject's own data, so the inspector and prefabs edit it like any exported attribute. Setting a sprite or
// collider field creates the sprite / collider when the object has none.
void registerGameObject(TypeInfo& t) {
    using G = GameObject;
    using reflect::addAccessor;
    t.createObject = [] { return std::make_unique<GameObject>(); };
    addAccessor<G, std::string>(t, "name", "", [](const G& o) { return o.name; }, [](G& o, std::string v) { o.name = std::move(v); });
    addAccessor<G, glm::vec2>(t, "position", glm::vec2{0.0f}, [](const G& o) { return o.transform.position; }, [](G& o, glm::vec2 v) { o.transform.position = v; });
    addAccessor<G, glm::vec2>(t, "scale", glm::vec2{1.0f}, [](const G& o) { return o.transform.scale; }, [](G& o, glm::vec2 v) { o.transform.scale = v; });
    addAccessor<G, float>(t, "rotation", 0.0f, [](const G& o) { return o.transform.rotation; }, [](G& o, float v) { o.transform.rotation = v; });
    addAccessor<G, bool>(t, "visible", true, [](const G& o) { return o.visible; }, [](G& o, bool v) { o.visible = v; });
    addAccessor<G, int>(t, "updateOrder", 0, [](const G& o) { return o.updateOrder; }, [](G& o, int v) { o.updateOrder = v; });
    addAccessor<G, bool>(t, "persistent", false, [](const G& o) { return o.persistent; }, [](G& o, bool v) { o.persistent = v; });
    addAccessor<G, AssetRef>(t, "spriteAtlas", AssetRef{}, [](const G& o) { return AssetRef(spriteOr(o).atlasName); }, [](G& o, AssetRef v) { spriteOf(o).atlasName = std::move(v); });
    addAccessor<G, std::string>(t, "spriteFrame", "", [](const G& o) { return spriteOr(o).frameName; }, [](G& o, std::string v) { spriteOf(o).frameName = std::move(v); });
    addAccessor<G, Color>(t, "spriteColor", Color{1.0f}, [](const G& o) { return spriteOr(o).color; }, [](G& o, Color v) { spriteOf(o).color = v; });
    addAccessor<G, int>(t, "spriteLayer", 0, [](const G& o) { return spriteOr(o).layer; }, [](G& o, int v) { spriteOf(o).layer = v; });
    addAccessor<G, float>(t, "depth", 0.0f, [](const G& o) { return spriteOr(o).depth; }, [](G& o, float v) { spriteOf(o).depth = v; });
    addAccessor<G, bool>(t, "additive", false, [](const G& o) { return spriteOr(o).additive; }, [](G& o, bool v) { spriteOf(o).additive = v; });
    addAccessor<G, bool>(t, "flipX", false, [](const G& o) { return o.flipX; }, [](G& o, bool v) { o.flipX = v; });
    addAccessor<G, glm::vec2>(t, "colliderSize", glm::vec2{1.0f}, [](const G& o) { return colliderOr(o).size; }, [](G& o, glm::vec2 v) { colliderOf(o).size = v; });
    addAccessor<G, glm::vec2>(t, "colliderOffset", glm::vec2{0.0f}, [](const G& o) { return colliderOr(o).offset; }, [](G& o, glm::vec2 v) { colliderOf(o).offset = v; });
    addAccessor<G, bool>(t, "isTrigger", false, [](const G& o) { return colliderOr(o).isTrigger; }, [](G& o, bool v) { colliderOf(o).isTrigger = v; });
}

void registerComponent(TypeInfo& t) {
    t.isComponent = true; // abstract-ish base: no createComponent
    reflect::addAccessor<Component, bool>(t, "enabled", true, [](const Component& c) { return c.enabled; }, [](Component& c, bool v) { c.enabled = v; });
}

} // namespace

TypeRegistry& typeRegistry() {
    // Function-local static: exists before the first macro registration, whatever the static init order.
    // The engine roots are filled here directly (going through typeRegistry() would recurse).
    static TypeRegistry registry = [] {
        TypeRegistry r;
        registerGameObject(r.add("GameObject"));
        registerComponent(r.add("Component"));
        return r;
    }();
    return registry;
}

const char* propTypeName(PropType type) {
    switch (type) {
        case PropType::Float: return "Float";
        case PropType::Int: return "Int";
        case PropType::Bool: return "Bool";
        case PropType::String: return "String";
        case PropType::Vec2: return "Vec2";
        case PropType::Color: return "Color";
        case PropType::Enum: return "Enum";
        case PropType::PrefabRef: return "PrefabRef";
        case PropType::EntityRef: return "EntityRef";
        case PropType::AssetRef: return "AssetRef";
    }
    return "?";
}

TypeInfo& TypeRegistry::add(const std::string& name, const std::type_info* cppType) {
    if (cppType) {
        auto it = types_.find(name);
        // Same name, different C++ class (for example two games with a "Player"): refuse before touching it.
        if (it != types_.end() && it->second.cppType && *it->second.cppType != *cppType)
            throw std::runtime_error("TypeRegistry: type name \"" + name + "\" is registered twice, by C++ classes " +
                                     it->second.cppType->name() + " and " + cppType->name() +
                                     ". Type names must be unique across the compiled games.");
    }
    TypeInfo& t = types_[name];
    t.name = name;
    if (cppType) t.cppType = cppType; // remembers the owner, for the check above
    return t;
}

void TypeRegistry::addEnum(const std::string& name, std::vector<std::string> values) {
    enums_[name] = std::move(values);
}

const TypeInfo* TypeRegistry::find(const std::string& name) const {
    auto it = types_.find(name);
    return it == types_.end() ? nullptr : &it->second;
}

std::vector<const PropertyInfo*> TypeRegistry::allProps(const std::string& name) const {
    std::vector<const TypeInfo*> chain; // leaf first
    std::set<std::string> seen;
    for (const TypeInfo* t = find(name); t && seen.insert(t->name).second; t = find(t->base)) chain.push_back(t);
    std::vector<const PropertyInfo*> props;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        for (const PropertyInfo& p : (*it)->props) props.push_back(&p);
    return props;
}

const std::vector<std::string>* TypeRegistry::enumValues(const std::string& name) const {
    auto it = enums_.find(name);
    return it == enums_.end() ? nullptr : &it->second;
}

const TypeInfo& GameObject::reflectType() const { return *typeRegistry().find("GameObject"); }
const TypeInfo& Component::reflectType() const { return *typeRegistry().find("Component"); }

const TypeInfo& typeOf(const GameObject& object) { return object.reflectType(); }
const TypeInfo& typeOf(const Component& component) { return component.reflectType(); }

std::vector<std::string> reflect::splitEnumNames(const char* list) {
    std::vector<std::string> names;
    std::string current;
    auto flush = [&] {
        const auto b = current.find_first_not_of(" \t\r\n");
        const auto e = current.find_last_not_of(" \t\r\n");
        if (b != std::string::npos) names.push_back(current.substr(b, e - b + 1));
        current.clear();
    };
    for (const char* c = list; *c; ++c) {
        if (*c == ',') flush();
        else current += *c;
    }
    flush();
    return names;
}
