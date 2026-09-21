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

int Pattern::Add(Note note) {
    note.id = nextId++;

    notes.push_back(Limited(note));

    return note.id;
}

void Pattern::Remove(int id) {
    for (std::size_t i = 0; i < notes.size(); i++) {
        if (notes[i].id == id) {
            notes.erase(notes.begin() + static_cast<long>(i));
            return;
        }
    }
}

void Pattern::Set(int id, Note note) {
    for (Note &existing: notes) {
        if (existing.id == id) {
            note.id = id;
            existing = Limited(note);

            return;
        }
    }
}

const Note *Pattern::Get(int id) const {
    for (const Note &note: notes) {
        if (note.id == id) {
            return &note;
        }
    }

    return nullptr;
}

int Pattern::At(int step, int pitch) const {
    for (std::size_t i = notes.size(); i > 0; i--) {
        const Note &note = notes[i - 1];

        if (note.pitch == pitch && note.Covers(step)) {
            return note.id;
        }
    }

    return NONE;
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
