#include "Widgets.h"

#include "Pixels.h"

float Widgets::RowHeight(const FontRenderer &font) {
    return static_cast<float>(font.LetterHeight()) + 2.0f * PADDING;
}

float Widgets::RowWidth(const FontRenderer &font, const std::string &text) {
    return static_cast<float>(font.Measure(text, TextSpacing::Narrow)) + 2.0f * PADDING;
}

bool Widgets::Hovered(Ui &ui, Rectangle bounds) {
    if (!CheckCollisionPointRec(ui.mouse, bounds)) {
        return false;
    }

    ui.hovering = true;

    return true;
}

void Widgets::Panel(Ui &ui, Rectangle bounds) {
    DrawRectangleRec(bounds, ui.theme.surface);
    DrawRectangleLinesEx(bounds, 1.0f, ui.theme.surfaceBorder);
}

void Widgets::Sunken(Ui &ui, Rectangle bounds) {
    DrawRectangleRec(bounds, ui.theme.sunken);
    DrawRectangleLinesEx(bounds, 1.0f, ui.theme.sunkenBorder);
}

void Widgets::Bar(Ui &ui, Rectangle bounds) {
    DrawRectangleRec(bounds, ui.theme.bar);

    // Only a line towards the program, so bars read as edges of the window
    DrawRectangle(
        static_cast<int>(bounds.x),
        static_cast<int>(bounds.y + (bounds.y > 0 ? 0 : bounds.height - 1)),
        static_cast<int>(bounds.width),
        1,
        ui.theme.surfaceBorder
    );
}

void Widgets::Label(Ui &ui, Vector2 position, const std::string &text, int variant) {
    ui.font.Draw(text, SnapToPixel(position), variant, TextSpacing::Narrow);
}

void Widgets::CenteredLabel(Ui &ui, Rectangle bounds, const std::string &text, int variant) {
    float width = static_cast<float>(ui.font.Measure(text, TextSpacing::Narrow));
    float height = static_cast<float>(ui.font.LetterHeight());

    Label(ui, {bounds.x + (bounds.width - width) / 2.0f, bounds.y + (bounds.height - height) / 2.0f}, text, variant);
}

bool Widgets::Button(Ui &ui, Rectangle bounds, const std::string &text) {
    bool hovered = Hovered(ui, bounds);

    DrawRectangleRec(bounds, ui.theme.sunken);
    DrawRectangleLinesEx(bounds, 1.0f, hovered ? ui.theme.highlight : ui.theme.surfaceBorder);

    CenteredLabel(ui, bounds, text, hovered ? ui.theme.hoverVariant : ui.theme.textVariant);

    return hovered && ui.clicked;
}

bool Widgets::Toggle(Ui &ui, Rectangle bounds, const std::string &text, bool on) {
    bool hovered = Hovered(ui, bounds);

    DrawRectangleRec(bounds, on ? ui.theme.surface : ui.theme.sunken);
    DrawRectangleLinesEx(bounds, 1.0f, hovered ? ui.theme.highlight : (on ? ui.theme.active : ui.theme.surfaceBorder));

    int variant = hovered ? ui.theme.hoverVariant : (on ? ui.theme.titleVariant : ui.theme.mutedVariant);

    CenteredLabel(ui, bounds, text, variant);

    return hovered && ui.clicked;
}
