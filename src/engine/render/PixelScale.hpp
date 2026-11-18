#pragma once

#include <algorithm>

// Pixel art: largest integer N (1 art pixel = NxN screen pixels) that still shows >= 400 art pixels across.
inline int computePixelScale(int viewportWidth) {
    return std::max(1, viewportWidth / 400);
}
