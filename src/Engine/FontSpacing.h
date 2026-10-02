#pragma once

#include <string>
#include <unordered_map>

// How much room a character takes when text is drawn, read from
// assets/fontSpacing.json. The first of the settings that live outside the
// game: nothing in here is changed while it runs, the file is edited by hand
// and read once at the start.
//
// The file looks like this:
//
//     {
//       "defaultSpacing": 7,
//       "spaceWidth": 4,
//       "iconSpace": 8,
//       "caretSpace": 1,
//       "groups": [
//         {
//           "characters": [",", ".", "\"", ":", ";", "'"],
//           "spacing": 5
//         }
//       ]
//     }
//
// defaultSpacing is what every character takes that no group names, spaceWidth
// belongs to the space alone, and a group gives its characters their own
// width, e.g. two pixels less for the thin ones. A character that stands in
// several groups belongs to the first one.
//
// The numbers are the pixels of normal text. Narrow text, e.g. a list or the
// code editor, takes one pixel less per character, see TextSpacing.
//
// iconSpace is the exception: an icon is a picture that fills its cell to the
// edge, so it keeps its width in narrow text as well.
//
// caretSpace is the gap between the last character in front of a text caret
// (console, editors) and the caret itself, so it does not stick to the letter.
//
// Without the file nothing is set and the font falls back to its own cell
// width, so a missing or broken file never breaks the game.
class FontSpacing {
public:
    // Name of the file in the asset folder
    static constexpr const char *FILE = "fontSpacing.json";

    // Reads the rules, the path relative to the asset folder
    static FontSpacing Load(const std::string &relativePath = FILE);

    // Was a file read? Otherwise everything below is meaningless.
    bool IsLoaded() const;

    int DefaultSpacing() const;

    int SpaceWidth() const;

    // What an icon takes, the same in narrow text. 0 when the file says
    // nothing about it.
    int IconSpace() const;

    // The gap in front of a text caret in pixels, 0 when the file says
    // nothing about it.
    int CaretSpace() const;

    // What a group gives this character, -1 when no group names it
    int SpacingFor(int codepoint) const;

private:
    bool loaded = false;

    int defaultSpacing = 0;
    int spaceWidth = 0;
    int iconSpace = 0;
    int caretSpace = 0;

    // Character -> the width of its group
    std::unordered_map<int, int> characters;
};
