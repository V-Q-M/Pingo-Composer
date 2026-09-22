#include "PianoRoll.h"

#include <algorithm>
#include <cmath>

// Colors of the roll. The keys stay dark like the rest of the program, only
// their brightness tells the white keys from the black ones.
constexpr Color ROLL_WHITE_KEY{72, 70, 92, 255};
constexpr Color ROLL_BLACK_KEY{28, 26, 42, 255};
constexpr Color ROLL_KEY_BORDER{18, 16, 28, 255};

// A key the mouse is on is a little brighter
constexpr Color ROLL_WHITE_KEY_HOVER{116, 114, 140, 255};
constexpr Color ROLL_BLACK_KEY_HOVER{60, 58, 80, 255};

// How much of the width a black key takes
constexpr float BLACK_KEY_SHARE = 0.6f;

// The rows of the black keys are a little darker than the rest
constexpr Color ROLL_ROW_SHARP{0, 0, 0, 40};

// The row of the note that is being written or moved lights up a little
constexpr Color ROLL_ROW_LIT{255, 255, 255, 28};

// The line of a bar is brighter than the one of a beat
constexpr Color ROLL_BEAT{255, 255, 255, 20};
constexpr Color ROLL_BAR{255, 255, 255, 60};

constexpr Color NOTE_BORDER{18, 16, 28, 255};

// How much darker a quiet note is drawn
constexpr float NOTE_MIN_SHADE = 0.45f;

// Steps the wheel scrolls, and half steps
constexpr float SCROLL_STEPS = 2.0f;
constexpr int SCROLL_PITCHES = 3;

constexpr float ZOOM_STEP = 1.25f;

// The scrollbars, in the same colors as the forms of the engine
constexpr Color SCROLL_TRACK{0, 0, 0, 120};
constexpr Color SCROLL_THUMB{255, 255, 255, 150};

// However little is visible, this much thumb stays to grab
constexpr float SCROLLBAR_MIN_THUMB = 8.0f;

// The window that chooses notes
constexpr Color SELECTION_FILL{255, 229, 26, 40};

static Color Shade(Color colour, float factor) {
    return {
        static_cast<unsigned char>(static_cast<float>(colour.r) * factor),
        static_cast<unsigned char>(static_cast<float>(colour.g) * factor),
        static_cast<unsigned char>(static_cast<float>(colour.b) * factor),
        colour.a
    };
}

void PianoRoll::SetPlayhead(float beats, bool isFollowing) {
    playhead = beats;
    following = isFollowing;
}

float PianoRoll::ScrubbedBeats() const {
    return scrubbed;
}

const PianoRoll::Preview &PianoRoll::Asked() const {
    return asked;
}

PianoRoll::Grid PianoRoll::LayoutFor(Rectangle bounds) const {
    Grid grid;

    float width = bounds.width - KEYS_WIDTH - SCROLLBAR_SIZE;
    float height = bounds.height - RULER_HEIGHT - SCROLLBAR_SIZE;

    grid.ruler = {bounds.x + KEYS_WIDTH, bounds.y, width, RULER_HEIGHT};
    grid.keys = {bounds.x, bounds.y + RULER_HEIGHT, KEYS_WIDTH, height};
    grid.area = {bounds.x + KEYS_WIDTH, bounds.y + RULER_HEIGHT, width, height};

    grid.timeBar = {grid.area.x, grid.area.y + height, width, SCROLLBAR_SIZE};
    grid.pitchBar = {grid.area.x + width, grid.area.y, SCROLLBAR_SIZE, height};

    return grid;
}

float PianoRoll::VisibleSteps(const Grid &grid) const {
    return grid.area.width / stepWidth;
}

float PianoRoll::VisibleRows(const Grid &grid) const {
    return grid.area.height / PITCH_HEIGHT;
}

int PianoRoll::StepAt(const Grid &grid, float x) const {
    return static_cast<int>(std::floor((x - grid.area.x) / stepWidth + scroll));
}

int PianoRoll::PitchAt(const Grid &grid, float y) const {
    return topPitch - static_cast<int>(std::floor((y - grid.area.y) / PITCH_HEIGHT));
}

float PianoRoll::XOf(const Grid &grid, float step) const {
    return grid.area.x + (step - scroll) * stepWidth;
}

float PianoRoll::YOf(const Grid &grid, int pitch) const {
    return grid.area.y + static_cast<float>(topPitch - pitch) * PITCH_HEIGHT;
}

void PianoRoll::Draw(Ui &ui, Rectangle bounds, Pattern &pattern, Color colour, NoteClipboard &clipboard) {
    Grid grid = LayoutFor(bounds);

    scrubbed = -1.0f;
    asked = Preview{};

    // Behind the last note there is always room for more
    contentSteps = std::max(
        static_cast<float>(pattern.Length() + TAIL_STEPS),
        scroll + VisibleSteps(grid)
    );

    // While playing the view follows the line, so the song stays visible
    if (following) {
        float visible = grid.area.width / stepWidth;
        float head = playhead * static_cast<float>(Pattern::STEPS_PER_BEAT);

        if (head < scroll || head > scroll + visible) {
            scroll = std::max(head - visible / 4.0f, 0.0f);
        }
    }

    HandleWheel(ui, grid);
    HandleKeyboard(ui, grid);
    HandleKeys(pattern);
    HandleClipboard(ui, grid, pattern, clipboard);
    HandleScrollbars(ui, grid);
    HandleMouse(ui, grid, pattern);
    LimitView(grid);

    BeginScissorMode(
        static_cast<int>(grid.area.x),
        static_cast<int>(grid.area.y),
        static_cast<int>(grid.area.width),
        static_cast<int>(grid.area.height)
    );

    DrawCells(grid);
    DrawNotes(ui, grid, pattern, colour);
    DrawPlayhead(ui, grid);

    // The window lies above the notes while it is pulled
    if (drag == Drag::Select) {
        Rectangle window = SelectionBounds(ui);

        DrawRectangleRec(window, SELECTION_FILL);
        DrawRectangleLinesEx(window, 1.0f, ui.theme.highlight);
    }

    EndScissorMode();

    DrawKeys(ui, grid);
    DrawRuler(ui, grid);
}

// The keyboard on the left, so every row can be read as a note
void PianoRoll::DrawKeys(Ui &ui, const Grid &grid) const {
    BeginScissorMode(
        static_cast<int>(grid.keys.x),
        static_cast<int>(grid.keys.y),
        static_cast<int>(grid.keys.width),
        static_cast<int>(grid.keys.height)
    );

    for (int pitch = topPitch; YOf(grid, pitch) < grid.keys.y + grid.keys.height; pitch--) {
        if (pitch < Pattern::LOWEST_PITCH) {
            break;
        }

        float y = YOf(grid, pitch);
        bool sharp = Pattern::IsSharp(pitch);

        // Black keys are shorter, like on a real keyboard
        float width = grid.keys.width - 1.0f;
        Rectangle key{grid.keys.x, y, sharp ? width * BLACK_KEY_SHARE : width, PITCH_HEIGHT};

        Color colour = sharp ? ROLL_BLACK_KEY : ROLL_WHITE_KEY;

        if (pitch == heldKey || pitch == litPitch) {
            colour = ui.theme.highlight;
        } else if (pitch == hoveredKey) {
            colour = sharp ? ROLL_BLACK_KEY_HOVER : ROLL_WHITE_KEY_HOVER;
        }

        DrawRectangleRec({grid.keys.x, y, width, PITCH_HEIGHT}, ROLL_KEY_BORDER);
        DrawRectangleRec(key, colour);
        DrawRectangleLinesEx(key, 1.0f, ROLL_KEY_BORDER);

        // Only the C of an octave is written out, everything else would be noise
        if (pitch % 12 == 0) {
            ui.font.Draw(
                Pattern::PitchName(pitch),
                {key.x + 2.0f, key.y},
                FontVariant::White,
                TextSpacing::Narrow
            );
        }
    }

    EndScissorMode();
}

// The keys on the left are an instrument of their own: the one under the
// mouse lights up, and holding it asks for its sound.
void PianoRoll::HandleKeyboard(Ui &ui, const Grid &grid) {
    bool inside = Widgets::Hovered(ui, grid.keys);

    hoveredKey = inside ? PitchAt(grid, ui.mouse.y) : 0;

    if (hoveredKey < Pattern::LOWEST_PITCH || hoveredKey > Pattern::HIGHEST_PITCH) {
        hoveredKey = 0;
    }

    // Holding the button and brushing over the keys plays one after another,
    // so the keyboard can be tried out like a real one
    if (inside && ui.down && hoveredKey != 0 && hoveredKey != heldKey) {
        heldKey = hoveredKey;

        asked.pitch = heldKey;
        asked.steps = 0;
    }

    // The sound stops as soon as the button is let go, wherever the mouse is
    if (heldKey != 0 && !ui.down) {
        heldKey = 0;
        asked.stop = true;
    }
}

// Bars above the grid, and where the song stands
void PianoRoll::DrawRuler(Ui &ui, const Grid &grid) {
    DrawRectangleRec(grid.ruler, ui.theme.bar);

    int stepsPerBar = Pattern::STEPS_PER_BEAT * Pattern::BEATS_PER_BAR;
    int first = static_cast<int>(scroll) / stepsPerBar;

    BeginScissorMode(
        static_cast<int>(grid.ruler.x),
        static_cast<int>(grid.ruler.y),
        static_cast<int>(grid.ruler.width),
        static_cast<int>(grid.ruler.height)
    );

    for (int bar = first; XOf(grid, static_cast<float>(bar * stepsPerBar)) < grid.ruler.x + grid.ruler.width; bar++) {
        float x = XOf(grid, static_cast<float>(bar * stepsPerBar));

        DrawRectangle(static_cast<int>(x), static_cast<int>(grid.ruler.y), 1, static_cast<int>(grid.ruler.height),
                      ui.theme.surfaceBorder);

        ui.font.Draw(
            std::to_string(bar + 1),
            {x + 2.0f, grid.ruler.y + 1.0f},
            FontVariant::Grey,
            TextSpacing::Narrow
        );
    }

    // The head of the playhead line, so it can be grabbed
    float head = XOf(grid, playhead * static_cast<float>(Pattern::STEPS_PER_BEAT));

    DrawRectangle(static_cast<int>(head) - 1, static_cast<int>(grid.ruler.y), 3, 3, ui.theme.highlight);

    EndScissorMode();

    // A click into the ruler jumps to that place
    if (Widgets::Hovered(ui, grid.ruler) && ui.down) {
        scrubbed = std::max(
            static_cast<float>(StepAt(grid, ui.mouse.x)) / static_cast<float>(Pattern::STEPS_PER_BEAT),
            0.0f
        );
    }
}

// The rows of the half steps and the lines of the beats
void PianoRoll::DrawCells(const Grid &grid) const {
    for (int pitch = topPitch; YOf(grid, pitch) < grid.area.y + grid.area.height; pitch--) {
        if (pitch < Pattern::LOWEST_PITCH) {
            break;
        }

        if (Pattern::IsSharp(pitch)) {
            DrawRectangle(
                static_cast<int>(grid.area.x),
                static_cast<int>(YOf(grid, pitch)),
                static_cast<int>(grid.area.width),
                static_cast<int>(PITCH_HEIGHT),
                ROLL_ROW_SHARP
            );
        } else if (pitch % 12 == 0) {
            // The line below every C, as a hold for the eye
            DrawRectangle(
                static_cast<int>(grid.area.x),
                static_cast<int>(YOf(grid, pitch) + PITCH_HEIGHT - 1.0f),
                static_cast<int>(grid.area.width),
                1,
                ROLL_BEAT
            );
        }
    }

    // The row of the note under the hand, so it is clear which one is meant
    if (litPitch != 0) {
        DrawRectangle(
            static_cast<int>(grid.area.x),
            static_cast<int>(YOf(grid, litPitch)),
            static_cast<int>(grid.area.width),
            static_cast<int>(PITCH_HEIGHT),
            ROLL_ROW_LIT
        );
    }

    int first = static_cast<int>(scroll);

    for (int step = first; XOf(grid, static_cast<float>(step)) < grid.area.x + grid.area.width; step++) {
        if (step % Pattern::STEPS_PER_BEAT != 0) {
            continue;
        }

        bool bar = step % (Pattern::STEPS_PER_BEAT * Pattern::BEATS_PER_BAR) == 0;

        DrawRectangle(
            static_cast<int>(XOf(grid, static_cast<float>(step))),
            static_cast<int>(grid.area.y),
            1,
            static_cast<int>(grid.area.height),
            bar ? ROLL_BAR : ROLL_BEAT
        );
    }
}

void PianoRoll::DrawNotes(Ui &ui, const Grid &grid, const Pattern &pattern, Color colour) const {
    for (const Note &note: pattern.Notes()) {

        float x = XOf(grid, static_cast<float>(note.step));
        float y = YOf(grid, note.pitch);
        float width = static_cast<float>(note.length) * stepWidth;

        if (x + width < grid.area.x || x > grid.area.x + grid.area.width) {
            continue;
        }

        Rectangle bounds{x, y, std::max(width - 1.0f, 2.0f), PITCH_HEIGHT - 1.0f};

        // A quiet note is darker, so the volume can be seen
        float shade = NOTE_MIN_SHADE + (1.0f - NOTE_MIN_SHADE) * static_cast<float>(note.velocity) / 100.0f;

        DrawRectangleRec(bounds, Shade(colour, shade));
        DrawRectangleLinesEx(bounds, 1.0f, IsChosen(note.id) ? ui.theme.highlight : NOTE_BORDER);

        // The right edge is where the length is changed
        if (bounds.width > 2.0f * EDGE_WIDTH) {
            DrawRectangle(
                static_cast<int>(bounds.x + bounds.width - 2.0f),
                static_cast<int>(bounds.y + 1.0f),
                1,
                static_cast<int>(bounds.height - 2.0f),
                Shade(colour, 0.6f)
            );
        }
    }
}

void PianoRoll::DrawPlayhead(Ui &ui, const Grid &grid) const {
    float x = XOf(grid, playhead * static_cast<float>(Pattern::STEPS_PER_BEAT));

    DrawRectangle(
        static_cast<int>(x),
        static_cast<int>(grid.area.y),
        1,
        static_cast<int>(grid.area.height),
        ui.theme.highlight
    );
}

void PianoRoll::HandleWheel(Ui &ui, const Grid &grid) {
    if (ui.wheel == 0.0f || !Widgets::Hovered(ui, grid.area)) {
        return;
    }

    if (ui.control) {
        // Zoom around the mouse, so what is under it stays under it
        int step = StepAt(grid, ui.mouse.x);

        stepWidth = std::clamp(
            ui.wheel > 0.0f ? stepWidth * ZOOM_STEP : stepWidth / ZOOM_STEP,
            MIN_STEP_WIDTH,
            MAX_STEP_WIDTH
        );

        scroll = static_cast<float>(step) - (ui.mouse.x - grid.area.x) / stepWidth;

        return;
    }

    if (ui.shift) {
        scroll -= ui.wheel * SCROLL_STEPS;
        return;
    }

    topPitch += static_cast<int>(ui.wheel) * SCROLL_PITCHES;
}

// The chosen notes with the arrow keys: length, place and pitch, without
// aiming with the mouse
void PianoRoll::HandleKeys(Pattern &pattern) {
    if (chosen.empty()) {
        return;
    }

    auto pressed = [](int key) {
        return IsKeyPressed(key) || IsKeyPressedRepeat(key);
    };

    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    int steps = 0;
    int pitches = 0;
    int length = 0;

    // With Shift the whole note walks, otherwise only its end
    if (pressed(KEY_RIGHT)) {
        (shift ? steps : length) += 1;
    }

    if (pressed(KEY_LEFT)) {
        (shift ? steps : length) -= 1;
    }

    if (pressed(KEY_UP)) {
        pitches += shift ? 12 : 1;
    }

    if (pressed(KEY_DOWN)) {
        pitches -= shift ? 12 : 1;
    }

    if (steps == 0 && pitches == 0 && length == 0) {
        return;
    }

    RememberChosen(pattern);
    ApplyToChosen(pattern, steps, pitches, length, 0);

    if (length != 0) {
        RememberLength(pattern, chosen.front());
    }
}

// A note that was made longer or shorter says how long the next one starts
void PianoRoll::RememberLength(const Pattern &pattern, int id) {
    if (const Note *note = pattern.Get(id)) {
        lastLength = note->length;
    }
}

// Control and C take the chosen notes, Control and V write them again. They
// land under the mouse, or where the song stands while it is somewhere else.
void PianoRoll::HandleClipboard(Ui &ui, const Grid &grid, Pattern &pattern, NoteClipboard &clipboard) {
    if (!ui.control) {
        return;
    }

    if (IsKeyPressed(KEY_C) && !chosen.empty()) {
        std::vector<Note> copied;

        for (const Note &note: pattern.Notes()) {
            if (IsChosen(note.id)) {
                copied.push_back(note);
            }
        }

        clipboard.Put(std::move(copied));
    }

    if (!IsKeyPressed(KEY_V) || clipboard.Empty()) {
        return;
    }

    bool inside = Widgets::Hovered(ui, grid.area);

    // The corner of what was copied goes where the mouse is
    int step = inside
                   ? std::max(StepAt(grid, ui.mouse.x), 0)
                   : static_cast<int>(playhead * static_cast<float>(Pattern::STEPS_PER_BEAT));
    int pitch = inside ? PitchAt(grid, ui.mouse.y) : clipboard.HighestPitch();

    ChooseNone();

    for (const Note &note: clipboard.Notes()) {
        Note pasted = note;

        pasted.step = note.step - clipboard.FirstStep() + step;
        pasted.pitch = note.pitch - clipboard.HighestPitch() + pitch;

        Choose(pattern.Add(pasted));
    }
}

bool PianoRoll::IsChosen(int id) const {
    return std::find(chosen.begin(), chosen.end(), id) != chosen.end();
}

void PianoRoll::Choose(int id) {
    if (id != Pattern::NONE && !IsChosen(id)) {
        chosen.push_back(id);
    }
}

void PianoRoll::ChooseNone() {
    chosen.clear();
}

void PianoRoll::RememberChosen(const Pattern &pattern) {
    before.clear();

    for (int id: chosen) {
        if (const Note *note = pattern.Get(id)) {
            before[id] = *note;
        }
    }
}

// steps and pitches move the notes, length changes their end and start their
// beginning. Everything counts from how the notes sat when the drag started.
void PianoRoll::ApplyToChosen(Pattern &pattern, int steps, int pitches, int length, int start) {
    for (const auto &[id, was]: before) {
        Note note = was;

        note.step = std::max(was.step + steps, 0);
        note.pitch = was.pitch + pitches;
        note.length = std::max(was.length + length, 1);

        if (start != 0) {
            // The end stays where it was, the start walks up to it
            int moved = std::clamp(was.step + start, 0, was.End() - 1);

            note.step = moved;
            note.length = was.End() - moved;
        }

        pattern.Set(id, note);
    }
}

// Both scrollbars, at the right and at the bottom like in the engine: the
// thumb shows how much of the pattern is visible and can be dragged.
void PianoRoll::HandleScrollbars(Ui &ui, const Grid &grid) {
    Rectangle timeThumb = TimeThumb(grid);
    Rectangle pitchThumb = PitchThumb(grid);

    bool onTime = Widgets::Hovered(ui, grid.timeBar);
    bool onPitch = Widgets::Hovered(ui, grid.pitchBar);

    // A click grabs the thumb. Next to it the thumb jumps to the mouse first,
    // so a click into the track goes straight to that place.
    if (ui.clicked && onTime) {
        drag = Drag::ScrollTime;
        grabOffset = CheckCollisionPointRec(ui.mouse, timeThumb)
                         ? ui.mouse.x - timeThumb.x
                         : timeThumb.width / 2.0f;
    } else if (ui.clicked && onPitch) {
        drag = Drag::ScrollPitch;
        grabOffset = CheckCollisionPointRec(ui.mouse, pitchThumb)
                         ? ui.mouse.y - pitchThumb.y
                         : pitchThumb.height / 2.0f;
    }

    if ((drag == Drag::ScrollTime || drag == Drag::ScrollPitch) && !ui.down) {
        drag = Drag::None;
    }

    if (drag == Drag::ScrollTime) {
        float free = grid.timeBar.width - timeThumb.width;
        float share = free > 0.0f ? (ui.mouse.x - grabOffset - grid.timeBar.x) / free : 0.0f;

        scroll = std::clamp(share, 0.0f, 1.0f) * std::max(contentSteps - VisibleSteps(grid), 0.0f);
    }

    if (drag == Drag::ScrollPitch) {
        float free = grid.pitchBar.height - pitchThumb.height;
        float share = free > 0.0f ? (ui.mouse.y - grabOffset - grid.pitchBar.y) / free : 0.0f;

        float pitches = static_cast<float>(Pattern::HIGHEST_PITCH - Pattern::LOWEST_PITCH) - VisibleRows(grid) + 1.0f;

        topPitch = Pattern::HIGHEST_PITCH - static_cast<int>(std::round(std::clamp(share, 0.0f, 1.0f) * pitches));
    }

    DrawScrollbar(ui, grid.timeBar, TimeThumb(grid), drag == Drag::ScrollTime);
    DrawScrollbar(ui, grid.pitchBar, PitchThumb(grid), drag == Drag::ScrollPitch);
}

Rectangle PianoRoll::TimeThumb(const Grid &grid) const {
    float visible = VisibleSteps(grid);
    float share = std::clamp(visible / std::max(contentSteps, 1.0f), 0.0f, 1.0f);

    float width = std::max(grid.timeBar.width * share, SCROLLBAR_MIN_THUMB);
    float free = grid.timeBar.width - width;
    float scrolled = std::max(contentSteps - visible, 0.0f);

    float at = scrolled > 0.0f ? free * std::clamp(scroll / scrolled, 0.0f, 1.0f) : 0.0f;

    return {grid.timeBar.x + at, grid.timeBar.y, width, grid.timeBar.height};
}

Rectangle PianoRoll::PitchThumb(const Grid &grid) const {
    float rows = VisibleRows(grid);
    float pitches = static_cast<float>(Pattern::HIGHEST_PITCH - Pattern::LOWEST_PITCH + 1);

    float share = std::clamp(rows / pitches, 0.0f, 1.0f);
    float height = std::max(grid.pitchBar.height * share, SCROLLBAR_MIN_THUMB);
    float free = grid.pitchBar.height - height;

    float scrolled = pitches - rows;
    float above = static_cast<float>(Pattern::HIGHEST_PITCH - topPitch);

    float at = scrolled > 0.0f ? free * std::clamp(above / scrolled, 0.0f, 1.0f) : 0.0f;

    return {grid.pitchBar.x, grid.pitchBar.y + at, grid.pitchBar.width, height};
}

void PianoRoll::DrawScrollbar(Ui &ui, Rectangle track, Rectangle thumb, bool held) const {
    DrawRectangleRec(track, SCROLL_TRACK);
    DrawRectangleRec(thumb, held ? ui.theme.highlight : SCROLL_THUMB);
}

void PianoRoll::HandleMouse(Ui &ui, const Grid &grid, Pattern &pattern) {
    if (drag == Drag::ScrollTime || drag == Drag::ScrollPitch) {
        return;
    }

    bool inside = Widgets::Hovered(ui, grid.area);

    int step = std::max(StepAt(grid, ui.mouse.x), 0);
    int pitch = PitchAt(grid, ui.mouse.y);

    // Delete and Backspace take away every chosen note. Backspace with Shift
    // belongs to the transport and rewinds, see StudioView.
    bool erase = IsKeyPressed(KEY_DELETE) ||
                 (IsKeyPressed(KEY_BACKSPACE) && !ui.shift);

    if (erase && !chosen.empty()) {
        for (int id: chosen) {
            pattern.Remove(id);
        }

        ChooseNone();
    }

    // The right button erases, also while it is dragged along
    if (inside && ui.rightDown) {
        int under = pattern.At(step, pitch);

        if (under != Pattern::NONE) {
            pattern.Remove(under);

            chosen.erase(std::remove(chosen.begin(), chosen.end(), under), chosen.end());
        }

        return;
    }

    if (drag == Drag::None && inside && ui.clicked) {
        int under = pattern.At(step, pitch);

        if (ui.control) {
            // Control picks single notes, one after another, and lets go of
            // one that was already chosen
            if (under != Pattern::NONE) {
                if (IsChosen(under)) {
                    chosen.erase(std::remove(chosen.begin(), chosen.end(), under), chosen.end());
                } else {
                    Choose(under);
                }
            }
        } else if (ui.shift) {
            // Shift pulls a window over the notes it should choose
            drag = Drag::Select;
            selectionStart = ui.mouse;

            ChooseNone();
        } else if (under == Pattern::NONE) {
            // A new note, as long as the one written last. Keeping the button
            // held and moving right makes it longer right away.
            Note note;
            note.step = step;
            note.pitch = pitch;
            note.length = lastLength;

            dragNote = pattern.Add(note);
            dragStart = *pattern.Get(dragNote);
            drag = Drag::Create;
            createMoved = false;

            ChooseNone();
            Choose(dragNote);
            RememberChosen(pattern);

            // A written note is heard right away, as long as it is
            asked.pitch = note.pitch;
            asked.steps = note.length;
        } else {
            const Note &note = *pattern.Get(under);

            float start = XOf(grid, static_cast<float>(note.step));
            float end = XOf(grid, static_cast<float>(note.End()));

            dragNote = under;
            dragStart = note;
            grabStep = step;
            grabPitch = pitch;

            if (ui.mouse.x >= end - EDGE_WIDTH) {
                drag = Drag::Resize;
            } else if (ui.mouse.x <= start + EDGE_WIDTH) {
                drag = Drag::ResizeStart;
            } else {
                drag = Drag::Move;
            }

            // A note outside the selection becomes the only chosen one, one
            // inside it takes the whole selection along
            if (!IsChosen(under)) {
                ChooseNone();
                Choose(under);
            }

            RememberChosen(pattern);
        }
    }

    if (drag == Drag::Select) {
        if (!ui.down) {
            Rectangle window = SelectionBounds(ui);

            for (const Note &note: pattern.Notes()) {
                Rectangle bounds{
                    XOf(grid, static_cast<float>(note.step)),
                    YOf(grid, note.pitch),
                    static_cast<float>(note.length) * stepWidth,
                    PITCH_HEIGHT
                };

                if (CheckCollisionRecs(window, bounds)) {
                    Choose(note.id);
                }
            }

            drag = Drag::None;
        }

        return;
    }

    if (drag == Drag::Create || drag == Drag::Resize || drag == Drag::ResizeStart || drag == Drag::Move) {
        // A new note keeps the length of the one written last. Only once the
        // mouse leaves the step it started in does the drag say how long it is.
        if (drag == Drag::Create) {
            createMoved = createMoved || step != dragStart.step;

            int wanted = createMoved ? std::max(step - dragStart.step + 1, 1) : lastLength;

            ApplyToChosen(pattern, 0, 0, wanted - dragStart.length, 0);
        } else if (drag == Drag::Resize) {
            ApplyToChosen(pattern, 0, 0, step - dragStart.End() + 1, 0);
        } else if (drag == Drag::ResizeStart) {
            ApplyToChosen(pattern, 0, 0, 0, step - dragStart.step);
        } else {
            ApplyToChosen(pattern, step - grabStep, pitch - grabPitch, 0, 0);
        }

        // The key and the row of the note that is in the hand light up
        if (const Note *held = pattern.Get(dragNote)) {
            litPitch = held->pitch;
        }

        if (!ui.down) {
            // A note that was pulled longer is heard in its full length
            if (drag == Drag::Create) {
                if (const Note *written = pattern.Get(dragNote); written != nullptr && written->length > 1) {
                    asked.pitch = written->pitch;
                    asked.steps = written->length;
                }
            }

            // However long it ended up, the next note starts that long
            if (drag != Drag::Move) {
                RememberLength(pattern, dragNote);
            }

            drag = Drag::None;
            dragNote = Pattern::NONE;
            litPitch = 0;
        }
    }
}

// The window while it is being pulled, however it was dragged
Rectangle PianoRoll::SelectionBounds(Ui &ui) const {
    return {
        std::min(selectionStart.x, ui.mouse.x),
        std::min(selectionStart.y, ui.mouse.y),
        std::abs(ui.mouse.x - selectionStart.x),
        std::abs(ui.mouse.y - selectionStart.y)
    };
}

void PianoRoll::LimitView(const Grid &grid) {
    int rows = static_cast<int>(grid.area.height / PITCH_HEIGHT);

    topPitch = std::clamp(topPitch, Pattern::LOWEST_PITCH + rows - 1, Pattern::HIGHEST_PITCH);

    scroll = std::max(scroll, 0.0f);
}
