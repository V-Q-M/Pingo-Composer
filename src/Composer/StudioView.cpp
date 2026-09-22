#include "StudioView.h"

#include <cmath>

#include "Engine/App.h"

// Heights of the bars and the width of the channel list, in canvas pixels
constexpr float TRANSPORT_HEIGHT = 16.0f;
constexpr float STATUS_HEIGHT = 11.0f;
constexpr float CHANNEL_WIDTH = 84.0f;

constexpr float GAP = 3.0f;

constexpr int TEMPO_MIN = 40;
constexpr int TEMPO_MAX = 300;
constexpr int TEMPO_STEP = 5;

StudioView::StudioView(App &app)
    : View(app) {
    // The voices of a classic sound chip, as a start. Every one of them draws
    // its notes in its own colour.
    channels.push_back({"PULSE 1", Color{0, 249, 255, 255}, Synth::Wave::Square, false, {}});
    channels.push_back({"PULSE 2", Color{61, 255, 20, 255}, Synth::Wave::Pulse, false, {}});
    channels.push_back({"TRIANGLE", Color{255, 229, 26, 255}, Synth::Wave::Triangle, false, {}});
    channels.push_back({"NOISE", Color{188, 190, 202, 255}, Synth::Wave::Noise, false, {}});
    channels.push_back({"SAMPLE", Color{255, 108, 34, 255}, Synth::Wave::Square, false, {}});
}

float StudioView::SecondsPerStep() const {
    return 60.0f / static_cast<float>(tempo) / static_cast<float>(Pattern::STEPS_PER_BEAT);
}

// What the roll asked for: a key that is held sounds until it is let go, a
// written note for as long as it is
void StudioView::PlayPreview(const PianoRoll::Preview &asked) {
    const Channel &channel = channels[current];

    if (asked.stop) {
        synth.Stop(heldVoice);
        heldVoice = Synth::NO_VOICE;
    }

    if (asked.pitch == 0 || channel.muted) {
        return;
    }

    // A note with a length plays on its own, a held key needs to be let go
    float seconds = static_cast<float>(asked.steps) * SecondsPerStep();
    int voice = synth.Play(asked.pitch, channel.wave, seconds);

    if (asked.steps == 0) {
        synth.Stop(heldVoice);
        heldVoice = voice;
    }
}

// Every note that starts between the two places is started now
void StudioView::PlayPassedNotes(float from, float to) {
    int first = static_cast<int>(std::floor(from * static_cast<float>(Pattern::STEPS_PER_BEAT)));
    int last = static_cast<int>(std::floor(to * static_cast<float>(Pattern::STEPS_PER_BEAT)));

    if (last <= first) {
        return;
    }

    for (const Channel &channel: channels) {
        if (channel.muted) {
            continue;
        }

        for (const Note &note: channel.pattern.Notes()) {
            if (note.step > first && note.step <= last) {
                synth.Play(note.pitch, channel.wave, static_cast<float>(note.length) * SecondsPerStep());
            }
        }
    }
}

void StudioView::Update(float dt) {
    synth.Update();

    if (playing) {
        float before = position;

        position += dt * static_cast<float>(tempo) / 60.0f;

        PlayPassedNotes(before, position);
    }

    // Space starts and stops, like in every other program
    if (IsKeyPressed(KEY_SPACE)) {
        playing = !playing;

        // Nothing keeps ringing after the stop
        if (!playing) {
            synth.StopAll();
        }
    }

    // Backspace rewinds. With Shift it belongs to the roll and deletes a note.
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (IsKeyPressed(KEY_BACKSPACE) && !shift)) {
        position = 0.0f;

        // The note before the jump does not belong to the new place
        synth.StopAll();
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

    if (rollOpen) {
        DrawPattern(ui, layout.pattern);
    } else {
        DrawEmpty(ui, layout.pattern);
    }

    DrawStatus(ui, layout.status);
}

// Nothing is open: only the hint how a pattern is opened
void StudioView::DrawEmpty(Ui &ui, Rectangle bounds) {
    Widgets::Panel(ui, bounds);

    Widgets::CenteredLabel(ui, bounds, "DOUBLE CLICK A CHANNEL", ui.theme.mutedVariant);
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

        synth.StopAll();
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

            // A double click opens the roll of this channel
            if (ui.doubleClicked) {
                rollOpen = true;
            }
        }

        // The speaker says whether the channel is heard
        std::string sound(1, channel.muted ? FontRenderer::ICON_SOUND_OFF : FontRenderer::ICON_SOUND);

        if (Widgets::Toggle(ui, button, sound, !channel.muted)) {
            channel.muted = !channel.muted;
        }

        y += row + 1.0f;
    }
}

// The pattern of the chosen channel, as a piano roll
void StudioView::DrawPattern(Ui &ui, Rectangle bounds) {
    Widgets::Panel(ui, bounds);

    Channel &channel = channels[current];
    float row = Widgets::RowHeight(ui.font);

    Widgets::Label(
        ui,
        {bounds.x + Widgets::PADDING, bounds.y + Widgets::PADDING},
        channel.name + (channel.muted ? " PATTERN  MUTED" : " PATTERN"),
        ui.theme.mutedVariant
    );

    // How many notes are in it, so an empty pattern is obvious
    std::size_t notes = channel.pattern.Notes().size();
    std::string count = std::to_string(notes) + (notes == 1 ? " NOTE" : " NOTES");

    Widgets::Label(
        ui,
        {bounds.x + bounds.width - Widgets::RowWidth(ui.font, count), bounds.y + Widgets::PADDING},
        count,
        ui.theme.mutedVariant
    );

    Rectangle area{
        bounds.x + 1.0f,
        bounds.y + row,
        bounds.width - 2.0f,
        bounds.height - row - 1.0f
    };

    roll.SetPlayhead(position, playing);
    roll.Draw(ui, area, channel.pattern, channel.colour);

    PlayPreview(roll.Asked());

    // A double click inside the roll closes it again
    if (roll.ClosingAsked()) {
        rollOpen = false;
    }

    // A click into the ruler of the roll moves the song
    if (roll.ScrubbedBeats() >= 0.0f) {
        position = roll.ScrubbedBeats();
    }
}

// What the program is doing and which keys are worth knowing
void StudioView::DrawStatus(Ui &ui, Rectangle bounds) {
    Widgets::Bar(ui, bounds);

    std::string state = playing ? "PLAYING" : "STOPPED";
    std::string bar = std::to_string(static_cast<int>(position) / Pattern::BEATS_PER_BAR + 1);
    std::string beat = std::to_string(static_cast<int>(position) % Pattern::BEATS_PER_BAR + 1);

    Widgets::Label(
        ui,
        {GAP, bounds.y + 2.0f},
        state + "  BAR " + bar + "." + beat,
        playing ? ui.theme.titleVariant : ui.theme.mutedVariant
    );

    std::string keys = rollOpen
                           ? "DRAG DRAW   RIGHT ERASE   ARROWS EDIT   DOUBLE CLICK CLOSE   SPACE PLAY"
                           : "DOUBLE CLICK A CHANNEL   BACKSPACE REWIND   SPACE PLAY";

    Widgets::Label(
        ui,
        {bounds.width - Widgets::RowWidth(ui.font, keys), bounds.y + 2.0f},
        keys,
        ui.theme.mutedVariant
    );
}
