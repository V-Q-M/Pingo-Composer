#pragma once

#include <string>
#include <vector>

#include "Channel.h"

// The song as a file of its own, so work can be picked up again.
//
// The format is JSON: a song can be read, looked at and fixed by hand, like
// the files of the game engine. Everything is in there, so a song that was
// saved comes back exactly as it was: channels with their sound and colour,
// all their patterns, the arrangement and the tempo.
//
// Notes come back as notes, not as sound. Whoever wants the sound exports a
// wave file instead, see SongExport.
class SongFile {
public:
    // What a file of this program is called
    static constexpr const char *EXTENSION = "pingo";

    // false if the file could not be written
    static bool Save(const std::string &file, const std::vector<Channel> &channels, int tempo);

    // Reads a song into channels and tempo. false if the file is missing or
    // broken, and then nothing is changed.
    static bool Load(const std::string &file, std::vector<Channel> &channels, int &tempo);
};
