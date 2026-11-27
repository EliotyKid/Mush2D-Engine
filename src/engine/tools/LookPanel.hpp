#pragma once

#include "engine/audio/Audio.hpp"
#include "engine/graphics/Camera2D.hpp"
#include "engine/platform/Actions.hpp"
#include "engine/render/GpuTimers.hpp"
#include "engine/tools/LookFile.hpp"
#include "engine/world/Region.hpp"

#include <vector>

// Call between ImGui::NewFrame and Render. Edits go straight into the arguments; the caller applies them.
// region = the current region (its look and background layers); lights and layers = what is loaded now.
// camera and audio: their juice numbers (maxShake, volumes), not saved in the look.
// actions: the live input limits and press buffers ("Projeto - Entrada"), not saved in project.json.
class LookPanel {
public:
    void draw(Region& region, std::vector<EditableLight>& lights, std::vector<EditableLayer>& layers,
              const GpuTimers& timers, Camera2D& camera, Audio& audio, ActionMap& actions, bool& saveRequested);
};
