#pragma once

#include <string>
#include <vector>

struct AtlasFrame {
    std::string name;

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    int sourceW = 0;
    int sourceH = 0;

    int spriteSourceX = 0;
    int spriteSourceY = 0;

    int durationMs = 0;

    bool rotated = false;
    bool trimmed = false;
};

struct AtlasAnimationFrame {
    std::string frameName;
    int durationMs = 0;
};

struct AtlasAnimation {
    std::string name;
    std::vector<AtlasAnimationFrame> frames;
    bool looping = true;
};

struct AtlasMeta {
    std::string imagePath;
    int imageWidth = 0;
    int imageHeight = 0;
};