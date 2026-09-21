#include "FontRenderer.h"

#include <algorithm>

#include "Pixels.h"

FontRenderer::FontRenderer(Texture2D &texture, int letterWidth, int letterHeight)
    : texture(&texture),
      letterWidth(letterWidth),
      letterHeight(letterHeight) {
    int rows = texture.height / letterHeight;

    // Letters and digits have one row per color each, the icons lie below them.
    // An older atlas without icons simply does not draw them.
    variantCount = std::min(rows / 2, FontVariant::Count);
    iconVariantCount = std::clamp(rows - 2 * variantCount, 0, ICON_VARIANT_COUNT);
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
    // Every cell has an empty column on the left and the right. Leaving one of
    // them out halves the spacing between the characters.
    return spacing == TextSpacing::Narrow ? letterWidth - 1 : letterWidth;
}

// A space only advances half as far as a letter. With an odd width it
// rounds up, so words do not stick together.
int FontRenderer::CharacterAdvance(int codepoint, TextSpacing spacing) const {
    int advance = Advance(spacing);

    if (IsIcon(codepoint)) {
        return advance + ICON_GAP;
    }

    return codepoint == ' ' ? (advance + 1) / 2 : advance;
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
    // The atlas only knows capital letters
    if (codepoint >= 'a' && codepoint <= 'z') {
        codepoint = codepoint - 'a' + 'A';
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
        // After the Z: Ae, Ue, Oe and sz
        case 0xC4:
        case 0xE4:
            return {26, LETTER_GROUP, true};
        case 0xDC:
        case 0xFC:
            return {27, LETTER_GROUP, true};
        case 0xD6:
        case 0xF6:
            return {28, LETTER_GROUP, true};
        case 0xDF:
            return {29, LETTER_GROUP, true};

        case '0':
            return {9, DIGIT_GROUP, true};
        case ',':
            return {10, DIGIT_GROUP, true};
        case '.':
            return {11, DIGIT_GROUP, true};
        case ':':
            return {12, DIGIT_GROUP, true};
        case '"':
            return {13, DIGIT_GROUP, true};
        case '?':
            return {14, DIGIT_GROUP, true};
        case '!':
            return {15, DIGIT_GROUP, true};
        case '+':
            return {16, DIGIT_GROUP, true};
        case '-':
            return {17, DIGIT_GROUP, true};
        case '*':
            return {18, DIGIT_GROUP, true};
        case '/':
            return {19, DIGIT_GROUP, true};
        case '%':
            return {20, DIGIT_GROUP, true};
        case '=':
            return {21, DIGIT_GROUP, true};
        case '_':
            return {22, DIGIT_GROUP, true};
        case '&':
            return {23, DIGIT_GROUP, true};
        case '(':
            return {24, DIGIT_GROUP, true};
        case ')':
            return {25, DIGIT_GROUP, true};
        case '[':
            return {26, DIGIT_GROUP, true};
        case ']':
            return {27, DIGIT_GROUP, true};
        case ARROW_LEFT:
            return {28, DIGIT_GROUP, true};
        case ARROW_RIGHT:
            return {29, DIGIT_GROUP, true};
        case ARROW_UP:
            return {30, DIGIT_GROUP, true};
        case ARROW_DOWN:
            return {31, DIGIT_GROUP, true};

        default:
            // Everything else, including the space, only advances
            return {0, LETTER_GROUP, false};
    }
}

int FontRenderer::RowFor(const Cell &cell, int variant) const {
    if (cell.group != ICON_GROUP) {
        return cell.group * variantCount + variant;
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

    return iconVariantCount > 0 ? 2 * variantCount + iconRow : -1;
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
