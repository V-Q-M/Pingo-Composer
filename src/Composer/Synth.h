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
    // How the voice sounds. The pulses are the same waveform at different
    // widths: the thinner it is, the more nasal it gets.
    enum class Wave {
        // Half, a quarter and an eighth of the period
        Square,
        Pulse,
        Thin,

        Triangle,

        // Buzzy, everything a pulse has and more
        Saw,

        // Without a pitch, for drums
        Noise,

        // Noise that repeats itself: it has a pitch of its own and rings,
        // good for metal and for engines
        Metal
    };

    // Nothing is playing on this voice
    static constexpr int NO_VOICE = -1;

    static constexpr int SAMPLE_RATE = 44100;

    // Voices that can sound at the same time
    static constexpr std::size_t VOICES = 8;

    // Every wave there is, in the order they are offered
    static constexpr std::array<Wave, 7> WAVES{
        Wave::Square, Wave::Pulse, Wave::Thin, Wave::Triangle, Wave::Saw, Wave::Noise, Wave::Metal
    };

    // Name of a wave in files and on the screen, e.g. "triangle"
    static const char *WaveName(Wave wave);

    // The wave for a name, square for anything unknown
    static Wave WaveFromName(const std::string &name);

    // How a channel sounds: its waveform and how a note comes and goes.
    //
    // The four times are the ones every synthesizer has: a note rises in the
    // attack, falls to the sustain during the decay, stays there while it is
    // held and fades away in the release. A bass that is plucked has a short
    // decay and no sustain, an organ has none of both.
    struct Instrument {
        Wave wave = Wave::Square;

        // How loud the channel is next to the others, 0 to 1
        float volume = 0.6f;

        // Seconds
        float attack = 0.004f;
        float decay = 0.0f;

        // What is left after the decay, 0 to 1
        float sustain = 1.0f;

        float release = 0.05f;

        // How far the pitch swings while the note is held, in half steps, and
        // how often per second
        float vibrato = 0.0f;
        float vibratoHertz = 5.5f;

        // Half steps the pitch walks per second, e.g. down for a drum
        float sweep = 0.0f;
    };

    Synth();

    ~Synth();

    Synth(const Synth &) = delete;

    Synth &operator=(const Synth &) = delete;

    // Hands new samples to the sound card, belongs into every frame
    void Update();

    // Starts a note and gives back the voice it plays on. seconds of 0 keeps
    // it sounding until Stop, e.g. while a key is held. loudness is how hard
    // the note was played, 0 to 1.
    int Play(int pitch, const Instrument &instrument, float seconds, float loudness = 1.0f);

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
        Decay,
        Hold,
        Release
    };

    struct Voice {
        Stage stage = Stage::Silent;

        Instrument instrument;

        // Where the waveform stands, 0 to 1
        float phase = 0.0f;

        // The pitch it plays, as a step of the phase per sample. The vibrato
        // and the sweep work on it while it sounds.
        float step = 0.0f;

        // Seconds since the note started, for the vibrato and the sweep
        float age = 0.0f;

        float volume = 0.0f;

        // How loud it is right now: rises in the attack, falls in the decay
        // and in the release
        float level = 0.0f;

        // Samples left before it lets go on its own, -1 for a held note
        int left = -1;

        // Counts up while the voice is used, so an old number never stops a
        // new note that got the same place
        int generation = 0;

        // The noise keeps its own shift register, like a real chip
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
