#include "Clipboard.h"

#include <algorithm>

void NoteClipboard::Put(std::vector<Note> copied) {
    notes = std::move(copied);
}

const std::vector<Note> &NoteClipboard::Notes() const {
    return notes;
}

bool NoteClipboard::Empty() const {
    return notes.empty();
}

int NoteClipboard::FirstStep() const {
    int first = 0;

    for (std::size_t i = 0; i < notes.size(); i++) {
        first = i == 0 ? notes[i].step : std::min(first, notes[i].step);
    }

    return first;
}

int NoteClipboard::HighestPitch() const {
    int highest = 0;

    for (std::size_t i = 0; i < notes.size(); i++) {
        highest = i == 0 ? notes[i].pitch : std::max(highest, notes[i].pitch);
    }

    return highest;
}
