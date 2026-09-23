#include "StudioView.h"

#include <cmath>

#include "MidiFile.h"
#include "SongExport.h"
#include "SongFile.h"
#include "Engine/App.h"
#include "Engine/SaveDialog.h"

// Heights of the bars and the width of the channel list, in canvas pixels.
// The transport holds two rows: the files above, the playing below.
constexpr float TRANSPORT_HEIGHT = 31.0f;
constexpr float STATUS_HEIGHT = 11.0f;
constexpr float CHANNEL_WIDTH = 104.0f;

constexpr float GAP = 3.0f;

constexpr int TEMPO_MIN = 40;
constexpr int TEMPO_MAX = 300;
constexpr int TEMPO_STEP = 5;

StudioView::StudioView(App &app)
    : View(app) {
    // The voices of a classic sound chip, as a start. Every one of them draws
    // its notes in its own colour.
    auto add = [this](const std::string &name, Color colour, Synth::Wave wave) {
        Channel channel;

        channel.name = name;
        channel.colour = colour;
        channel.instrument.wave = wave;

        channels.push_back(std::move(channel));
    };

    add("PULSE 1", Color{0, 249, 255, 255}, Synth::Wave::Square);
    add("PULSE 2", Color{61, 255, 20, 255}, Synth::Wave::Pulse);
    add("TRIANGLE", Color{255, 229, 26, 255}, Synth::Wave::Triangle);
    add("NOISE", Color{188, 190, 202, 255}, Synth::Wave::Noise);
    add("SAMPLE", Color{255, 108, 34, 255}, Synth::Wave::Square);
}

// The name of a file without its folders, for the title
static std::string NameOf(const std::string &path) {
    std::size_t slash = path.find_last_of('/');

    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Writes the whole song, so work can go on later. Only the first time asks
// where it should go, after that it goes there again.
void StudioView::Save() {
    if (songFile.empty()) {
        SaveAs();

        return;
    }

    bool written = SongFile::Save(songFile, channels, tempo);

    report = written ? "SAVED " + NameOf(songFile) : "COULD NOT SAVE";

    if (!written) {
        songFile.clear();
    }
}

// Always asks where: a song can be put down under a second name this way, and
// a song without a file gets one
void StudioView::SaveAs() {
    std::string suggested = songFile.empty() ? std::string("song.") + SongFile::EXTENSION : NameOf(songFile);

    std::string file = AskWhereToSave(suggested, {SongFile::EXTENSION});

    if (file.empty()) {
        return;
    }

    songFile = file;

    Save();
}

// Reads a song of our own, or the notes of a midi file
void StudioView::Open() {
    std::string file = AskWhatToOpen({SongFile::EXTENSION, MidiFile::EXTENSION});

    if (file.empty()) {
        return;
    }

    bool midi = file.size() > 4 && file.compare(file.size() - 4, 4, ".mid") == 0;
    bool read = midi ? MidiFile::Load(file, channels, tempo) : SongFile::Load(file, channels, tempo);

    if (!read) {
        report = "COULD NOT OPEN " + NameOf(file);
        return;
    }

    // Notes from somewhere else have no file of ours yet, so saving asks again
    songFile = midi ? "" : file;

    AfterLoading();

    report = (midi ? "IMPORTED " : "OPENED ") + NameOf(file);
}

// Everything that pointed into the old song starts over
void StudioView::AfterLoading() {
    current = 0;
    currentPattern = 0;
    rollOpen = false;
    playing = false;
    position = 0.0f;

    RestartPlayback();
}

// The whole song, or only the pattern that is open: whatever is on the screen
// is what gets written
void StudioView::Export() {
    bool onlyPattern = rollOpen;

    std::string suggested = onlyPattern
                                ? channels[current].name + " " + std::to_string(currentPattern + 1) + ".wav"
                                : "song.wav";

    std::string file = AskWhereToSave(suggested, {"wav", "mid"});

    if (file.empty()) {
        return;
    }

    std::vector<SongExport::Event> events = onlyPattern
                                                ? SongExport::OnePattern(CurrentPattern())
                                                : SongExport::Song(channels);

    // One pattern belongs to its own channel, so its sound and its name fit
    std::vector<Channel> used = onlyPattern ? std::vector<Channel>{channels[current]} : channels;

    bool written = SongExport::Write(file, events, used, tempo);

    report = written ? "SAVED " + std::to_string(events.size()) + " NOTES" : "NOTHING TO SAVE";
}

Pattern &StudioView::CurrentPattern() {
    Channel &channel = channels[current];

    return channel.patterns[static_cast<std::size_t>(channel.Reserve(currentPattern))];
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
    int voice = synth.Play(asked.pitch, channel.instrument, seconds);

    if (asked.steps == 0) {
        synth.Stop(heldVoice);
        heldVoice = voice;
    }
}

// Every note that begins in a step the playhead has reached is started now.
// The steps are counted, not the places: that way the note in the very first
// step sounds as well.
void StudioView::PlayReachedNotes() {
    const int stepsPerBar = Pattern::STEPS_PER_BEAT * Pattern::BEATS_PER_BAR;

    int reached = static_cast<int>(std::floor(position * static_cast<float>(Pattern::STEPS_PER_BEAT)));

    if (reached <= playedThrough) {
        return;
    }

    for (const Channel &channel: channels) {
        if (channel.muted) {
            continue;
        }

        // A block says from which bar its pattern plays, the notes count from
        // there
        for (int bar = 0; bar < static_cast<int>(channel.bars.size()); bar++) {
            const Pattern *pattern = channel.At(bar);

            if (pattern == nullptr) {
                continue;
            }

            int start = bar * stepsPerBar;

            for (const Note &note: pattern->Notes()) {
                int at = start + note.step;

                if (at > playedThrough && at <= reached) {
                    synth.Play(
                        note.pitch,
                        channel.instrument,
                        static_cast<float>(note.length) * SecondsPerStep(),
                        static_cast<float>(note.velocity) / 100.0f
                    );
                }
            }
        }
    }

    playedThrough = reached;
}

// Nothing before this place counts as played, so the step under the playhead
// is heard again
void StudioView::RestartPlayback() {
    playedThrough = static_cast<int>(std::floor(position * static_cast<float>(Pattern::STEPS_PER_BEAT))) - 1;

    synth.StopAll();
}

void StudioView::Update(float dt) {
    synth.Update();

    // A pattern that was written longer takes the bars behind it, whatever
    // stood there before gives way
    for (Channel &channel: channels) {
        channel.Tidy();
    }

    if (playing) {
        PlayReachedNotes();

        position += dt * static_cast<float>(tempo) / 60.0f;
    }

    // Space starts and stops, like in every other program
    if (IsKeyPressed(KEY_SPACE)) {
        playing = !playing;

        // Starting listens from here on, stopping lets nothing ring on
        RestartPlayback();
    }

    // Control and S save, like everywhere else
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

    if (control && IsKeyPressed(KEY_S)) {
        Save();
    }

    if (control && IsKeyPressed(KEY_E)) {
        Export();
    }

    if (control && IsKeyPressed(KEY_O)) {
        Open();
    }

    // Backspace alone belongs to the roll and deletes, with Shift it rewinds
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (IsKeyPressed(KEY_BACKSPACE) && shift)) {
        position = 0.0f;

        RestartPlayback();
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

    // The list is only as tall as it has rows, the instrument sits below it
    float row = Widgets::RowHeight(app.GetFont());
    float list = row + static_cast<float>(channels.size()) * (row + 1.0f) + 2.0f;

    layout.channels = {GAP, top, CHANNEL_WIDTH, std::min(list, bottom - top)};
    layout.instrument = {
        GAP,
        layout.channels.y + layout.channels.height + GAP,
        CHANNEL_WIDTH,
        std::max(bottom - layout.channels.y - layout.channels.height - GAP, 0.0f)
    };
    layout.middle = {
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
    DrawInstrument(ui, layout.instrument);

    if (rollOpen) {
        DrawPattern(ui, layout.middle);
    } else {
        DrawArrangement(ui, layout.middle);
    }

    DrawStatus(ui, layout.status);
}

// The arrangement: which pattern of a channel plays in which bar
void StudioView::DrawArrangement(Ui &ui, Rectangle bounds) {
    Widgets::Panel(ui, bounds);

    float row = Widgets::RowHeight(ui.font);

    Widgets::Label(ui, {bounds.x + Widgets::PADDING, bounds.y + Widgets::PADDING}, "ARRANGEMENT",
                   ui.theme.mutedVariant);

    Rectangle area{bounds.x + 1.0f, bounds.y + row, bounds.width - 2.0f, bounds.height - row - 1.0f};

    arranger.SetPlayhead(position);
    arranger.Draw(ui, area, channels, clipboard);

    // A double click on a block opens its pattern in the roll
    if (arranger.OpenedChannel() != Arranger::NOTHING) {
        current = static_cast<std::size_t>(arranger.OpenedChannel());
        currentPattern = channels[current].bars[static_cast<std::size_t>(arranger.OpenedBar())];
        rollOpen = true;
    }

    if (arranger.ScrubbedBeats() >= 0.0f) {
        position = arranger.ScrubbedBeats();

        RestartPlayback();
    }
}

// Play, stop and the tempo, plus the name of the song
void StudioView::DrawTransport(Ui &ui, Rectangle bounds) {
    Widgets::Bar(ui, bounds);

    const FontRenderer &font = ui.font;
    float row = Widgets::RowHeight(font);

    // The files above, playing below: two short rows read faster than one
    // long one, and the buttons stay big enough to hit
    float files = bounds.y + 1.0f;
    float y = files + row + 1.0f;
    float x = GAP;

    // Everything about files stands together at the left end: opening,
    // exporting and saving
    float openWidth = Widgets::RowWidth(font, "OPEN");
    float exportWidth = Widgets::RowWidth(font, "EXPORT");

    if (Widgets::Button(ui, {x, files, openWidth, row}, "OPEN")) {
        Open();
    }

    x += openWidth + 2.0f;

    if (Widgets::Button(ui, {x, files, exportWidth, row}, "EXPORT")) {
        Export();
    }

    x += exportWidth + 2.0f;

    // The floppy asks every time where the song should go, Control and S
    // writes it straight away
    if (Widgets::Button(ui, {x, files, row, row}, std::string(1, FontRenderer::ICON_SAVE))) {
        SaveAs();
    }

    // The second row starts at the left again
    x = GAP;

    std::string play(1, playing ? FontRenderer::ICON_PAUSE : FontRenderer::ICON_PLAY);

    if (Widgets::Button(ui, {x, y, row, row}, play)) {
        playing = !playing;
    }

    x += row + 2.0f;

    if (Widgets::Button(ui, {x, y, row, row}, std::string(1, FontRenderer::CROSS[0]))) {
        playing = false;
        position = 0.0f;

        RestartPlayback();
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

    // The name of the song at the right end of the upper row, cyan like the
    // titles of the engine
    std::string title = songFile.empty() ? "UNTITLED SONG" : NameOf(songFile);

    Widgets::Label(
        ui,
        {bounds.width - Widgets::RowWidth(font, title) - GAP, files + Widgets::PADDING},
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
            // A single click shows this channel, a double click opens the roll
            // and closes it again. The first of the two clicks has already
            // switched the channel, so the second one only switches the roll.
            current = i;

            if (ui.doubleClicked) {
                rollOpen = !rollOpen;
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

// How the chosen channel sounds. Everything a note needs is here: the wave it
// is made of, how loud it is and how it comes and goes.
//
// The four times are the ones of every synthesizer, see Synth::Instrument. A
// plucked bass has a short decay and no sustain, an organ neither of both.
void StudioView::DrawInstrument(Ui &ui, Rectangle bounds) {
    if (bounds.height < Widgets::RowHeight(ui.font) * 2.0f) {
        return;
    }

    Widgets::Panel(ui, bounds);

    Synth::Instrument &instrument = channels[current].instrument;

    float row = Widgets::RowHeight(ui.font);

    Widgets::Label(ui, {bounds.x + Widgets::PADDING, bounds.y + Widgets::PADDING}, "INSTRUMENT",
                   ui.theme.mutedVariant);

    float y = bounds.y + row + 1.0f;

    auto place = [&]() {
        Rectangle line{bounds.x + 2.0f, y, bounds.width - 4.0f, row};

        y += row + 1.0f;

        return line;
    };

    // A time grows by a quarter of itself, so the short ones can be set
    // exactly and the long ones are reached in a few clicks
    auto nudge = [](float seconds, int turn) {
        float step = std::max(std::round(seconds * 1000.0f * 0.25f), 5.0f) / 1000.0f;

        return std::clamp(seconds + static_cast<float>(turn) * step, 0.0f, 4.0f);
    };

    auto millis = [](float seconds) {
        return std::to_string(static_cast<int>(std::round(seconds * 1000.0f)));
    };

    auto percent = [](float share) {
        return std::to_string(static_cast<int>(std::round(share * 100.0f)));
    };

    // The wave, by its name
    if (int turn = Widgets::Stepper(ui, place(), "", Synth::WaveName(instrument.wave)); turn != 0) {
        const auto &waves = Synth::WAVES;

        std::size_t at = 0;

        for (std::size_t i = 0; i < waves.size(); i++) {
            if (waves[i] == instrument.wave) {
                at = i;
            }
        }

        instrument.wave = waves[(at + waves.size() + static_cast<std::size_t>(turn)) % waves.size()];

        // Hearing it right away beats reading its name
        synth.Play(69, instrument, 0.25f);
    }

    if (int turn = Widgets::Stepper(ui, place(), "VOL", percent(instrument.volume)); turn != 0) {
        instrument.volume = std::clamp(instrument.volume + static_cast<float>(turn) * 0.05f, 0.0f, 1.0f);
    }

    if (int turn = Widgets::Stepper(ui, place(), "ATT", millis(instrument.attack)); turn != 0) {
        instrument.attack = nudge(instrument.attack, turn);
    }

    if (int turn = Widgets::Stepper(ui, place(), "DEC", millis(instrument.decay)); turn != 0) {
        instrument.decay = nudge(instrument.decay, turn);
    }

    if (int turn = Widgets::Stepper(ui, place(), "SUS", percent(instrument.sustain)); turn != 0) {
        instrument.sustain = std::clamp(instrument.sustain + static_cast<float>(turn) * 0.1f, 0.0f, 1.0f);
    }

    if (int turn = Widgets::Stepper(ui, place(), "REL", millis(instrument.release)); turn != 0) {
        instrument.release = nudge(instrument.release, turn);
    }

    if (int turn = Widgets::Stepper(ui, place(), "VIB", percent(instrument.vibrato)); turn != 0) {
        instrument.vibrato = std::clamp(instrument.vibrato + static_cast<float>(turn) * 0.05f, 0.0f, 2.0f);
    }

    if (int turn = Widgets::Stepper(ui, place(), "SWP", std::to_string(static_cast<int>(instrument.sweep)));
        turn != 0) {
        instrument.sweep = std::clamp(instrument.sweep + static_cast<float>(turn) * 4.0f, -240.0f, 240.0f);
    }
}

// The pattern of the chosen channel, as a piano roll
void StudioView::DrawPattern(Ui &ui, Rectangle bounds) {
    Widgets::Panel(ui, bounds);

    Channel &channel = channels[current];
    Pattern &pattern = CurrentPattern();

    float row = Widgets::RowHeight(ui.font);

    std::string title = channel.name + " " + std::to_string(currentPattern + 1);

    Widgets::Label(
        ui,
        {bounds.x + Widgets::PADDING, bounds.y + Widgets::PADDING},
        channel.muted ? title + "  MUTED" : title,
        ui.theme.mutedVariant
    );

    // How many notes are in it, so an empty pattern is obvious
    std::size_t notes = pattern.Notes().size();
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
    roll.Draw(ui, area, pattern, channel.colour, clipboard);

    PlayPreview(roll.Asked());

    // A click into the ruler of the roll moves the song
    if (roll.ScrubbedBeats() >= 0.0f) {
        position = roll.ScrubbedBeats();

        RestartPlayback();
    }
}

// What the program is doing and which keys are worth knowing
void StudioView::DrawStatus(Ui &ui, Rectangle bounds) {
    Widgets::Bar(ui, bounds);

    std::string state = playing ? "PLAYING" : "STOPPED";
    std::string bar = std::to_string(static_cast<int>(position) / Pattern::BEATS_PER_BAR + 1);
    std::string beat = std::to_string(static_cast<int>(position) % Pattern::BEATS_PER_BAR + 1);

    // After saving its answer stands here instead of the place in the song
    std::string left = report.empty() ? state + "  BAR " + bar + "." + beat : report;

    Widgets::Label(
        ui,
        {GAP, bounds.y + 2.0f},
        left,
        report.empty() ? (playing ? ui.theme.titleVariant : ui.theme.mutedVariant) : ui.theme.titleVariant
    );

    std::string keys = rollOpen
                           ? "DRAG DRAW   CTRL PICK   CTRL C V COPY   BACKSPACE DELETE"
                           : "DRAG BLOCKS   WHEEL PATTERN   CTRL C V COPY   CTRL S SAVE";

    Widgets::Label(
        ui,
        {bounds.width - Widgets::RowWidth(ui.font, keys), bounds.y + 2.0f},
        keys,
        ui.theme.mutedVariant
    );
}
