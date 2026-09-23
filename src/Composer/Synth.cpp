#include "Synth.h"

#include <algorithm>
#include <cmath>

// Samples in one buffer of the stream. Smaller means the sound starts sooner
// after a click, but also costs more work per second.
constexpr int SYNTH_BUFFER = 1024;

// All voices together never reach the edge, so a chord does not clip
constexpr float MASTER_VOLUME = 0.35f;

// How much of its period a pulse is on
constexpr float PULSE_DUTY = 0.25f;
constexpr float THIN_DUTY = 0.125f;

// However short a time is set, this much is always taken: a note that starts
// or ends in no time at all clicks
constexpr float SHORTEST_SECONDS = 0.002f;

// The A above the middle C, and its pitch
constexpr float TUNING_HERTZ = 440.0f;
constexpr int TUNING_PITCH = 69;

// The noise of a chip is a shift register: the long way round it never
// repeats for the ear, the short way round it does and gets a pitch
constexpr int NOISE_TAP = 1;
constexpr int METAL_TAP = 6;

const char *Synth::WaveName(Wave wave) {
    switch (wave) {
        case Wave::Square:
            return "square";

        case Wave::Pulse:
            return "pulse";

        case Wave::Thin:
            return "thin";

        case Wave::Triangle:
            return "triangle";

        case Wave::Saw:
            return "saw";

        case Wave::Noise:
            return "noise";

        case Wave::Metal:
            return "metal";
    }

    return "square";
}

Synth::Wave Synth::WaveFromName(const std::string &name) {
    for (Wave wave: WAVES) {
        if (name == WaveName(wave)) {
            return wave;
        }
    }

    return Wave::Square;
}

Synth::Synth() {
    if (!IsAudioDeviceReady()) {
        return;
    }

    SetAudioStreamBufferSizeDefault(SYNTH_BUFFER);

    stream = LoadAudioStream(SAMPLE_RATE, 16, 1);

    ready = IsAudioStreamValid(stream);

    if (ready) {
        PlayAudioStream(stream);
    }
}

Synth::~Synth() {
    if (ready) {
        UnloadAudioStream(stream);
    }
}

float Synth::FrequencyOf(int pitch) {
    return TUNING_HERTZ * std::pow(2.0f, static_cast<float>(pitch - TUNING_PITCH) / 12.0f);
}

int Synth::Play(int pitch, const Instrument &instrument, float seconds, float loudness) {
    // The voice that is silent, otherwise the one that is furthest along
    std::size_t chosen = 0;
    float quietest = 2.0f;

    for (std::size_t i = 0; i < voices.size(); i++) {
        if (voices[i].stage == Stage::Silent) {
            chosen = i;
            quietest = -1.0f;
            break;
        }

        if (voices[i].level < quietest) {
            chosen = i;
            quietest = voices[i].level;
        }
    }

    Voice &voice = voices[chosen];

    voice.stage = Stage::Attack;
    voice.instrument = instrument;
    voice.phase = 0.0f;
    voice.step = FrequencyOf(pitch) / static_cast<float>(SAMPLE_RATE);
    voice.age = 0.0f;
    voice.volume = std::clamp(instrument.volume * loudness, 0.0f, 1.0f);
    voice.level = 0.0f;
    voice.left = seconds > 0.0f ? static_cast<int>(seconds * SAMPLE_RATE) : -1;
    voice.noise = 1;
    voice.last = 0.0f;
    voice.generation++;

    // The number carries the place and the generation, so an old number cannot
    // stop the note that plays there now
    return static_cast<int>(chosen) + voice.generation * static_cast<int>(VOICES);
}

Synth::Voice *Synth::VoiceOf(int voice) {
    if (voice < 0) {
        return nullptr;
    }

    std::size_t index = static_cast<std::size_t>(voice) % VOICES;
    int generation = voice / static_cast<int>(VOICES);

    return voices[index].generation == generation ? &voices[index] : nullptr;
}

void Synth::Stop(int voice) {
    if (Voice *found = VoiceOf(voice); found != nullptr && found->stage != Stage::Silent) {
        found->stage = Stage::Release;
    }
}

void Synth::StopAll() {
    for (Voice &voice: voices) {
        voice.stage = Stage::Silent;
        voice.level = 0.0f;
    }
}

// One sample of the waveform, from -1 to 1
float Synth::Shape(const Voice &voice) {
    switch (voice.instrument.wave) {
        case Wave::Square:
            return voice.phase < 0.5f ? 1.0f : -1.0f;

        case Wave::Pulse:
            return voice.phase < PULSE_DUTY ? 1.0f : -1.0f;

        case Wave::Thin:
            return voice.phase < THIN_DUTY ? 1.0f : -1.0f;

        case Wave::Triangle:
            return 4.0f * std::abs(voice.phase - 0.5f) - 1.0f;

        case Wave::Saw:
            return 2.0f * voice.phase - 1.0f;

        case Wave::Noise:
        case Wave::Metal:
            return voice.last;
    }

    return 0.0f;
}

void Synth::Render(short *samples, int count) {
    const float perSample = 1.0f / static_cast<float>(SAMPLE_RATE);

    for (int i = 0; i < count; i++) {
        float mixed = 0.0f;

        for (Voice &voice: voices) {
            if (voice.stage == Stage::Silent) {
                continue;
            }

            const Instrument &instrument = voice.instrument;

            bool noisy = instrument.wave == Wave::Noise || instrument.wave == Wave::Metal;

            // The noise takes a new value every period. Which bit it listens
            // to decides whether it repeats: the short way round rings.
            if (noisy && voice.phase + voice.step >= 1.0f) {
                int tap = instrument.wave == Wave::Metal ? METAL_TAP : NOISE_TAP;

                std::uint32_t bit = (voice.noise ^ (voice.noise >> tap)) & 1u;

                voice.noise = (voice.noise >> 1) | (bit << 14);
                voice.last = (voice.noise & 1u) != 0u ? 1.0f : -1.0f;
            }

            mixed += Shape(voice) * voice.level * voice.volume;

            // The vibrato swings the pitch, the sweep walks it away
            float bend = 0.0f;

            if (instrument.vibrato > 0.0f) {
                bend += instrument.vibrato *
                        std::sin(6.2831853f * instrument.vibratoHertz * voice.age);
            }

            bend += instrument.sweep * voice.age;

            voice.phase += bend != 0.0f ? voice.step * std::pow(2.0f, bend / 12.0f) : voice.step;
            voice.age += perSample;

            if (voice.phase >= 1.0f) {
                voice.phase -= std::floor(voice.phase);
            }

            // Comes in, falls to the sustain, holds and goes out again
            float attack = 1.0f / (std::max(instrument.attack, SHORTEST_SECONDS) * SAMPLE_RATE);
            float decay = 1.0f / (std::max(instrument.decay, SHORTEST_SECONDS) * SAMPLE_RATE);
            float release = 1.0f / (std::max(instrument.release, SHORTEST_SECONDS) * SAMPLE_RATE);

            float sustain = std::clamp(instrument.sustain, 0.0f, 1.0f);

            if (voice.stage == Stage::Attack) {
                voice.level += attack;

                if (voice.level >= 1.0f) {
                    voice.level = 1.0f;
                    voice.stage = instrument.decay > 0.0f ? Stage::Decay : Stage::Hold;
                }
            } else if (voice.stage == Stage::Decay) {
                voice.level -= decay * (1.0f - sustain);

                if (voice.level <= sustain) {
                    voice.level = sustain;
                    voice.stage = Stage::Hold;
                }

                // An instrument without any sustain is done once it is quiet
                if (voice.level <= 0.0f) {
                    voice.level = 0.0f;
                    voice.stage = Stage::Silent;
                }
            } else if (voice.stage == Stage::Release) {
                voice.level -= release;

                if (voice.level <= 0.0f) {
                    voice.level = 0.0f;
                    voice.stage = Stage::Silent;
                }
            }

            // A note with a length lets go on its own
            if (voice.left > 0) {
                voice.left--;

                if (voice.left == 0 && voice.stage != Stage::Release) {
                    voice.stage = Stage::Release;
                }
            }
        }

        float value = std::clamp(mixed * MASTER_VOLUME, -1.0f, 1.0f);

        samples[i] = static_cast<short>(value * 32767.0f);
    }
}

void Synth::Update() {
    if (!ready) {
        return;
    }

    static short buffer[SYNTH_BUFFER];

    // Fill whatever the sound card has played already
    while (IsAudioStreamProcessed(stream)) {
        Render(buffer, SYNTH_BUFFER);

        UpdateAudioStream(stream, buffer, SYNTH_BUFFER);
    }
}
