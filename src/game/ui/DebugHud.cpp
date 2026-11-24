#include "game/ui/DebugHud.hpp"

#include "engine/world/Resources.hpp"

#include <algorithm>

#include <imgui.h>

void drawDebugHud(const Resources& resources) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    constexpr float Left = 16.0f, Top = 16.0f, Radius = 9.0f, Gap = 6.0f, BarWidth = 140.0f, BarHeight = 10.0f;
    const ImU32 heart = IM_COL32(220, 50, 60, 255);
    for (int i = 0; i < resources.maxHealth; ++i) {
        const ImVec2 centre{Left + Radius + float(i) * (2.0f * Radius + Gap), Top + Radius};
        if (i < resources.health()) draw->AddCircleFilled(centre, Radius, heart);
        else draw->AddCircle(centre, Radius - 1.0f, heart, 0, 2.0f);
    }
    auto bar = [&](float y, float value, float max, ImU32 color) {
        const ImVec2 a{Left, y}, b{Left + BarWidth, y + BarHeight};
        const float fill = max > 0.0f ? std::clamp(value / max, 0.0f, 1.0f) : 0.0f;
        draw->AddRectFilled(a, b, IM_COL32(0, 0, 0, 140));
        draw->AddRectFilled(a, {a.x + BarWidth * fill, b.y}, color);
        draw->AddRect(a, b, IM_COL32(255, 255, 255, 160));
    };
    bar(Top + 2.0f * Radius + 8.0f, resources.energy(Element::Ice), resources.maxIce, IM_COL32(80, 170, 255, 255));
    bar(Top + 2.0f * Radius + 24.0f, resources.energy(Element::Fire), resources.maxFire, IM_COL32(255, 140, 40, 255));
}