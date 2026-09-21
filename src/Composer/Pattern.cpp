#include "Pattern.h"

#include <algorithm>

// The names of the twelve half steps, starting at C
constexpr const char *PITCH_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Which of them are black keys
constexpr bool SHARP_KEYS[] = {false, true, false, true, false, false, true, false, true, false, true, false};

// The octave of pitch 60, as on a keyboard: 60 is C4
constexpr int MIDDLE_PITCH = 60;
constexpr int MIDDLE_OCTAVE = 4;

int Note::End() const {
    return step + length;
}

bool Note::Covers(int atStep) const {
    return atStep >= step && atStep < End();
}

std::string Pattern::PitchName(int pitch) {
    int index = ((pitch % 12) + 12) % 12;
    int octave = MIDDLE_OCTAVE + (pitch - MIDDLE_PITCH - index + (pitch % 12 < 0 ? 12 : 0)) / 12;

    return std::string(PITCH_NAMES[index]) + std::to_string(octave);
}

bool Pattern::IsSharp(int pitch) {
    return SHARP_KEYS[((pitch % 12) + 12) % 12];
}

const std::vector<Note> &Pattern::Notes() const {
    return notes;
}

Note Pattern::Limited(Note note) {
    note.step = std::max(note.step, 0);
    note.length = std::max(note.length, 1);
    note.pitch = std::clamp(note.pitch, LOWEST_PITCH, HIGHEST_PITCH);
    note.velocity = std::clamp(note.velocity, 0, 100);

    return note;
}

std::size_t Pattern::Add(Note note) {
    notes.push_back(Limited(note));

    return notes.size() - 1;
}

void Pattern::Remove(std::size_t index) {
    if (index < notes.size()) {
        notes.erase(notes.begin() + static_cast<long>(index));
    }
}

void Pattern::Set(std::size_t index, Note note) {
    if (index < notes.size()) {
        notes[index] = Limited(note);
    }
}

const Note *Pattern::Get(std::size_t index) const {
    return index < notes.size() ? &notes[index] : nullptr;
}

std::size_t Pattern::At(int step, int pitch) const {
    for (std::size_t i = notes.size(); i > 0; i--) {
        const Note &note = notes[i - 1];

        if (note.pitch == pitch && note.Covers(step)) {
            return i - 1;
        }
    }

    return NOTHING;
}

int Pattern::Length() const {
    int end = 0;

    for (const Note &note: notes) {
        end = std::max(end, note.End());
    }

    return end;
}

void Pattern::Clear() {
    notes.clear();
}
