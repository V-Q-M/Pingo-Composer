#pragma once

#include <string>
#include <vector>

#include "Channel.h"

// Reads the notes of a midi file into channels.
//
// Only the notes come across: a midi file says which note sounds when, not
// what it sounds like. Every track becomes one channel, and its notes are cut
// into patterns bar by bar, so the song can be arranged afterwards like any
// other. Our own songs keep their sound in a file of their own, see SongFile.
class MidiFile {
public:
    // What such a file is called
    static constexpr const char *EXTENSION = "mid";

    // Reads the file into channels, and the tempo if the file names one.
    // false if it is missing or not a midi file, and then nothing is changed.
    static bool Load(const std::string &file, std::vector<Channel> &channels, int &tempo);
};
