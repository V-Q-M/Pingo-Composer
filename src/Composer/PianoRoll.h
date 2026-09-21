#pragma once

#include <cstddef>

#include "Pattern.h"
#include "Engine/Widgets.h"

// The piano roll: the notes of a pattern on a grid, written with the mouse.
//
// How it is used, all without switching tools:
//   left button    on a free cell draws a note. Keeping it held and moving
//                  right makes the note longer right away.
//   left button    on a note moves it, on its right edge it changes its length
//   right button   erases every note it is dragged over
//   wheel          scrolls through the pitches, with Shift through the time,
//                  with Control it zooms the time
//   Delete         removes the note that was touched last
//
// The roll keeps where it looks (scrolling and zoom), the notes belong to the
// pattern. One roll can therefore show one channel after another.
class PianoRoll {
public:
    // Width of the keys on the left and height of the ruler at the top
    static constexpr float KEYS_WIDTH = 25.0f;
    static constexpr float RULER_HEIGHT = 9.0f;

    // Height of one half step and the width of one step, in pixels. A row is
    // as high as a letter, so the names of the keys fit into it.
    static constexpr float PITCH_HEIGHT = 8.0f;

    static constexpr float MIN_STEP_WIDTH = 3.0f;
    static constexpr float MAX_STEP_WIDTH = 24.0f;

    // Inside this many pixels of its right edge a note is made longer instead
    // of moved
    static constexpr float EDGE_WIDTH = 3.0f;

    // Draws the roll into the bounds and lets the mouse edit the pattern.
    // colour is the colour of the channel the pattern belongs to.
    void Draw(Ui &ui, Rectangle bounds, Pattern &pattern, Color colour);

    // Where the song stands, in beats. The roll draws the line and scrolls
    // along while it is playing.
    void SetPlayhead(float beats, bool following);

    // Beats the mouse asked for by clicking into the ruler, -1 for none.
    // The view above decides what happens, e.g. jumping there.
    float ScrubbedBeats() const;

private:
    // What the left button is doing right now
    enum class Drag {
        None,

        // A new note, its length follows the mouse
        Create,

        Move,
        Resize
    };

    struct Grid {
        // The cells, without keys and ruler
        Rectangle area;

        Rectangle keys;
        Rectangle ruler;
    };

    Grid LayoutFor(Rectangle bounds) const;

    // The cell under a point. Outside the grid the values still make sense,
    // so dragging beyond the edge keeps working.
    int StepAt(const Grid &grid, float x) const;

    int PitchAt(const Grid &grid, float y) const;

    // Where a cell sits on the screen
    float XOf(const Grid &grid, float step) const;

    float YOf(const Grid &grid, int pitch) const;

    void DrawKeys(Ui &ui, const Grid &grid) const;

    void DrawRuler(Ui &ui, const Grid &grid);

    void DrawCells(const Grid &grid) const;

    void DrawNotes(Ui &ui, const Grid &grid, const Pattern &pattern, Color colour) const;

    void DrawPlayhead(Ui &ui, const Grid &grid) const;

    void HandleMouse(Ui &ui, const Grid &grid, Pattern &pattern);

    void HandleWheel(Ui &ui, const Grid &grid);

    // Keeps the view inside the pattern and the pitch range
    void LimitView(const Grid &grid);

    // A removed note moves the notes behind it one place forward. Whatever
    // the roll remembers has to follow, otherwise Delete would hit the wrong
    // note afterwards.
    void Forget(std::size_t removed);

    // How far the view is scrolled: the first step on the left and the pitch
    // in the top row
    float scroll = 0.0f;
    int topPitch = 84;

    float stepWidth = 8.0f;

    Drag drag = Drag::None;
    std::size_t dragNote = Pattern::NOTHING;

    // Where the note sat when the drag started, and where it was grabbed
    Note dragStart;
    int grabStep = 0;
    int grabPitch = 0;

    // The note the mouse touched last, e.g. for Delete
    std::size_t touched = Pattern::NOTHING;

    float playhead = 0.0f;
    bool following = false;

    float scrubbed = -1.0f;
};
