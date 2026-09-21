#pragma once

#include <string>

#include "raylib.h"

#include "FontRenderer.h"
#include "Theme.h"

// The building blocks every view of the program draws with.
//
// They work in the moment: a widget is drawn and answers right away what the
// mouse did with it, there is no list of widgets to keep in sync with the
// program. A view therefore reads like what it shows:
//
//     if (Widgets::Button(ui, {4, 4, 40, 11}, "PLAY")) {
//         player.Start();
//     }
//
// Everything works in canvas pixels, see Screen.
struct Ui {
    const FontRenderer &font;
    const Theme &theme;

    // Where the mouse is and what it did in this frame
    Vector2 mouse{0.0f, 0.0f};

    // Just pressed, held and just let go of the left button
    bool clicked = false;
    bool down = false;
    bool released = false;

    // The right button erases where the left one draws
    bool rightClicked = false;
    bool rightDown = false;

    // Mouse wheel of this frame, positive is up
    float wheel = 0.0f;

    // Is a modifier held? Shift and Control change what dragging and the wheel
    // do, e.g. scrolling sideways instead of up.
    bool shift = false;
    bool control = false;

    // Set by the widgets: was the mouse over one of them?
    bool hovering = false;
};

class Widgets {
public:
    // Space between a border and the text inside it
    static constexpr float PADDING = 3.0f;

    // Height of a row with one line of text, e.g. a button
    static float RowHeight(const FontRenderer &font);

    // Width a text needs in a row, including the padding on both sides
    static float RowWidth(const FontRenderer &font, const std::string &text);

    // A panel: the surface other things sit on
    static void Panel(Ui &ui, Rectangle bounds);

    // A box inside a panel, e.g. the area of a track
    static void Sunken(Ui &ui, Rectangle bounds);

    // A bar across the window, e.g. at the top or at the bottom
    static void Bar(Ui &ui, Rectangle bounds);

    // Text at a position, the row's padding is already in it
    static void Label(Ui &ui, Vector2 position, const std::string &text, int variant);

    // Text inside an area, centered
    static void CenteredLabel(Ui &ui, Rectangle bounds, const std::string &text, int variant);

    // A button with a border. true in the frame it was clicked in.
    static bool Button(Ui &ui, Rectangle bounds, const std::string &text);

    // A button that shows whether it is on, e.g. the loop of a track. Clicking
    // it reports true, switching it is up to the caller.
    static bool Toggle(Ui &ui, Rectangle bounds, const std::string &text, bool on);

    // Is the mouse inside this area? Also remembers it in the Ui, so the
    // program knows whether the mouse is on anything at all.
    static bool Hovered(Ui &ui, Rectangle bounds);
};
