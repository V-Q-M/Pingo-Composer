#pragma once

#include <string>
#include <vector>

#include "Pattern.h"
#include "PianoRoll.h"
#include "Synth.h"
#include "Engine/View.h"

// The studio: the screen the whole program happens in.
//
// At the top the transport bar with play and tempo, on the left the channels,
// at the bottom the status line. A double click on a channel opens its piano
// roll in the middle, a double click inside the roll closes it again.
//
// While the song plays, the notes of every channel that is not muted sound as
// the playhead reaches them.
class StudioView : public View {
public:
    explicit StudioView(App &app);

    void Update(float dt) override;

    void Draw(Ui &ui) override;

private:
    // A channel of the song, e.g. one of the two pulse voices of a NES
    struct Channel {
        std::string name;

        // Its notes are drawn in this colour, so channels can be told apart
        Color colour;

        // How it sounds, see Synth
        Synth::Wave wave = Synth::Wave::Square;

        // A muted channel stays in the song but is not heard
        bool muted = false;

        Pattern pattern;
    };

    // Where everything sits, worked out from the canvas every frame
    struct Layout {
        Rectangle transport;
        Rectangle channels;
        Rectangle pattern;
        Rectangle status;
    };

    Layout LayoutFor(const Ui &ui) const;

    void DrawTransport(Ui &ui, Rectangle bounds);

    void DrawChannels(Ui &ui, Rectangle bounds);

    void DrawPattern(Ui &ui, Rectangle bounds);

    void DrawStatus(Ui &ui, Rectangle bounds);

    std::vector<Channel> channels;

    // The channel the pattern belongs to
    std::size_t current = 0;

    // The notes of the chosen channel
    PianoRoll roll;

    // What the keys and the written notes sound like
    Synth synth;

    // The voice a held key sounds on, so it can be let go again
    int heldVoice = Synth::NO_VOICE;

    // Plays what the roll asked for, see PianoRoll::Preview
    void PlayPreview(const PianoRoll::Preview &asked);

    // Starts every note the playhead has just reached
    void PlayPassedNotes(float from, float to);

    // The empty middle while no roll is open
    void DrawEmpty(Ui &ui, Rectangle bounds);

    // How long a step lasts at the tempo right now
    float SecondsPerStep() const;

    // Is a piano roll open in the middle?
    bool rollOpen = false;

    bool playing = false;

    // Beats per minute and how far the song has run, in beats
    int tempo = 120;
    float position = 0.0f;
};
