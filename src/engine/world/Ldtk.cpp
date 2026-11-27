#include "engine/world/Ldtk.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>

using nlohmann::json;

std::vector<IntRect> mergeIntGrid(const std::vector<int>& csv, int cw, int ch, int solidValue) {
    std::vector<IntRect> out;
    if (cw <= 0 || ch <= 0 || csv.size() < size_t(cw) * size_t(ch)) {
        std::fprintf(stderr, "LDtk: intGridCsv has %zu cells, expected %dx%d; ignored\n", csv.size(), cw, ch);
        return out;
    }
    std::vector<char> used(csv.size(), 0);
    auto avail = [&](int x, int y) { size_t i = size_t(y) * cw + x; return csv[i] == solidValue && !used[i]; };
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            if (!avail(x, y)) continue;
            int w = 1;
            while (x + w < cw && avail(x + w, y)) ++w;
            int h = 1;
            for (; y + h < ch; ++h) {
                bool full = true;
                for (int i = 0; i < w && full; ++i) full = avail(x + i, y + h);
                if (!full) break;
            }
            for (int j = 0; j < h; ++j)
                for (int i = 0; i < w; ++i) used[size_t(y + j) * cw + x + i] = 1;
            out.push_back({x, y, w, h});
        }
    return out;
}

float parseLayerDepth(const std::string& id, int orderFromFront, int frontIndexOfGameplay) {
    size_t p = id.rfind("_d");
    if (p != std::string::npos && p + 2 < id.size()) {
        std::string num = id.substr(p + 2);
        if (num.size() > 1 && num[0] == '_') num[0] = '-'; // LDtk identifiers cannot hold '-': it saves _d_2
        char* end = nullptr;
        float v = std::strtof(num.c_str(), &end);
        if (end && *end == '\0') return v;
    }
    return float(orderFromFront - frontIndexOfGameplay);
}

float ldtkParallaxToScroll(float f) { return 1.0f - f; }

glm::vec2 ldtkParallaxAnchor(glm::vec2 p, glm::vec2 scroll, glm::vec2 c) { return p - (1.0f - scroll) * c; }

namespace {
std::string str(const json& j, const char* k) {
    auto it = j.find(k);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}
glm::ivec2 pair2i(const json& a) { return {a[0].get<int>(), a[1].get<int>()}; }
}

glm::vec2 ldtkToWorld(glm::ivec2 o, glm::vec2 px) { return {(o.x + px.x) / 32.0f, -(o.y + px.y) / 32.0f}; }

std::vector<std::vector<int>> computeNeighbours(const std::vector<LdtkLevel>& lv, int tol) {
    std::vector<std::vector<int>> out(lv.size());
    for (size_t i = 0; i < lv.size(); ++i)
        for (size_t j = 0; j < lv.size(); ++j) {
            if (i == j) continue;
            const LdtkLevel &a = lv[i], &b = lv[j];
            if (b.worldPx.x <= a.worldPx.x + a.pxSize.x + tol && a.worldPx.x <= b.worldPx.x + b.pxSize.x + tol &&
                b.worldPx.y <= a.worldPx.y + a.pxSize.y + tol && a.worldPx.y <= b.worldPx.y + b.pxSize.y + tol)
                out[i].push_back(int(j));
        }
    return out;
}

LdtkProject loadLdtkProject(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) throw std::runtime_error("LDtk: cannot open " + file.string());
    json root = json::parse(in, nullptr, false);
    if (root.is_discarded()) throw std::runtime_error("LDtk: invalid JSON in " + file.string());

    std::map<std::string, glm::vec2> parallax; // layer def identifier -> LDtk factors
    std::map<std::string, std::map<int, std::string>> intNames; // layer def identifier -> {IntGrid value: identifier}
    std::map<int, glm::ivec2> tilesetSize;     // tileset uid -> pixel size
    if (root.contains("defs")) {
        for (auto& d : root["defs"].value("layers", json::array()))
            parallax[str(d, "identifier")] = {d.value("parallaxFactorX", 0.0f), d.value("parallaxFactorY", 0.0f)};
        for (auto& d : root["defs"].value("layers", json::array()))
            for (auto& v : d.value("intGridValues", json::array()))
                if (v.contains("value") && v["value"].is_number_integer() && v.contains("identifier") && v["identifier"].is_string())
                    intNames[str(d, "identifier")][v["value"].get<int>()] = v["identifier"].get<std::string>();
        for (auto& t : root["defs"].value("tilesets", json::array()))
            tilesetSize[t.value("uid", -1)] = {t.value("pxWid", 0), t.value("pxHei", 0)};
    }

    LdtkProject proj;
    proj.baseDir = file.parent_path();
    for (auto& lv : root.value("levels", json::array())) {
        LdtkLevel out;
        out.identifier = str(lv, "identifier");
        out.iid = str(lv, "iid");
        out.pxSize = {lv.value("pxWid", 0), lv.value("pxHei", 0)};
        out.worldPx = {lv.value("worldX", 0), lv.value("worldY", 0)};
        out.baseDir = proj.baseDir;
        for (auto& f : lv.value("fieldInstances", json::array()))
            if (str(f, "__identifier") == "region" && f.contains("__value") && f["__value"].is_string() &&
                !f["__value"].get<std::string>().empty())
                out.region = f["__value"].get<std::string>();

        for (auto& li : lv.value("layerInstances", json::array())) {
            LdtkLayer L;
            L.identifier = str(li, "__identifier");
            L.type = str(li, "__type");
            L.gridSize = li.value("__gridSize", 16);
            L.cells = {li.value("__cWid", 0), li.value("__cHei", 0)};
            L.tilesetPath = str(li, "__tilesetRelPath");
            if (li.contains("__tilesetDefUid") && li["__tilesetDefUid"].is_number_integer()) {
                auto it = tilesetSize.find(li["__tilesetDefUid"].get<int>());
                if (it != tilesetSize.end()) L.tilesetPx = it->second;
            }
            const bool tiled = L.type == "Tiles" || L.type == "AutoLayer";
            if (tiled && (L.tilesetPath.empty() || L.tilesetPx.x <= 0 || L.tilesetPx.y <= 0)) {
                std::fprintf(stderr, "LDtk: layer '%s' has no tileset, skipped\n", L.identifier.c_str());
                continue;
            }
            auto pit = parallax.find(L.identifier);
            glm::vec2 pf = pit != parallax.end() ? pit->second : glm::vec2(0.0f);
            L.parallaxX = ldtkParallaxToScroll(pf.x);
            L.parallaxY = ldtkParallaxToScroll(pf.y);
            // shortcut: parallaxScaling ignored; add per-layer scaling if a layer needs it.
            const glm::ivec2 off{li.value("__pxTotalOffsetX", 0), li.value("__pxTotalOffsetY", 0)};

            if (auto nit = intNames.find(L.identifier); nit != intNames.end()) L.intGridNames = nit->second;
            if (li.contains("intGridCsv")) L.intGrid = li["intGridCsv"].get<std::vector<int>>();
            for (const char* key : {"gridTiles", "autoLayerTiles"})
                for (auto& t : li.value(key, json::array()))
                    L.tiles.push_back({pair2i(t["px"]) + off, pair2i(t["src"]), t.value("f", 0)});
            for (auto& e : li.value("entityInstances", json::array())) {
                LdtkEntity E;
                E.identifier = str(e, "__identifier");
                E.iid = str(e, "iid");
                E.layerGridPx = L.gridSize;
                E.size = {e.value("width", 0.0f), e.value("height", 0.0f)};
                glm::vec2 pivot{0.0f};
                if (e.contains("__pivot")) pivot = {e["__pivot"][0].get<float>(), e["__pivot"][1].get<float>()};
                E.px = glm::vec2(pair2i(e["px"]) + off) - pivot * E.size;
                E.fields = json::object();
                for (auto& f : e.value("fieldInstances", json::array()))
                    E.fields[str(f, "__identifier")] = f.value("__value", json());
                L.entities.push_back(std::move(E));
            }
            out.layers.push_back(std::move(L));
        }

        // gameplay layer = "Collision", else the first layer with a tileset
        int gameplay = -1, firstTiled = -1;
        for (int i = 0; i < int(out.layers.size()); ++i) {
            if (out.layers[i].identifier == "Collision" && gameplay < 0) gameplay = i;
            if (!out.layers[i].tilesetPath.empty() && firstTiled < 0) firstTiled = i;
        }
        if (gameplay < 0) gameplay = firstTiled < 0 ? 0 : firstTiled;
        for (int i = 0; i < int(out.layers.size()); ++i) {
            LdtkLayer& L = out.layers[i];
            L.depth = parseLayerDepth(L.identifier, i, gameplay);
            for (LdtkEntity& e : L.entities) { e.depth = L.depth; e.parallax = {L.parallaxX, L.parallaxY}; }
        }
        proj.levels.push_back(std::move(out));
    }
    return proj;
}

LdtkLevel loadLdtkLevel(const std::filesystem::path& file, const std::string& levelId) {
    for (LdtkLevel& l : loadLdtkProject(file).levels)
        if (l.identifier == levelId) return std::move(l);
    throw std::runtime_error("LDtk: level not found: " + levelId);
}

float numberField(const LdtkEntity& e, const char* key, float fallback) {
    auto it = e.fields.find(key);
    return it != e.fields.end() && it->is_number() ? it->get<float>() : fallback;
}

std::string stringField(const LdtkEntity& e, const char* key, const std::string& fallback) {
    auto it = e.fields.find(key);
    return it != e.fields.end() && it->is_string() ? it->get<std::string>() : fallback;
}

glm::vec3 colorField(const LdtkEntity& e, const char* key, glm::vec3 fallback) {
    std::string hex = stringField(e, key, "");
    unsigned rgb = 0;
    if (hex.size() != 7 || hex[0] != '#' || std::sscanf(hex.c_str() + 1, "%6x", &rgb) != 1) return fallback;
    return glm::vec3((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF) / 255.0f;
}

glm::vec2 bottomOf(const LdtkEntity& e, glm::vec2 centre) {
    return centre - glm::vec2(0.0f, e.size.y / (2.0f * LdtkPixelsPerUnit));
}
