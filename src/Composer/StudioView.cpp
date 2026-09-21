#include "StudioView.h"

#include <cmath>

#include "Engine/App.h"

// Heights of the bars and the width of the channel list, in canvas pixels
constexpr float TRANSPORT_HEIGHT = 16.0f;
constexpr float STATUS_HEIGHT = 11.0f;
constexpr float CHANNEL_WIDTH = 84.0f;

constexpr float GAP = 3.0f;

// One beat of the pattern, and how many of them make a bar
constexpr float BEAT_WIDTH = 16.0f;
constexpr int BEATS_PER_BAR = 4;

// Height of a row in the pattern, one per half step of the scale
constexpr float NOTE_HEIGHT = 5.0f;

constexpr int TEMPO_MIN = 40;
constexpr int TEMPO_MAX = 300;
constexpr int TEMPO_STEP = 5;

StudioView::StudioView(App &app)
    : View(app) {
    // The voices of a classic sound chip, as a start
    channels = {
        {"PULSE 1"},
        {"PULSE 2"},
        {"TRIANGLE"},
        {"NOISE"},
        {"SAMPLE"}
    };
}

void StudioView::Update(float dt) {
    if (playing) {
        position += dt * static_cast<float>(tempo) / 60.0f;
    }

    // Space starts and stops, like in every other program
    if (IsKeyPressed(KEY_SPACE)) {
        playing = !playing;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        position = 0.0f;
    }
}

StudioView::Layout StudioView::LayoutFor(const Ui &) const {
    const float width = static_cast<float>(app.GetScreen().Width());
    const float height = static_cast<float>(app.GetScreen().Height());

    Layout layout;

    layout.transport = {0.0f, 0.0f, width, TRANSPORT_HEIGHT};
    layout.status = {0.0f, height - STATUS_HEIGHT, width, STATUS_HEIGHT};

    float top = layout.transport.height + GAP;
    float bottom = layout.status.y - GAP;

    layout.channels = {GAP, top, CHANNEL_WIDTH, bottom - top};
    layout.pattern = {
        layout.channels.x + layout.channels.width + GAP,
        top,
        width - layout.channels.width - 3.0f * GAP,
        bottom - top
    };

    return layout;
}

void StudioView::Draw(Ui &ui) {
    Layout layout = LayoutFor(ui);

    DrawTransport(ui, layout.transport);
    DrawChannels(ui, layout.channels);
    DrawPattern(ui, layout.pattern);
    DrawStatus(ui, layout.status);
}

// Play, stop and the tempo, plus the name of the song
void StudioView::DrawTransport(Ui &ui, Rectangle bounds) {
    Widgets::Bar(ui, bounds);

    const FontRenderer &font = ui.font;
    float row = Widgets::RowHeight(font);
    float y = bounds.y + (bounds.height - row) / 2.0f;
    float x = GAP;

    std::string play(1, playing ? FontRenderer::ICON_PAUSE : FontRenderer::ICON_PLAY);

    if (Widgets::Button(ui, {x, y, row, row}, play)) {
        playing = !playing;
    }

    x += row + 2.0f;

    if (Widgets::Button(ui, {x, y, row, row}, std::string(1, FontRenderer::CROSS[0]))) {
        playing = false;
        position = 0.0f;
    }

    // Tempo with a minus and a plus next to the number
    x += row + GAP * 2.0f;

    if (Widgets::Button(ui, {x, y, row, row}, "-")) {
        tempo = std::max(tempo - TEMPO_STEP, TEMPO_MIN);
    }

    x += row + 1.0f;

    std::string beats = std::to_string(tempo) + " BPM";
    float width = Widgets::RowWidth(font, "300 BPM");

    Widgets::Sunken(ui, {x, y, width, row});
    Widgets::CenteredLabel(ui, {x, y, width, row}, beats, ui.theme.textVariant);

    x += width + 1.0f;

    if (Widgets::Button(ui, {x, y, row, row}, "+")) {
        tempo = std::min(tempo + TEMPO_STEP, TEMPO_MAX);
    }

    // The name of the song on the right, cyan like the titles of the engine
    std::string title = "UNTITLED SONG";

    Widgets::Label(
        ui,
        {bounds.width - Widgets::RowWidth(font, title), y + Widgets::PADDING},
        title,
        ui.theme.titleVariant
    );
}

// One row per channel: its name, and whether it is muted
void StudioView::DrawChannels(Ui &ui, Rectangle bounds) {
    Widgets::Panel(ui, bounds);

    const FontRenderer &font = ui.font;
    float row = Widgets::RowHeight(font);
    float mute = row;

    Widgets::Label(ui, {bounds.x + Widgets::PADDING, bounds.y + Widgets::PADDING}, "CHANNELS", ui.theme.mutedVariant);

    float y = bounds.y + row + 1.0f;

    for (std::size_t i = 0; i < channels.size(); i++) {
        Channel &channel = channels[i];

        Rectangle name{bounds.x + 1.0f, y, bounds.width - mute - 3.0f, row};
        Rectangle button{name.x + name.width + 1.0f, y, mute, row};

        bool hovered = Widgets::Hovered(ui, name);
        bool chosen = i == current;

        if (chosen) {
            DrawRectangleRec(name, ui.theme.sunken);
        }

        DrawRectangleLinesEx(name, 1.0f, chosen ? ui.theme.active : (hovered ? ui.theme.highlight : BLANK));

        Widgets::Label(
            ui,
            {name.x + Widgets::PADDING, name.y + Widgets::PADDING},
            channel.name,
            channel.muted ? ui.theme.mutedVariant : (chosen ? ui.theme.titleVariant : ui.theme.textVariant)
        );

        if (hovered && ui.clicked) {
            current = i;
        }

        // The speaker says whether the channel is heard
        std::string sound(1, channel.muted ? FontRenderer::ICON_SOUND_OFF : FontRenderer::ICON_SOUND);

        if (Widgets::Toggle(ui, button, sound, !channel.muted)) {
            channel.muted = !channel.muted;
        }

        y += row + 1.0f;
    }
}

// The pattern of the chosen channel: for now the grid it will be written in
void StudioView::DrawPattern(Ui &ui, Rectangle bounds) {
    Widgets::Panel(ui, bounds);

    const FontRenderer &font = ui.font;
    float row = Widgets::RowHeight(font);

    Widgets::Label(
        ui,
        {bounds.x + Widgets::PADDING, bounds.y + Widgets::PADDING},
        channels[current].name + " PATTERN",
        ui.theme.mutedVariant
    );

    Rectangle grid{
        bounds.x + 1.0f,
        bounds.y + row,
        bounds.width - 2.0f,
        bounds.height - row - 1.0f
    };

    Widgets::Sunken(ui, grid);

    BeginScissorMode(
        static_cast<int>(grid.x),
        static_cast<int>(grid.y),
        static_cast<int>(grid.width),
        static_cast<int>(grid.height)
    );

    // Rows for the notes, a brighter line every octave
    for (int note = 0; grid.y + static_cast<float>(note) * NOTE_HEIGHT < grid.y + grid.height; note++) {
        float y = grid.y + static_cast<float>(note) * NOTE_HEIGHT;

        DrawRectangle(
            static_cast<int>(grid.x),
            static_cast<int>(y),
            static_cast<int>(grid.width),
            1,
            note % 12 == 0 ? ui.theme.gridAccent : ui.theme.grid
        );
    }

    // Columns for the beats, a brighter line at every bar
    for (int beat = 0; grid.x + static_cast<float>(beat) * BEAT_WIDTH < grid.x + grid.width; beat++) {
        float x = grid.x + static_cast<float>(beat) * BEAT_WIDTH;

        DrawRectangle(
            static_cast<int>(x),
            static_cast<int>(grid.y),
            1,
            static_cast<int>(grid.height),
            beat % BEATS_PER_BAR == 0 ? ui.theme.gridAccent : ui.theme.grid
        );

        if (beat % BEATS_PER_BAR == 0) {
            Widgets::Label(
                ui,
                {x + 2.0f, grid.y + 2.0f},
                std::to_string(beat / BEATS_PER_BAR + 1),
                ui.theme.mutedVariant
            );
        }
    }

    // Where the song stands right now
    float playhead = grid.x + std::fmod(position, grid.width / BEAT_WIDTH) * BEAT_WIDTH;

    DrawRectangle(
        static_cast<int>(playhead),
        static_cast<int>(grid.y),
        1,
        static_cast<int>(grid.height),
        ui.theme.highlight
    );

    EndScissorMode();
}

// What the program is doing and which keys are worth knowing
void StudioView::DrawStatus(Ui &ui, Rectangle bounds) {
    Widgets::Bar(ui, bounds);

    std::string state = playing ? "PLAYING" : "STOPPED";
    std::string bar = std::to_string(static_cast<int>(position) / BEATS_PER_BAR + 1);
    std::string beat = std::to_string(static_cast<int>(position) % BEATS_PER_BAR + 1);

    Widgets::Label(
        ui,
        {GAP, bounds.y + 2.0f},
        state + "  BAR " + bar + "." + beat,
        playing ? ui.theme.titleVariant : ui.theme.mutedVariant
    );

    std::string keys = "SPACE PLAY   ENTER REWIND   CTRL +- ZOOM " + std::to_string(app.GetScreen().Scale()) + "X";

    Widgets::Label(
        ui,
        {bounds.width - Widgets::RowWidth(ui.font, keys), bounds.y + 2.0f},
        keys,
        ui.theme.mutedVariant
    );
}
