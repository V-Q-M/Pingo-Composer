#pragma once

// Spacing between two characters when drawing
enum class TextSpacing {
    // One column less than the cell is wide, so one pixel of space between
    // two letters
    Normal,

    // One column narrower still, the letters almost touch
    Narrow
};
