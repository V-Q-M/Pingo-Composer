#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "raylib.h"

// The sound chip of the composer.
//
// It knows a handful of voices, each one a simple waveform like a real chip
// of the eighties. A voice is started with Play and either stops on its own
// after the given time or when Stop is called, e.g. when a key is let go.
//
// The samples are made on the main thread in Update and handed to the audio
// stream. Nothing here runs on the audio thread, so voices can be started and
// stopped from the program without any locking.
class Synth {
public:
    // How the voice sounds
    enum class Wave {
        // Half and a quarter of the period, the two pulse voices of a chip
        Square,
        Pulse,

        Triangle,

        // Without a pitch, for drums
        Noise
    };

    // Nothing is playing on this voice
    static constexpr int NO_VOICE = -1;

    static constexpr int SAMPLE_RATE = 44100;

    // Voices that can sound at the same time
    static constexpr std::size_t VOICES = 8;

    // Name of a wave in files, e.g. "triangle"
    static const char *WaveName(Wave wave);

    // The wave for a name, square for anything unknown
    static Wave WaveFromName(const std::string &name);

    Synth();

    ~Synth();

    Synth(const Synth &) = delete;

    Synth &operator=(const Synth &) = delete;

    // Hands new samples to the sound card, belongs into every frame
    void Update();

    // Starts a note and gives back the voice it plays on. seconds of 0 keeps
    // it sounding until Stop, e.g. while a key is held.
    int Play(int pitch, Wave wave, float seconds, float volume = 0.6f);

    // Lets a voice fade out. An old number that is long gone does nothing.
    void Stop(int voice);

    void StopAll();

    // Writes the next samples, also usable without a sound card, e.g. for a
    // test or for writing a file later
    void Render(short *samples, int count);

    // Frequency of a pitch in hertz, 69 is the A above the middle C
    static float FrequencyOf(int pitch);

private:
    // Where a voice is in its life
    enum class Stage {
        Silent,
        Attack,
        Hold,
        Release
    };

    struct Voice {
        Stage stage = Stage::Silent;

        Wave wave = Wave::Square;

        // Where the waveform stands, 0 to 1
        float phase = 0.0f;
        float step = 0.0f;

        float volume = 0.0f;

        // How loud it is right now: rises in the attack, falls in the release
        float level = 0.0f;

        // Samples left before it lets go on its own, -1 for a held note
        int left = -1;

        // Counts up while the voice is used, so an old number never stops a
        // new note that got the same place
        int generation = 0;

        // The noise keeps its own little shift register
        std::uint32_t noise = 1;

        float last = 0.0f;
    };

    // One sample of a waveform at this place of its period
    static float Shape(const Voice &voice);

    // The voice a number points at, nullptr if it is long gone
    Voice *VoiceOf(int voice);

    std::array<Voice, VOICES> voices{};

    AudioStream stream{};

    bool ready = false;
};
