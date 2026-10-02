#include "FontSpacing.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "FontRenderer.h"

using json = nlohmann::json;

// The smallest width that still makes sense: a character has to move the
// cursor, otherwise text would be drawn on top of itself
constexpr int SPACING_MIN = 1;

FontSpacing FontSpacing::Load(const std::string &relativePath) {
    FontSpacing spacing;

    std::ifstream file("assets/" + relativePath);

    if (!file) {
        return spacing;
    }

    // A file broken by hand editing falls back to the font's own width
    // instead of taking the game down with it
    try {
        json j = json::parse(file);

        spacing.defaultSpacing = std::max(j.value("defaultSpacing", 0), SPACING_MIN);
        spacing.spaceWidth = std::max(j.value("spaceWidth", 0), SPACING_MIN);

        // Without the field the font falls back to its own gap behind an icon
        spacing.iconSpace = std::max(j.value("iconSpace", 0), 0);

        spacing.caretSpace = std::max(j.value("caretSpace", 0), 0);

        for (const json &group: j.value("groups", json::array())) {
            int width = std::max(group.value("spacing", spacing.defaultSpacing), SPACING_MIN);

            for (const json &character: group.value("characters", json::array())) {
                int codepoint = FontRenderer::FirstCodepoint(character.get<std::string>());

                if (codepoint > 0) {
                    spacing.characters.emplace(codepoint, width);
                }
            }
        }
    } catch (const json::exception &) {
        return FontSpacing{};
    }

    spacing.loaded = true;

    return spacing;
}

bool FontSpacing::IsLoaded() const {
    return loaded;
}

int FontSpacing::DefaultSpacing() const {
    return defaultSpacing;
}

int FontSpacing::SpaceWidth() const {
    return spaceWidth;
}

int FontSpacing::IconSpace() const {
    return iconSpace;
}

int FontSpacing::CaretSpace() const {
    return caretSpace;
}

int FontSpacing::SpacingFor(int codepoint) const {
    auto it = characters.find(codepoint);

    return it == characters.end() ? -1 : it->second;
}
