#pragma once

#include <cstddef>
#include <string>

#include "raylib.h"

#include "TextSpacing.h"

// The color rows of the atlas, from top to bottom. Whoever adds a color or
// changes the order only adjusts this list.
struct FontVariant {
    static constexpr int White = 0;
    static constexpr int Grey = 1;
    static constexpr int Red = 2;
    static constexpr int Yellow = 3;
    static constexpr int Cyan = 4;
    static constexpr int Green = 5;

    static constexpr int Count = 6;

    // Base color of a row, e.g. for color swatches in the editor
    static Color Swatch(int variant);

    // Name in files, e.g. "cyan"
    static const char *Name(int variant);

    // The variant for the name, -1 if it does not exist
    static int FromName(const std::string &name);
};

// Draws text from a font atlas with a fixed cell size.
//
// Layout of the atlas: the characters are arranged in columns in their natural
// order, 'A' is column 0, so 'C' is letterWidth * 2. The letters are at the top,
// digits and punctuation below, both in all FontVariant colors. The icons
// follow at the very bottom, they only exist in white, grey and yellow. In
// other colors they are drawn white.
//
// Text is UTF-8. Besides ASCII the font knows Ae, Oe, Ue and sz, lowercase
// letters are drawn as capitals.
class FontRenderer {
public:
    // Arrows from the atlas. Usable as characters in a string, e.g.
    // std::string(1, FontRenderer::ARROW_LEFT)
    static constexpr char ARROW_LEFT = '\x01';
    static constexpr char ARROW_RIGHT = '\x02';
    static constexpr char ARROW_UP = '\x03';
    static constexpr char ARROW_DOWN = '\x04';

    // Icons from the bottom group, usable as characters the same way
    static constexpr char ICON_LOCK = '\x10';
    static constexpr char ICON_UNLOCK = '\x11';
    static constexpr char ICON_SOUND = '\x12';
    static constexpr char ICON_SOUND_OFF = '\x13';
    static constexpr char ICON_MUSIC = '\x14';
    static constexpr char ICON_MUSIC_OFF = '\x15';
    static constexpr char ICON_SEARCH = '\x16';
    static constexpr char ICON_POINTER = '\x17';
    static constexpr char ICON_GRAB = '\x18';
    static constexpr char ICON_DELETE = '\x19';
    static constexpr char ICON_EDIT = '\x1A';
    static constexpr char ICON_DUPLICATE = '\x1B';

    // Three dots, e.g. for shortened texts
    static constexpr char ICON_ELLIPSIS = '\x1C';

    // Play, pause and a floppy disk for saving
    static constexpr char ICON_PLAY = '\x1D';
    static constexpr char ICON_PAUSE = '\x1E';
    static constexpr char ICON_SAVE = '\x1F';

    // The atlas draws the asterisk as a small cross, e.g. for a checked box or a
    // factor like "x2"
    static constexpr const char *CROSS = "*";

    // Without an atlas: draws nothing until e.g. a theme sets a font
    FontRenderer() = default;

    FontRenderer(Texture2D &texture, int letterWidth, int letterHeight);

    // scale enlarges by whole numbers: every pixel of the font becomes a
    // scale x scale block and so stays on the pixel grid.
    void Draw(const std::string &text,
              Vector2 position,
              int variant = 0,
              TextSpacing spacing = TextSpacing::Normal,
              int scale = 1) const;

    // Width the text takes up when drawn
    int Measure(const std::string &text,
                TextSpacing spacing = TextSpacing::Normal,
                int scale = 1) const;

    // How many pixels the cursor advances per letter, without scaling.
    // Spaces only advance half as far, icons a little further: text follows
    // them without a space.
    int Advance(TextSpacing spacing) const;

    // Can the font draw this character? The space counts, control characters
    // like arrows and icons do not: they are not typed.
    static bool CanDraw(int codepoint);

    // The character as UTF-8, e.g. for typed umlauts
    static std::string Encode(int codepoint);

    // Number of characters, not bytes
    static std::size_t GlyphCount(const std::string &text);

    // Removes the last character, even if it consists of several bytes
    static void RemoveLastGlyph(std::string &text);

    int LetterWidth() const;

    int LetterHeight() const;

    // Number of colors the atlas provides for letters and digits
    int VariantCount() const;

private:
    // Which group of atlas rows a character is in
    static constexpr int LETTER_GROUP = 0;
    static constexpr int DIGIT_GROUP = 1;
    static constexpr int ICON_GROUP = 2;

    // White, grey and yellow
    static constexpr int ICON_VARIANT_COUNT = 3;

    // Space after an icon, about half a space
    static constexpr int ICON_GAP = 2;

    struct Cell {
        int column;
        int group;
        bool printable;
    };

    static Cell CellFor(int codepoint);

    static bool IsIcon(int codepoint);

    // Reads the character at index and moves index past it. -1 for broken UTF-8.
    static int NextGlyph(const std::string &text, std::size_t &index);

    // The atlas row for a cell in this color, -1 if the atlas does not have it
    int RowFor(const Cell &cell, int variant) const;

    int CharacterAdvance(int codepoint, TextSpacing spacing) const;

    // Pointer instead of reference: this way the font can be swapped without
    // menus losing their reference to the FontRenderer
    Texture2D *texture = nullptr;

    int letterWidth = 0;
    int letterHeight = 0;
    int variantCount = 0;
    int iconVariantCount = 0;
};
