#pragma once

// Spacing between two characters when drawing
enum class TextSpacing {
    // Full cell width. The empty border columns of two neighboring cells add up
    // to two pixels of space.
    Normal,

    // One column narrower, so only one pixel of space
    Narrow
};
