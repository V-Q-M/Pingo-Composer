#include "FontRenderer.h"

#include <algorithm>

#include "Pixels.h"

FontRenderer::FontRenderer(Texture2D &texture, int letterWidth, int letterHeight)
    : texture(&texture),
      letterWidth(letterWidth),
      letterHeight(letterHeight) {
    int rows = texture.height / letterHeight;

    // Every group has one row per color, the icons follow at the bottom. How
    // many groups the atlas has comes from its height, so an older one with
    // fewer of them still works.
    variantCount = std::min(rows / 2, FontVariant::Count);
    colouredGroups = std::max((rows - ICON_VARIANT_COUNT) / std::max(variantCount, 1), 1);

    iconVariantCount = std::clamp(rows - colouredGroups * variantCount, 0, ICON_VARIANT_COUNT);
}

int FontRenderer::CaretSpace() const {
    return spacingRules.CaretSpace();
}

int FontRenderer::LetterWidth() const {
    return letterWidth;
}

int FontRenderer::LetterHeight() const {
    return letterHeight;
}

int FontRenderer::VariantCount() const {
    return variantCount;
}

int FontRenderer::Advance(TextSpacing spacing) const {
    int narrower = spacing == TextSpacing::Narrow ? NARROW_GAP : 0;

    if (spacingRules.IsLoaded()) {
        return std::max(spacingRules.DefaultSpacing() - narrower, 1);
    }

    // Without the file: a cell is wider than the letter in it, so one of the
    // empty columns is left out and the letters stand as they were drawn
    return letterWidth - 1 - narrower;
}

void FontRenderer::SetSpacing(FontSpacing rules) {
    spacingRules = std::move(rules);
}

// A space only advances half as far as a letter. With an odd width it
// rounds up, so words do not stick together.
int FontRenderer::CharacterAdvance(int codepoint, TextSpacing spacing) const {
    int advance = Advance(spacing);
    int narrower = spacing == TextSpacing::Narrow ? NARROW_GAP : 0;

    // An icon fills its cell to the edge, so it keeps its width even in
    // narrow text. Text follows it without a space of its own.
    if (IsIcon(codepoint)) {
        return spacingRules.IconSpace() > 0 ? spacingRules.IconSpace() : advance + ICON_GAP;
    }

    if (codepoint == ' ') {
        return spacingRules.IsLoaded()
                   ? std::max(spacingRules.SpaceWidth() - narrower, 1)
                   : (advance + 1) / 2;
    }

    // What a group of fontSpacing.json gives this character, e.g. less for a
    // comma, which would otherwise tear a hole into the text
    int rule = spacingRules.SpacingFor(codepoint);

    return rule < 0 ? advance : std::max(rule - narrower, 1);
}

int FontRenderer::Measure(const std::string &text, TextSpacing spacing, int scale) const {
    int width = 0;

    for (std::size_t index = 0; index < text.size();) {
        width += CharacterAdvance(NextGlyph(text, index), spacing);
    }

    return width * std::max(scale, 1);
}

// Measured in the atlas, the most common bright color of each row
static const Color VARIANT_SWATCHES[FontVariant::Count] = {
    {255, 255, 255, 255},
    {166, 166, 166, 255},
    {255, 44, 34, 255},
    {255, 229, 26, 255},
    {0, 249, 255, 255},
    {61, 255, 20, 255}
};

static const char *VARIANT_NAMES[FontVariant::Count] = {
    "white",
    "grey",
    "red",
    "yellow",
    "cyan",
    "green"
};

Color FontVariant::Swatch(int variant) {
    return variant >= 0 && variant < Count ? VARIANT_SWATCHES[variant] : WHITE;
}

const char *FontVariant::Name(int variant) {
    return variant >= 0 && variant < Count ? VARIANT_NAMES[variant] : "";
}

int FontVariant::FromName(const std::string &name) {
    for (int variant = 0; variant < Count; variant++) {
        if (name == VARIANT_NAMES[variant]) {
            return variant;
        }
    }

    return -1;
}

bool FontRenderer::CanDraw(int codepoint) {
    // Control characters like arrows and icons are not typed
    return codepoint == ' ' || (codepoint > ' ' && CellFor(codepoint).printable);
}

std::string FontRenderer::Encode(int codepoint) {
    if (codepoint < 0) {
        return "";
    }

    if (codepoint < 0x80) {
        return std::string(1, static_cast<char>(codepoint));
    }

    // The font knows nothing beyond two bytes
    if (codepoint < 0x800) {
        return {
            static_cast<char>(0xC0 | (codepoint >> 6)),
            static_cast<char>(0x80 | (codepoint & 0x3F))
        };
    }

    return "";
}

int FontRenderer::FirstCodepoint(const std::string &text) {
    std::size_t index = 0;

    return text.empty() ? 0 : NextGlyph(text, index);
}

std::size_t FontRenderer::GlyphCount(const std::string &text) {
    std::size_t count = 0;

    for (std::size_t index = 0; index < text.size(); count++) {
        NextGlyph(text, index);
    }

    return count;
}

void FontRenderer::RemoveLastGlyph(std::string &text) {
    // Continuation bytes have the form 10xxxxxx
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }

    if (!text.empty()) {
        text.pop_back();
    }
}

int FontRenderer::NextGlyph(const std::string &text, std::size_t &index) {
    unsigned char first = static_cast<unsigned char>(text[index]);

    index++;

    if (first < 0x80) {
        return first;
    }

    auto isContinuation = [&](std::size_t at) {
        return at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80;
    };

    // Two bytes: 110xxxxx 10xxxxxx
    if ((first & 0xE0) == 0xC0 && isContinuation(index)) {
        int codepoint = ((first & 0x1F) << 6) | (static_cast<unsigned char>(text[index]) & 0x3F);

        index++;

        return codepoint;
    }

    // Longer or broken sequences count as one unknown character
    while (isContinuation(index)) {
        index++;
    }

    return -1;
}

bool FontRenderer::IsIcon(int codepoint) {
    return codepoint >= ICON_LOCK && codepoint <= ICON_SAVE;
}

FontRenderer::Cell FontRenderer::CellFor(int codepoint) {
    // Both rows hold the letters in the same order, so a small letter sits in
    // the same column as its capital
    if (codepoint >= 'a' && codepoint <= 'z') {
        return {codepoint - 'a', LOWER_GROUP, true};
    }

    if (codepoint >= 'A' && codepoint <= 'Z') {
        return {codepoint - 'A', LETTER_GROUP, true};
    }

    // The icons are in the order of their characters
    if (IsIcon(codepoint)) {
        return {codepoint - ICON_LOCK, ICON_GROUP, true};
    }

    // The digit row starts at 1, the 0 comes after the 9
    if (codepoint >= '1' && codepoint <= '9') {
        return {codepoint - '1', DIGIT_GROUP, true};
    }

    switch (codepoint) {
        // After the Z: Ae, Ue, Oe and sz, in both rows
        case 0xC4:
            return {26, LETTER_GROUP, true};
        case 0xE4:
            return {26, LOWER_GROUP, true};
        case 0xDC:
            return {27, LETTER_GROUP, true};
        case 0xFC:
            return {27, LOWER_GROUP, true};
        case 0xD6:
            return {28, LETTER_GROUP, true};
        case 0xF6:
            return {28, LOWER_GROUP, true};
        case 0xDF:
            return {29, LOWER_GROUP, true};

        // The digit row: the numbers, then what a calculation and a line of
        // code are written with, and the four arrows at its end
        case '0':
            return {9, DIGIT_GROUP, true};
        case '#':
            return {10, DIGIT_GROUP, true};
        case '+':
            return {11, DIGIT_GROUP, true};
        case '-':
            return {12, DIGIT_GROUP, true};
        case '*':
            return {13, DIGIT_GROUP, true};
        case '/':
            return {14, DIGIT_GROUP, true};
        case '%':
            return {15, DIGIT_GROUP, true};
        case '=':
            return {16, DIGIT_GROUP, true};
        case '&':
            return {17, DIGIT_GROUP, true};
        case '?':
            return {18, DIGIT_GROUP, true};
        case '!':
            return {19, DIGIT_GROUP, true};
        case '_':
            return {20, DIGIT_GROUP, true};
        case ARROW_LEFT:
            return {21, DIGIT_GROUP, true};
        case ARROW_RIGHT:
            return {22, DIGIT_GROUP, true};
        case ARROW_UP:
            return {23, DIGIT_GROUP, true};
        case ARROW_DOWN:
            return {24, DIGIT_GROUP, true};

        // The characters with a width of their own, see fontSpacing.json:
        // they share one row, in this order
        case ',':
            return {0, SPECIAL_GROUP, true};
        case '.':
            return {1, SPECIAL_GROUP, true};
        case ':':
            return {2, SPECIAL_GROUP, true};
        case ';':
            return {3, SPECIAL_GROUP, true};
        case '\'':
            return {4, SPECIAL_GROUP, true};
        case '"':
            return {5, SPECIAL_GROUP, true};
        case '(':
            return {6, SPECIAL_GROUP, true};
        case ')':
            return {7, SPECIAL_GROUP, true};
        case '[':
            return {8, SPECIAL_GROUP, true};
        case ']':
            return {9, SPECIAL_GROUP, true};
        case '{':
            return {10, SPECIAL_GROUP, true};
        case '}':
            return {11, SPECIAL_GROUP, true};
        case '<':
            return {12, SPECIAL_GROUP, true};
        case '>':
            return {13, SPECIAL_GROUP, true};
        case '\\':
            return {14, SPECIAL_GROUP, true};
        case '|':
            return {15, SPECIAL_GROUP, true};
        case '~':
            return {16, SPECIAL_GROUP, true};
        case '^':
            return {17, SPECIAL_GROUP, true};

        default:
            // Everything else, including the space, only advances
            return {0, LETTER_GROUP, false};
    }
}

int FontRenderer::RowFor(const Cell &cell, int variant) const {
    if (cell.group != ICON_GROUP) {
        int group = cell.group;

        if (group >= colouredGroups) {
            // An atlas without lowercase letters draws capitals instead: both
            // rows hold the letters in the same order. Its digits sit in the
            // group before the icons. Special characters it does not have at
            // all, so they stay empty.
            if (group == LOWER_GROUP) {
                group = LETTER_GROUP;
            } else if (group == DIGIT_GROUP) {
                group = colouredGroups - 1;
            } else {
                return -1;
            }
        }

        return group * variantCount + variant;
    }

    // The icon rows are white, grey and yellow. All other colors draw white.
    int iconRow = 0;

    if (variant == FontVariant::Grey) {
        iconRow = 1;
    } else if (variant == FontVariant::Yellow) {
        iconRow = 2;
    }

    if (iconRow >= iconVariantCount) {
        iconRow = 0;
    }

    return iconVariantCount > 0 ? colouredGroups * variantCount + iconRow : -1;
}

void FontRenderer::Draw(const std::string &text,
                        Vector2 position,
                        int variant,
                        TextSpacing spacing,
                        int scale) const {
    if (texture == nullptr) {
        return;
    }

    if (variant < 0 || variant >= variantCount) {
        variant = 0;
    }

    scale = std::max(scale, 1);

    Vector2 cursor = SnapToPixel(position);

    for (std::size_t index = 0; index < text.size();) {
        int codepoint = NextGlyph(text, index);

        Cell cell = CellFor(codepoint);
        int row = cell.printable ? RowFor(cell, variant) : -1;

        if (row >= 0) {
            Rectangle source = {
                static_cast<float>(cell.column * letterWidth),
                static_cast<float>(row * letterHeight),
                static_cast<float>(letterWidth),
                static_cast<float>(letterHeight)
            };

            Rectangle destination = {
                cursor.x,
                cursor.y,
                static_cast<float>(letterWidth * scale),
                static_cast<float>(letterHeight * scale)
            };

            DrawTexturePro(*texture, source, destination, {0.0f, 0.0f}, 0.0f, WHITE);
        }

        // The spacing grows too, otherwise large letters stick together
        cursor.x += static_cast<float>(CharacterAdvance(codepoint, spacing) * scale);
    }
}
