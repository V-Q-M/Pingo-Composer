#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Channel.h"

// Writes a song or a single pattern into a file.
//
// Two formats, both written by hand without another library:
//   .wav   the finished sound, rendered with the same Synth the program plays
//          with, so the file sounds exactly like what was heard
//   .mid   only the notes, for other programs. Our sounds are not in there,
//          see the note about saving in the backlog.
//
// Everything starts from a list of events: notes counted in steps from the
// beginning. Song and pattern only differ in how that list is collected.
class SongExport {
public:
    // A note of the song, its step counted from the very start
    struct Event {
        int step = 0;
        int length = 1;
        int pitch = 60;
        int velocity = 100;

        // Which channel it belongs to, for the wave and the track
        std::size_t channel = 0;
    };

    // Every note of the arrangement, muted channels left out
    static std::vector<Event> Song(const std::vector<Channel> &channels);

    // Every note of one pattern, as if it started in the first bar
    static std::vector<Event> OnePattern(const Pattern &pattern);

    // How long the events last in steps, including the last note
    static int Steps(const std::vector<Event> &events);

    // Renders the events with the waves of their channels
    static std::vector<short> Render(const std::vector<Event> &events,
                                     const std::vector<Synth::Wave> &waves,
                                     int tempo);

    // false when the file could not be written
    static bool WriteWave(const std::string &file,
                          const std::vector<Event> &events,
                          const std::vector<Synth::Wave> &waves,
                          int tempo);

    // names is one track name per channel, e.g. "PULSE 1"
    static bool WriteMidi(const std::string &file,
                          const std::vector<Event> &events,
                          const std::vector<std::string> &names,
                          int tempo);

    // Writes by the ending of the name: ".mid" gives notes, everything else
    // the sound
    static bool Write(const std::string &file,
                      const std::vector<Event> &events,
                      const std::vector<Channel> &channels,
                      int tempo);
};
