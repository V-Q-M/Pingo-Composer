#pragma once

#include <vector>

#include "Channel.h"
#include "Engine/Widgets.h"

// The arrangement of the song: one row per channel, one column per bar.
//
// A block in a cell means that this pattern of the channel plays in that bar.
// How it is used:
//   left button    puts a block into a bar, dragging paints a whole row
//   right button   takes blocks away again
//   wheel          over a block changes which pattern it plays, past the last
//                  one a new empty pattern is made
//   double click   opens the pattern of the block in the piano roll
//
// The arranger only edits the channels it is given, it owns nothing itself
// except where it looks.
class Arranger {
public:
    // Height of a channel row and width of a bar. A row is as high as one in
    // the channel list, so both read as the same thing.
    static constexpr float ROW_HEIGHT = 14.0f;
    static constexpr float BAR_WIDTH = 22.0f;

    // Width of the names on the left and height of the ruler above
    static constexpr float NAMES_WIDTH = 70.0f;
    static constexpr float RULER_HEIGHT = 9.0f;

    // Nothing was asked for
    static constexpr int NOTHING = -1;

    void Draw(Ui &ui, Rectangle bounds, std::vector<Channel> &channels);

    // Where the song stands, in beats
    void SetPlayhead(float beats);

    // The channel whose pattern should open in the roll, NOTHING for none
    int OpenedChannel() const;

    // The bar that was double clicked, so the view can open its pattern
    int OpenedBar() const;

    // Beats the mouse asked for by clicking into the ruler, -1 for none
    float ScrubbedBeats() const;

private:
    struct Grid {
        Rectangle area;
        Rectangle names;
        Rectangle ruler;
    };

    Grid LayoutFor(Rectangle bounds) const;

    int BarAt(const Grid &grid, float x) const;

    int RowAt(const Grid &grid, float y) const;

    float XOf(const Grid &grid, int bar) const;

    float YOf(const Grid &grid, int row) const;

    void DrawNames(Ui &ui, const Grid &grid, const std::vector<Channel> &channels) const;

    void DrawRuler(Ui &ui, const Grid &grid);

    void DrawBlocks(Ui &ui, const Grid &grid, const std::vector<Channel> &channels) const;

    void HandleMouse(Ui &ui, const Grid &grid, std::vector<Channel> &channels);

    // The first bar on the left
    float scroll = 0.0f;

    // The pattern a new block starts with, per channel
    std::vector<int> lastPattern;

    float playhead = 0.0f;

    int openedChannel = NOTHING;
    int openedBar = NOTHING;

    float scrubbed = -1.0f;
};
