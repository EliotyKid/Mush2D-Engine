#include "engine/tools/LookFile.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {
// 6 significant digits keep the file readable (0.2f -> 0.2, not 0.20000000298...).
double r6(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.6g", double(v));
    return std::strtod(b, nullptr);
}
json vec(glm::vec3 v) { return json::array({r6(v.x), r6(v.y), r6(v.z)}); }
json vec(glm::vec2 v) { return json::array({r6(v.x), r6(v.y)}); }

void get(const json& o, const char* key, float& out) {
    auto it = o.find(key);
    if (it != o.end() && it->is_number()) out = it->get<float>();
}
void get(const json& o, const char* key, int& out) {
    auto it = o.find(key);
    if (it != o.end() && it->is_number()) out = it->get<int>();
}
void get(const json& o, const char* key, bool& out) {
    auto it = o.find(key);
    if (it != o.end() && it->is_boolean()) out = it->get<bool>();
}
template <int N>
void get(const json& o, const char* key, glm::vec<N, float>& out) {
    auto it = o.find(key);
    if (it == o.end() || !it->is_array() || it->size() != size_t(N)) return;
    for (int i = 0; i < N; ++i) {
        if (!(*it)[i].is_number()) return;
    }
    for (int i = 0; i < N; ++i) out[i] = (*it)[i].get<float>();
}
const json* section(const json& o, const char* key) {
    auto it = o.find(key);
    return it != o.end() && it->is_object() ? &*it : nullptr;
}
}

std::optional<LookFile> parseLook(const std::string& text, const FrameSettings& defaults) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    LookFile look;
    FrameSettings& s = look.settings = defaults;
    if (auto* a = section(j, "ambient")) { get(*a, "color", s.ambientColor); get(*a, "intensity", s.ambientIntensity); }
    if (auto* f = section(j, "fog")) {
        get(*f, "color", s.fogColor); get(*f, "density", s.fogDensity);
        get(*f, "gradientStart", s.fogGradientStart); get(*f, "gradientEnd", s.fogGradientEnd);
        get(*f, "gradientStrength", s.fogGradientStrength);
    }
    if (auto* b = section(j, "bloom")) { get(*b, "threshold", s.bloomThreshold); get(*b, "knee", s.bloomKnee); get(*b, "intensity", s.bloomIntensity); }
    get(j, "exposure", s.exposure);
    if (auto* g = section(j, "grading")) {
        get(*g, "lift", s.lift); get(*g, "gamma", s.gamma); get(*g, "gain", s.gain);
        get(*g, "saturation", s.saturation); get(*g, "temperature", s.temperature);
    }
    if (auto* p = section(j, "pixelArt")) {
        get(*p, "enabled", s.pixelArt); get(*p, "pixelateLight", s.pixelateLight); get(*p, "pixelateFactor", s.pixelateFactor);
    }
    get(j, "lightHalfRes", s.lightHalfRes);

    if (auto* ls = section(j, "layers")) {
        for (auto& [id, v] : ls->items()) {
            if (!v.is_object()) continue;
            LayerOverride o{{1.0f, 1.0f}, 0.0f};
            get(v, "parallax", o.parallax); get(v, "depth", o.depth);
            look.layers[id] = o;
        }
    }
    if (auto* ls = section(j, "lights")) {
        for (auto& [iid, v] : ls->items()) {
            if (!v.is_object()) continue;
            LightOverride o{{1.0f, 1.0f, 1.0f}, 1.0f, 6.0f, 1.5f, 0.0f, 0.0f};
            get(v, "color", o.color); get(v, "intensity", o.intensity); get(v, "radius", o.radius);
            get(v, "height", o.height); get(v, "depth", o.depth); get(v, "flicker", o.flicker);
            look.lights[iid] = o;
        }
    }
    return look;
}

std::string serializeLook(const LookFile& look) {
    const FrameSettings& s = look.settings;
    json j;
    j["ambient"] = {{"color", vec(s.ambientColor)}, {"intensity", r6(s.ambientIntensity)}};
    j["fog"] = {{"color", vec(s.fogColor)}, {"density", r6(s.fogDensity)}, {"gradientStart", r6(s.fogGradientStart)},
                {"gradientEnd", r6(s.fogGradientEnd)}, {"gradientStrength", r6(s.fogGradientStrength)}};
    j["bloom"] = {{"threshold", r6(s.bloomThreshold)}, {"knee", r6(s.bloomKnee)}, {"intensity", r6(s.bloomIntensity)}};
    j["exposure"] = r6(s.exposure);
    j["grading"] = {{"lift", vec(s.lift)}, {"gamma", vec(s.gamma)}, {"gain", vec(s.gain)},
                    {"saturation", r6(s.saturation)}, {"temperature", r6(s.temperature)}};
    j["pixelArt"] = {{"enabled", s.pixelArt}, {"pixelateLight", s.pixelateLight}, {"pixelateFactor", s.pixelateFactor}};
    j["lightHalfRes"] = s.lightHalfRes;
    j["layers"] = json::object();
    for (auto& [id, o] : look.layers) j["layers"][id] = {{"parallax", vec(o.parallax)}, {"depth", r6(o.depth)}};
    j["lights"] = json::object();
    for (auto& [iid, o] : look.lights) {
        j["lights"][iid] = {{"color", vec(o.color)}, {"intensity", r6(o.intensity)}, {"radius", r6(o.radius)},
                            {"height", r6(o.height)}, {"depth", r6(o.depth)}, {"flicker", r6(o.flicker)}};
    }
    return j.dump(2) + "\n";
}

LookFile loadLookOrDefault(const std::filesystem::path& path, const FrameSettings& defaults) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "Look: %s not found, using defaults\n", path.string().c_str());
    } else {
        std::stringstream ss;
        ss << in.rdbuf();
        if (auto look = parseLook(ss.str(), defaults)) return *look;
        std::fprintf(stderr, "Look: %s is corrupt, using defaults\n", path.string().c_str());
    }
    LookFile look;
    look.settings = defaults;
    return look;
}

void saveLook(const std::filesystem::path& path, const LookFile& look) {
    std::ofstream out(path, std::ios::trunc);
    out << serializeLook(look);
    if (!out) std::fprintf(stderr, "Look: failed to write %s\n", path.string().c_str());
}

void applyLook(const LookFile& look, std::vector<EditableLight>& lights, std::vector<EditableLayer>& layers) {
    for (auto& [iid, o] : look.lights) {
        bool found = false;
        for (EditableLight& l : lights) {
            if (l.iid == iid) { l.value = o; found = true; }
        }
        if (!found) std::fprintf(stderr, "Look: light iid '%s' not in level, ignored\n", iid.c_str());
    }
    for (auto& [id, o] : look.layers) {
        bool found = false;
        for (EditableLayer& l : layers) {
            if (l.identifier == id) { l.value = o; found = true; }
        }
        if (!found) std::fprintf(stderr, "Look: layer '%s' not in level, ignored\n", id.c_str());
    }
}

void captureLook(LookFile& look, const std::vector<EditableLight>& lights, const std::vector<EditableLayer>& layers) {
    look.lights.clear();
    look.layers.clear();
    for (const EditableLight& l : lights) look.lights[l.iid] = l.value;
    for (const EditableLayer& l : layers) look.layers[l.identifier] = l.value;
}
