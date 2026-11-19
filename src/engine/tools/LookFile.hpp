#pragma once

#include "engine/render/SceneRenderer.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct LightOverride { glm::vec3 color; float intensity, radius, height, depth, flicker; };
struct LayerOverride { glm::vec2 parallax; float depth; };

struct LookFile {
    FrameSettings settings;
    std::map<std::string, LayerOverride> layers;   // by LDtk layer identifier
    std::map<std::string, LightOverride> lights;   // by LDtk entity iid
};

// Missing keys keep `defaults`; unknown keys are ignored. nullopt if the text is not a JSON object.
std::optional<LookFile> parseLook(const std::string& json, const FrameSettings& defaults);
std::string serializeLook(const LookFile&);

// Live-editable state the panel works on (what the level actually contains right now).
struct EditableLight { std::string iid; glm::vec2 pos; LightOverride value; std::string roomIid; };
struct EditableLayer { std::string identifier; LayerOverride value; };

// Copies overrides onto the editable lists; an iid or layer no longer in the level is skipped with a warning.
void applyLook(const LookFile&, std::vector<EditableLight>&, std::vector<EditableLayer>&);
// Inverse: look.lights / look.layers become exactly the editable lists.
void captureLook(LookFile&, const std::vector<EditableLight>&, const std::vector<EditableLayer>&);
