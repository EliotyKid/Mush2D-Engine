#pragma once

class Resources;

// Test HUD until the D9 UI (spec D2 "HUD de teste"): 5 hearts (filled or empty circles), a blue ice bar and an
// orange fire bar in the top-left corner, on ImGui's foreground draw list. Call inside an ImGui frame.
void drawDebugHud(const Resources& resources);
