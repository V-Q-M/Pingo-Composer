#pragma once

#include "raylib.h"

// The pixel canvas everything is drawn on.
//
// The program is drawn at a fixed pixel size: the canvas is the window divided
// by the scale, so a larger window shows more of the program instead of larger
// pixels. That is what a tool needs, unlike a game with a fixed resolution.
//
// The scale says how many window pixels one drawn pixel takes, 2 by default.
// Everything drawn between BeginDraw and EndDraw uses canvas coordinates, and
// MousePosition reports the mouse in them as well.
class Screen {
public:
    static constexpr int MIN_SCALE = 1;
    static constexpr int MAX_SCALE = 6;

    explicit Screen(int scale = 2);

    ~Screen();

    Screen(const Screen &) = delete;

    Screen &operator=(const Screen &) = delete;

    int Width() const;

    int Height() const;

    int Scale() const;

    // Larger pixels show less of the program. Values outside the limits are
    // cut off, the canvas follows at the next frame.
    void SetScale(int scale);

    // The mouse in canvas pixels, whole numbers
    Vector2 MousePosition() const;

    // Canvas as large as the window allows. Belongs at the start of a frame,
    // before anything is drawn.
    void Follow();

    void BeginDraw(Color background);

    // Draws the canvas into the window, scaled up by whole pixels
    void EndDraw();

private:
    void Resize(int width, int height);

    RenderTexture2D canvas{};

    int scale;
};
