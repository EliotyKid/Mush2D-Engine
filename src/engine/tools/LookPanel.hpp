#pragma once

#include "engine/render/GpuTimers.hpp"
#include "engine/tools/LookFile.hpp"

#include <vector>

// Call between ImGui::NewFrame and Render. Edits go straight into the arguments; the caller applies them.
class LookPanel {
public:
    void draw(LookFile& look, std::vector<EditableLight>& lights, std::vector<EditableLayer>& layers,
              const GpuTimers& timers, bool& saveRequested);
};
