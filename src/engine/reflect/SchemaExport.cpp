#include "engine/reflect/Reflect.hpp"

nlohmann::json exportSchema() {
    const TypeRegistry& registry = typeRegistry();
    nlohmann::json types = nlohmann::json::object();
    for (const auto& [name, type] : registry.types()) {
        nlohmann::json props = nlohmann::json::array();
        for (const PropertyInfo& p : type.props) {
            nlohmann::json prop{{"name", p.name}, {"type", propTypeName(p.type)}, {"default", p.defaultValue}};
            if (p.type == PropType::Enum) prop["enum"] = p.enumName;
            props.push_back(std::move(prop));
        }
        types[name] = {{"base", type.base}, {"component", type.isComponent}, {"props", std::move(props)}};
    }
    nlohmann::json enums = nlohmann::json::object();
    for (const auto& [name, values] : registry.enums()) enums[name] = values;
    return {{"version", 1}, {"types", std::move(types)}, {"enums", std::move(enums)}};
}
