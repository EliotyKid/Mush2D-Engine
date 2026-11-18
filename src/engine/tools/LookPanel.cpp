#include "engine/tools/LookPanel.hpp"

#include <imgui.h>

#include <string>

void LookPanel::draw(LookFile& look, std::vector<EditableLight>& lights, std::vector<EditableLayer>& layers,
                     const GpuTimers& timers, bool& saveRequested) {
    FrameSettings& s = look.settings;
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({380, 620}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Look (F1)")) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Salvar")) saveRequested = true;

    if (ImGui::CollapsingHeader("Ambiente", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("cor##amb", &s.ambientColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::SliderFloat("intensidade##amb", &s.ambientIntensity, 0.0f, 3.0f);
    }
    if (ImGui::CollapsingHeader("Nevoa")) {
        ImGui::ColorEdit3("cor##fog", &s.fogColor.x, ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("densidade", &s.fogDensity, 0.0f, 0.5f, "%.4f");
        ImGui::SliderFloat("gradiente inicio", &s.fogGradientStart, -20.0f, 40.0f);
        ImGui::SliderFloat("gradiente fim", &s.fogGradientEnd, -20.0f, 40.0f);
        ImGui::SliderFloat("gradiente forca", &s.fogGradientStrength, 0.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Bloom e exposicao")) {
        ImGui::SliderFloat("limiar", &s.bloomThreshold, 0.0f, 4.0f);
        ImGui::SliderFloat("joelho", &s.bloomKnee, 0.0f, 1.0f);
        ImGui::SliderFloat("intensidade##bloom", &s.bloomIntensity, 0.0f, 3.0f);
        ImGui::SliderFloat("exposicao", &s.exposure, 0.1f, 4.0f);
    }
    if (ImGui::CollapsingHeader("Grading")) {
        ImGui::SliderFloat3("lift", &s.lift.x, -0.3f, 0.3f);
        ImGui::SliderFloat3("gamma", &s.gamma.x, 0.4f, 2.5f);
        ImGui::SliderFloat3("gain", &s.gain.x, 0.0f, 2.5f);
        ImGui::SliderFloat("saturacao", &s.saturation, 0.0f, 2.5f);
        ImGui::SliderFloat("temperatura", &s.temperature, -1.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Pixel art")) {
        ImGui::Combo("modo", &s.pixelMode, "pixel art (nitido)\0pixel art (suave)\0HD\0");
        ImGui::Checkbox("pixelar luz", &s.pixelateLight);
        ImGui::SliderInt("fator", &s.pixelateFactor, 1, 8);
        ImGui::Checkbox("luz em meia resolucao", &s.lightHalfRes);
    }
    if (ImGui::CollapsingHeader("Camadas")) {
        for (EditableLayer& l : layers) {
            ImGui::PushID(l.identifier.c_str());
            ImGui::TextUnformatted(l.identifier.c_str());
            ImGui::SliderFloat2("parallax", &l.value.parallax.x, 0.0f, 1.5f);
            ImGui::SliderFloat("profundidade", &l.value.depth, -5.0f, 10.0f);
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Luzes")) {
        for (EditableLight& l : lights) {
            ImGui::PushID(l.iid.c_str());
            if (ImGui::TreeNode("light", "%s (%.1f, %.1f)", l.iid.substr(0, 8).c_str(), l.pos.x, l.pos.y)) {
                ImGui::ColorEdit3("cor", &l.value.color.x, ImGuiColorEditFlags_Float);
                ImGui::SliderFloat("intensidade", &l.value.intensity, 0.0f, 10.0f);
                ImGui::SliderFloat("raio", &l.value.radius, 0.1f, 20.0f);
                ImGui::SliderFloat("altura", &l.value.height, 0.0f, 5.0f);
                ImGui::SliderFloat("profundidade", &l.value.depth, -5.0f, 10.0f);
                ImGui::SliderFloat("flicker", &l.value.flicker, 0.0f, 1.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Desempenho", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%.0f fps (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        for (const auto& [name, ms] : timers.lastFrameMs()) ImGui::Text("%-10s %.3f ms", name.c_str(), ms);
    }
    ImGui::End();
}
