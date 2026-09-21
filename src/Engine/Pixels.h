#pragma once

#include <cmath>

#include "raylib.h"

// Pixel art must land on whole pixels. A position like 114.37 would be drawn
// between two pixels of the canvas and blur the edges.
inline Vector2 SnapToPixel(Vector2 position) {
    return {
        std::round(position.x),
        std::round(position.y)
    };
}
