#pragma once

#include "raylib.h"

#include "FontRenderer.h"

// Colors and font variants of the whole program, in the look of the Pingo
// engine menus: a dark ground, light borders and yellow for whatever the
// mouse or the keyboard is on right now.
struct Theme {
    // Behind everything
    Color background{18, 16, 28, 255};

    // Panels lie on the background, a sunken box lies inside a panel, e.g. a
    // text field or the track area
    Color surface{24, 20, 37, 255};
    Color surfaceBorder{255, 255, 255, 90};
    Color sunken{0, 0, 0, 110};
    Color sunkenBorder{255, 255, 255, 60};

    // The bar at the top and the status line at the bottom
    Color bar{32, 28, 48, 255};

    // What the mouse is on, and what is switched on
    Color highlight{255, 229, 26, 255};
    Color active{0, 249, 255, 255};

    // Lines of a grid, e.g. between the beats
    Color grid{255, 255, 255, 26};
    Color gridAccent{255, 255, 255, 60};

    int textVariant = FontVariant::White;
    int mutedVariant = FontVariant::Grey;
    int hoverVariant = FontVariant::Yellow;
    int titleVariant = FontVariant::Cyan;
};
