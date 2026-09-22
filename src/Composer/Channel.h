#pragma once

#include <string>
#include <vector>

#include "raylib.h"

#include "Pattern.h"
#include "Synth.h"

// One voice of the song, e.g. the first pulse channel.
//
// A channel owns its patterns and the arrangement: for every bar of the song
// it says which of its patterns plays there, or none at all. That way the same
// pattern can sound again later without being written twice.
struct Channel {
    // No pattern in this bar
    static constexpr int EMPTY = -1;

    // Bars an arrangement has room for
    static constexpr int BARS = 64;

    std::string name;

    // Its notes and its blocks are drawn in this colour
    Color colour{255, 255, 255, 255};

    // How it sounds, see Synth
    Synth::Wave wave = Synth::Wave::Square;

    // A muted channel stays in the song but is not heard
    bool muted = false;

    // At least one, they are named by their number
    std::vector<Pattern> patterns{Pattern{}};

    // Which pattern plays in which bar, EMPTY for a bar without one
    std::vector<int> bars = std::vector<int>(BARS, EMPTY);

    // The pattern in this bar, nullptr if there is none
    const Pattern *At(int bar) const;

    Pattern *At(int bar);

    // Puts a pattern into a bar. An index that does not exist clears the bar.
    void Set(int bar, int pattern);

    // Makes sure the pattern exists and gives back its index
    int Reserve(int pattern);
};
