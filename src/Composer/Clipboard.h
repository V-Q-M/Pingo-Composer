#pragma once

#include <vector>

#include "Pattern.h"

// What was copied last: a handful of notes.
//
// A few notes out of a pattern and a whole pattern are the same thing here, so
// the piano roll and the arrangement share one clipboard. Notes copied in the
// roll can therefore become a pattern in the arrangement, and the other way
// round.
//
// The notes keep their step and their pitch just as they were. Where they land
// is decided by whoever pastes them, FirstStep and HighestPitch are the corner
// they can be put under the mouse by.
class NoteClipboard {
public:
    void Put(std::vector<Note> copied);

    const std::vector<Note> &Notes() const;

    bool Empty() const;

    // The earliest step and the highest pitch of what was copied, 0 while the
    // clipboard is empty
    int FirstStep() const;

    int HighestPitch() const;

private:
    std::vector<Note> notes;
};
