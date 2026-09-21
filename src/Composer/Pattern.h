#pragma once

#include <cstddef>
#include <string>
#include <vector>

// One note of a pattern.
//
// Time is counted in steps, pitch like in MIDI: 60 is the C of the fourth
// octave. That way the notes fit any tempo and any sound chip later.
struct Note {
    // Names the note as long as it exists, see Pattern::Add. Notes keep their
    // id when others are removed, so a selection never points at the wrong one.
    int id = 0;

    int step = 0;

    // At least one step long
    int length = 1;

    int pitch = 60;

    // How hard the note is played, 0 to 100
    int velocity = 100;

    int End() const;

    // Does the note sound at this step?
    bool Covers(int atStep) const;
};

// The notes of one channel.
//
// The pattern only knows notes, not how they are drawn: the piano roll does
// that. Notes may overlap, the one added last wins when looking for one, so
// what was drawn last is what the mouse finds.
class Pattern {
public:
    // A beat is divided into this many steps, so the smallest note is a
    // sixteenth at four quarters per bar
    static constexpr int STEPS_PER_BEAT = 4;

    static constexpr int BEATS_PER_BAR = 4;

    // The range of the roll, about the range of a piano
    static constexpr int LOWEST_PITCH = 24;
    static constexpr int HIGHEST_PITCH = 107;

    // Name of a pitch, e.g. "C4" or "F#3"
    static std::string PitchName(int pitch);

    // Is this one of the black keys?
    static bool IsSharp(int pitch);

    // No note: what an empty search gives back
    static constexpr int NONE = 0;

    const std::vector<Note> &Notes() const;

    // Puts the note in and gives it its id, cut to the limits of the pattern
    int Add(Note note);

    void Remove(int id);

    // Changes one note, e.g. while dragging it. An id that is gone does
    // nothing, so a note that disappeared cannot break anything.
    void Set(int id, Note note);

    const Note *Get(int id) const;

    // The note that sounds at this cell, NONE if there is none. Later notes
    // win, so the one drawn last is found first.
    int At(int step, int pitch) const;

    // Where the last note ends, 0 for an empty pattern
    int Length() const;

    void Clear();

private:
    // Keeps a note inside the pitch range and at least one step long
    static Note Limited(Note note);

    std::vector<Note> notes;

    // The id the next note gets. Never counts down, so an id is never given
    // twice in one pattern.
    int nextId = 1;
};
