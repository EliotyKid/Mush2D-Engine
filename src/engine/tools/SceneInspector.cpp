#include "engine/tools/SceneInspector.hpp"

#include "engine/world/PrefabSave.hpp"
#include "engine/world/Resources.hpp"
#include "engine/world/StateMachine.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

using json = nlohmann::json;

namespace {

std::string labelOf(const GameObject& o) { return o.name.empty() ? typeOf(o).name : o.name; }

bool matches(const GameObject& o, const std::string& filter) {
    if (o.name.find(filter) != std::string::npos) return true;
    return std::any_of(o.tags.begin(), o.tags.end(), [&](const std::string& t) { return t.find(filter) != std::string::npos; });
}

// One widget for an attribute of obj (GameObject* or Component* as void*); true when the value was changed.
bool propWidget(void* obj, const PropertyInfo& p) {
    const json v = p.get(obj);
    json changed;
    const char* label = p.name.c_str();
    switch (p.type) {
        case PropType::Float: {
            float f = v.is_number() ? v.get<float>() : 0.0f;
            if (ImGui::DragFloat(label, &f, 0.05f)) changed = f;
            break;
        }
        case PropType::Int: {
            int i = v.is_number_integer() ? v.get<int>() : 0;
            if (ImGui::DragInt(label, &i)) changed = i;
            break;
        }
        case PropType::Bool: {
            bool b = v.is_boolean() && v.get<bool>();
            if (ImGui::Checkbox(label, &b)) changed = b;
            break;
        }
        case PropType::String:
        case PropType::PrefabRef:
        case PropType::EntityRef:
        case PropType::AssetRef: {
            char buf[256];
            std::snprintf(buf, sizeof buf, "%s", v.is_string() ? v.get<std::string>().c_str() : "");
            if (ImGui::InputText(label, buf, sizeof buf)) changed = std::string(buf);
            break;
        }
        case PropType::Vec2:
        case PropType::Color: {
            float c[4] = {0, 0, 0, 0};
            for (std::size_t i = 0; i < v.size() && i < 4; ++i) c[i] = v[i].is_number() ? v[i].get<float>() : 0.0f;
            const bool edit = p.type == PropType::Vec2 ? ImGui::DragFloat2(label, c, 0.05f) : ImGui::ColorEdit4(label, c, ImGuiColorEditFlags_Float);
            if (edit) changed = p.type == PropType::Vec2 ? json{c[0], c[1]} : json{c[0], c[1], c[2], c[3]};
            break;
        }
        case PropType::Enum: {
            const std::vector<std::string>* values = typeRegistry().enumValues(p.enumName);
            const std::string current = v.is_string() ? v.get<std::string>() : "?";
            if (values && ImGui::BeginCombo(label, current.c_str())) {
                for (const std::string& value : *values)
                    if (ImGui::Selectable(value.c_str(), value == current)) changed = value;
                ImGui::EndCombo();
            }
            break;
        }
    }
    if (changed.is_null()) return false;
    std::string error;
    return p.set(obj, changed, error);
}

} // namespace

void SceneInspector::draw(Scene2D& scene, PrefabLibrary& prefabs, bool& pause) {
    ImGui::SetNextWindowPos({400, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({380, 620}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cena (F1)")) {
        ImGui::End();
        return;
    }
    std::erase_if(edited_, [&](const auto& e) { return !scene.findObject(e.first); }); // objects gone (hot reload)
    ImGui::Checkbox("pausar o jogo", &pause);
    ImGui::InputText("filtro (nome ou tag)", filter_, sizeof filter_);

    ImGui::BeginChild("arvore", {0, ImGui::GetContentRegionAvail().y * 0.4f}, ImGuiChildFlags_Borders);
    const std::string filter = filter_;
    if (!filter.empty()) { // a flat list of the matches
        scene.forEach([&](GameObject& o) {
            if (!matches(o, filter)) return;
            ImGui::PushID(&o);
            if (ImGui::Selectable(labelOf(o).c_str(), o.id == selected_)) selected_ = o.id;
            ImGui::PopID();
        });
    } else {
        std::map<std::string, std::vector<GameObject*>> rooms; // roots by room
        scene.forEach([&](GameObject& o) { if (!o.parent()) rooms[o.roomIid].push_back(&o); });
        for (auto& [room, roots] : rooms) {
            if (!ImGui::TreeNode(room.empty() ? "(sem sala)" : room.c_str())) continue;
            for (GameObject* o : roots) drawNode(scene, *o);
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();

    if (GameObject* o = scene.findObject(selected_)) drawSelected(*o, prefabs);
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::End();
}

void SceneInspector::drawNode(Scene2D& scene, GameObject& o) {
    ImGui::PushID(&o);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (o.id == selected_) flags |= ImGuiTreeNodeFlags_Selected;
    if (o.children().empty() && o.components().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    const bool open = ImGui::TreeNodeEx("node", flags, "%s", labelOf(o).c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) selected_ = o.id;
    if (open) {
        for (GameObjectId child : o.children())
            if (GameObject* c = scene.findObject(child)) drawNode(scene, *c);
        for (const auto& c : o.components())
            ImGui::BulletText("[%s]", typeOf(*c).name.c_str());
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void SceneInspector::drawSelected(GameObject& o, PrefabLibrary& prefabs) {
    ImGui::SeparatorText(labelOf(o).c_str());
    ImGui::Text("prefab: %s", o.prefab.empty() ? "(nenhum: criado em codigo ou filho inline)" : o.prefab.c_str());
    ImGui::Text("entityIid: %s", o.entityIid.empty() ? "-" : o.entityIid.c_str());
    for (const auto& [label, machine] : o.stateMachines())
        ImGui::Text("%s: %s (%.2f s)", label.c_str(), machine->current(), machine->timeInState());

    std::set<std::string>& edited = edited_[o.id];
    std::vector<const TypeInfo*> chain; // GameObject -> ... -> the object's class
    for (const TypeInfo* t = &typeOf(o); t; t = t->base.empty() ? nullptr : typeRegistry().find(t->base)) chain.insert(chain.begin(), t);
    void* self = static_cast<GameObject*>(&o);
    for (const TypeInfo* t : chain) {
        ImGui::SeparatorText(t->name.c_str());
        for (const PropertyInfo& p : t->props) {
            const bool fromLdtk = o.ldtkOverridden.count(p.name) > 0;
            if (propWidget(self, p)) {
                edited.insert(p.name);
                if (fromLdtk) status_ = p.name + " vem do LDtk: mude no LDtk (nao e salvo no prefab).";
            }
            if (fromLdtk) {
                ImGui::SameLine();
                ImGui::TextDisabled("(LDtk)");
                ImGui::SetItemTooltip("Valor de um campo do LDtk: mude no LDtk. Nao e salvo no prefab.");
            }
        }
    }
    for (const auto& c : o.components()) {
        ImGui::PushID(c.get());
        if (ImGui::TreeNode("comp", "%s (ao vivo, nao salvo)", typeOf(*c).name.c_str())) {
            for (const PropertyInfo* p : typeRegistry().allProps(typeOf(*c).name)) propWidget(static_cast<Component*>(c.get()), *p);
            // Resources keeps its bars in a list (not exported attributes): one set of controls per bar.
            if (auto* resources = dynamic_cast<Resources*>(c.get())) {
                for (ResourceBarDef& bar : resources->bars()) {
                    ImGui::PushID(bar.name.c_str());
                    ImGui::SeparatorText(bar.name.c_str());
                    ImGui::DragFloat("max", &bar.max, 0.5f, 1.0f, 100000.0f);
                    ImGui::DragFloat("regenPerSecond", &bar.regenPerSecond, 0.1f, 0.0f, 100000.0f);
                    ImGui::DragFloat("regenDelay", &bar.regenDelay, 0.05f, 0.0f, 100.0f);
                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (!o.extra().empty() && ImGui::TreeNode("extra")) {
        ImGui::TextWrapped("%s", o.extra().dump(1, ' ', false, json::error_handler_t::replace).c_str());
        ImGui::TreePop();
    }

    ImGui::Separator();
    std::string error;
    const bool canSave = canSaveToPrefab(o);
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button("Salvar no prefab"))
        status_ = saveToSourcePrefab(prefabs, o, edited, error) ? "Salvo em " + o.prefab + ".prefab.json" : error;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s%s", canSave ? "Grava no prefab de origem so os atributos editados que diferem da base.\n"
                                            "Varias instancias do mesmo prefab editadas de forma diferente: cada uma grava\n"
                                            "a propria diferenca no mesmo arquivo, e a ultima gravacao vence."
                                          : "Sem prefab de origem (criado em codigo ou filho inline):\n"
                                            "use 'Salvar como novo prefab'.",
                          canSave && o.parent() ? "\nFilho: campos definidos na entrada do pai vencem o prefab." : "");
    ImGui::InputText("nome do novo prefab", newPrefab_, sizeof newPrefab_);
    if (ImGui::Button("Salvar como novo prefab"))
        status_ = saveAsNewPrefab(prefabs, o, newPrefab_, edited, error) ? std::string("Criado ") + newPrefab_ + ".prefab.json" : error;
    ImGui::SetItemTooltip("Cria <nome>.prefab.json com base = o prefab atual e so os atributos editados que diferem dele.");
}
