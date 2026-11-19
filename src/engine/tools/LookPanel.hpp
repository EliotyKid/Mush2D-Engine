#pragma once

#include "engine/render/GpuTimers.hpp"
#include "engine/tools/LookFile.hpp"
#include "engine/world/Region.hpp"

#include <vector>

// Call between ImGui::NewFrame and Render. Edits go straight into the arguments; the caller applies them.
// region = the current region (its look and background layers); lights and layers = what is loaded now.
class LookPanel {
public:
    void draw(Region& region, std::vector<EditableLight>& lights, std::vector<EditableLayer>& layers,
              const GpuTimers& timers, bool& saveRequested);
};
