#pragma once

#include <vector>

#include "Channel.h"
#include "Clipboard.h"
#include "Engine/Widgets.h"

// The arrangement of the song: one row per channel, one column per bar.
//
// A block in a cell means that this pattern of the channel plays in that bar.
// How it is used:
//   left button    puts a block into a bar, dragging paints a whole row
//   right button   takes blocks away again
//   wheel          over a block changes which pattern it plays, past the last
//                  one a new empty pattern is made. It takes a good turn per
//                  step: a song needs a handful of patterns, not fifty.
//   1 to 9         over a block says its number straight away
//   sideways       scrolls through the song, as does the wheel next to a block
//   Control + left picks single blocks, one after another
//   Delete         takes the picked blocks out of the song
//   Control C, V   copies the notes of a block and writes them into the bar
//                  under the mouse, as a pattern of its own
//   double click   opens the pattern of the block in the piano roll
//
// A block is as wide as its pattern is long: a pattern written over four bars
// takes four bars of the song and covers them, so nothing else starts inside
// it.
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

    // How far the wheel has to be turned for the next pattern number
    static constexpr float WHEEL_RESISTANCE = 6.0f;

    void Draw(Ui &ui, Rectangle bounds, std::vector<Channel> &channels, NoteClipboard &clipboard);

    // Where the song stands, in beats
    void SetPlayhead(float beats);

    // The channel whose pattern should open in the roll, NOTHING for none
    int OpenedChannel() const;

    // The bar that was double clicked, so the view can open its pattern
    int OpenedBar() const;

    // Beats the mouse asked for by clicking into the ruler, -1 for none
    float ScrubbedBeats() const;

private:
    // One cell of the grid: the channel and the bar it sits in
    struct Block {
        int row = 0;
        int bar = 0;

        bool operator==(const Block &other) const;
    };

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

    // Copies a whole pattern and writes it again, see the comment above
    void HandleClipboard(Ui &ui, const Grid &grid, std::vector<Channel> &channels, NoteClipboard &clipboard);

    bool IsChosen(Block block) const;

    // Puts a pattern into a bar, see the comment in the source
    static void Place(Channel &channel, int bar, int pattern);

    // The first bar on the left
    float scroll = 0.0f;

    // The pattern a new block starts with, per channel
    std::vector<int> lastPattern;

    // How far the wheel has been turned over the block it stands on, see
    // WHEEL_RESISTANCE. It starts over on another block.
    float turned = 0.0f;

    Block turning{NOTHING, NOTHING};

    // The blocks that were picked with Control, usually none
    std::vector<Block> chosen;

    float playhead = 0.0f;

    int openedChannel = NOTHING;
    int openedBar = NOTHING;

    float scrubbed = -1.0f;
};
