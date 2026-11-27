#include "AtlasLoader.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

// ordered_json keeps the file's frame order, which is what frameTags from/to index.
using json = nlohmann::ordered_json;

TextureAtlas AtlasLoader::loadFromAsepriteJsonHash(
    const std::string& jsonPath,
    const std::string& imagePath
) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Nao foi possivel abrir atlas json: " + jsonPath);
    }

    json j;
    file >> j;

    TextureAtlas atlas{};

    AtlasMeta meta{};
    meta.imagePath = imagePath;

    if (j.contains("meta")) {
        const auto& metaJson = j["meta"];

        if (metaJson.contains("size")) {
            const auto& sizeJson = metaJson["size"];
            meta.imageWidth = sizeJson.value("w", 0);
            meta.imageHeight = sizeJson.value("h", 0);
        }

        if (metaJson.contains("image") && meta.imagePath.empty()) {
            meta.imagePath = metaJson.value("image", "");
        }
    }

    atlas.setMeta(meta);

    if (!j.contains("frames")) {
        throw std::runtime_error("Atlas json invalido: campo 'frames' nao encontrado.");
    }

    const auto& framesJson = j["frames"];
    if (!framesJson.is_object()) {
        throw std::runtime_error("Atlas json invalido: 'frames' deve ser objeto no formato JSON Hash.");
    }

    std::vector<std::string> orderedFrameNames; // file order

    for (auto it = framesJson.begin(); it != framesJson.end(); ++it) {
        const std::string frameName = it.key();
        const auto& frameJson = it.value();

        AtlasFrame frame{};
        frame.name = frameName;

        if (frameJson.contains("frame")) {
            const auto& rect = frameJson["frame"];
            frame.x = rect.value("x", 0);
            frame.y = rect.value("y", 0);
            frame.w = rect.value("w", 0);
            frame.h = rect.value("h", 0);
        }

        frame.rotated = frameJson.value("rotated", false);
        frame.trimmed = frameJson.value("trimmed", false);
        frame.durationMs = frameJson.value("duration", 0);

        if (frameJson.contains("sourceSize")) {
            const auto& sourceSize = frameJson["sourceSize"];
            frame.sourceW = sourceSize.value("w", frame.w);
            frame.sourceH = sourceSize.value("h", frame.h);
        } else {
            frame.sourceW = frame.w;
            frame.sourceH = frame.h;
        }

        if (frameJson.contains("spriteSourceSize")) {
            const auto& spriteSourceSize = frameJson["spriteSourceSize"];
            frame.spriteSourceX = spriteSourceSize.value("x", 0);
            frame.spriteSourceY = spriteSourceSize.value("y", 0);
        }

        // The GameMaker origin, when the art import script in tools/ exported one; otherwise the frame centre.
        frame.pivotX = frame.w / 2;
        frame.pivotY = frame.h / 2;
        if (frameJson.contains("pivot")) {
            frame.pivotX = frameJson["pivot"].value("x", frame.pivotX);
            frame.pivotY = frameJson["pivot"].value("y", frame.pivotY);
        }

        atlas.addFrame(frame);
        orderedFrameNames.push_back(frameName);
    }
    if (j.contains("meta")) {
        const auto& metaJson = j["meta"];

        if (metaJson.contains("frameTags") && metaJson["frameTags"].is_array()) {
            const auto& frameTagsJson = metaJson["frameTags"];

            for (const auto& tagJson : frameTagsJson) {
                AtlasAnimation animation{};
                animation.name = tagJson.value("name", "");
                animation.looping = tagJson.value("direction", "forward") != "pingpong";

                int from = tagJson.value("from", 0);
                int to = tagJson.value("to", -1);

                if (animation.name.empty()) {
                    continue;
                }

                if (from < 0 || to < from || to >= static_cast<int>(orderedFrameNames.size())) {
                    continue;
                }

                for (int i = from; i <= to; ++i) {
                    const std::string& frameName = orderedFrameNames[static_cast<std::size_t>(i)];
                    const AtlasFrame& frame = atlas.getFrame(frameName);

                    animation.frames.push_back(AtlasAnimationFrame{
                        .frameName = frame.name,
                        .durationMs = frame.durationMs
                    });
                }

                atlas.addAnimation(animation);
            }
        }
    }

    return atlas;
}
