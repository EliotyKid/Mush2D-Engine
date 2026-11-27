#include "engine/tools/InputPanel.hpp"

#include <imgui.h>

#include <string>

void drawInputPanel(ActionMap& actions) {
    // Placed right of the "Cena (F1)" window the first time; ImGui remembers where the user moves it.
    ImGui::SetNextWindowPos({790, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({320, 260}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Projeto (F1)")) { // collapsed: draw nothing inside
        ImGui::End();
        return;
    }
    if (ImGui::CollapsingHeader("Projeto · Entrada", ImGuiTreeNodeFlags_DefaultOpen)) { // live values, not saved
        ImGui::SliderFloat("triggerThreshold", &actions.settings().triggerThreshold, 0.0f, 1.0f);
        ImGui::SliderFloat("stickDeadzone", &actions.settings().stickDeadzone, 0.0f, 1.0f);
        for (auto& [name, binding] : actions.bindings()) { // one buffer slider per action
            ImGui::PushID(name.c_str()); // the action name keeps each slider's ImGui id unique
            ImGui::SliderFloat((name + " buffer (s)").c_str(), &binding.buffer, 0.0f, 0.5f);
            ImGui::PopID();
        }
    }
    ImGui::End();
}
