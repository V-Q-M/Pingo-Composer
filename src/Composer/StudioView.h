#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Arranger.h"
#include "Channel.h"
#include "Clipboard.h"
#include "Pattern.h"
#include "PianoRoll.h"
#include "Synth.h"
#include "Engine/View.h"

// The studio: the screen the whole program happens in.
//
// At the top the transport bar with play and tempo, on the left the channels,
// at the bottom the status line. In the middle stands the arrangement: which
// pattern of a channel plays in which bar. A double click on a channel or on
// one of its blocks opens the piano roll there, a double click on the channel
// closes it again.
//
// While the song plays, every channel that is not muted sounds what its
// arrangement says.
class StudioView : public View {
public:
    explicit StudioView(App &app);

    void Update(float dt) override;

    void Draw(Ui &ui) override;

private:
    // Where everything sits, worked out from the canvas every frame
    struct Layout {
        Rectangle transport;
        Rectangle channels;

        // The arrangement, or the piano roll while one is open
        Rectangle middle;

        Rectangle status;
    };

    Layout LayoutFor(const Ui &ui) const;

    void DrawTransport(Ui &ui, Rectangle bounds);

    void DrawChannels(Ui &ui, Rectangle bounds);

    // The piano roll of the pattern that is open
    void DrawPattern(Ui &ui, Rectangle bounds);

    // The arrangement of the whole song
    void DrawArrangement(Ui &ui, Rectangle bounds);

    void DrawStatus(Ui &ui, Rectangle bounds);

    // Plays what the roll asked for, see PianoRoll::Preview
    void PlayPreview(const PianoRoll::Preview &asked);

    // Starts every note the playhead has reached since the last frame
    void PlayReachedNotes();

    // Starts listening again from where the playhead stands, e.g. after a
    // jump. The note right under it belongs to the new place and sounds.
    void RestartPlayback();

    // How long a step lasts at the tempo right now
    float SecondsPerStep() const;

    // The pattern the roll works on, made if it does not exist yet
    Pattern &CurrentPattern();

    // Writes the song, or the open pattern, as sound or as notes. The ending
    // of the name the user picks decides which of the two.
    void Export();

    // Writes the whole song into a file of our own, so work can go on later.
    // The first time it asks where, after that it writes there again. That is
    // what Control and S do.
    void Save();

    // Always asks where the song should go, e.g. to keep a second version of
    // it. That is what the button in the bar does.
    void SaveAs();

    // Reads a song of our own, or the notes of a midi file
    void Open();

    // Starts over after a song was read
    void AfterLoading();

    // What the last save, export or open did, shown in the status line
    std::string report;

    // Where the song was saved, empty as long as it has no file yet
    std::string songFile;

    std::vector<Channel> channels;

    // The channel and the pattern the roll works on
    std::size_t current = 0;
    int currentPattern = 0;

    // Is a piano roll open in the middle?
    bool rollOpen = false;

    PianoRoll roll;

    Arranger arranger;

    // What was copied last, shared by the roll and the arrangement: notes out
    // of a pattern and a whole pattern are the same thing to it
    NoteClipboard clipboard;

    // What the keys and the written notes sound like
    Synth synth;

    // The voice a held key sounds on, so it can be let go again
    int heldVoice = Synth::NO_VOICE;

    // The last step that was played already. -1 means that nothing was
    // played yet, so a note right at the start is not missed.
    int playedThrough = -1;

    bool playing = false;

    // Beats per minute and how far the song has run, in beats
    int tempo = 120;
    float position = 0.0f;
};
