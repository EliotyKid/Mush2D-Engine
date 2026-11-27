#pragma once

#include "engine/platform/Actions.hpp"

// F1 "Projeto (F1)" window, section "Projeto · Entrada": the live gamepad limits and the press buffer of every
// input action. Edits apply at once and are not saved (edit project.json to keep them).
// The engine draws it for every game while the F1 panel is open. Call between ImGui::NewFrame and Render.
void drawInputPanel(ActionMap& actions);
