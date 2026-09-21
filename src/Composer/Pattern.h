#pragma once

#include <cstddef>
#include <string>
#include <vector>

// One note of a pattern.
//
// Time is counted in steps, pitch like in MIDI: 60 is the C of the fourth
// octave. That way the notes fit any tempo and any sound chip later.
struct Note {
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

    const std::vector<Note> &Notes() const;

    // The new note, cut to the limits of the pattern
    std::size_t Add(Note note);

    void Remove(std::size_t index);

    // Changes one note, e.g. while dragging it. Out of range indices do
    // nothing, so a note that disappeared cannot break anything.
    void Set(std::size_t index, Note note);

    const Note *Get(std::size_t index) const;

    // The note that sounds at this cell, NOTHING if there is none. Later
    // notes win, so the one drawn last is found first.
    static constexpr std::size_t NOTHING = static_cast<std::size_t>(-1);

    std::size_t At(int step, int pitch) const;

    // Where the last note ends, 0 for an empty pattern
    int Length() const;

    void Clear();

private:
    // Keeps a note inside the pitch range and at least one step long
    static Note Limited(Note note);

    std::vector<Note> notes;
};
