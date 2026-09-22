#include "Synth.h"

#include <algorithm>
#include <cmath>

// Samples in one buffer of the stream. Smaller means the sound starts sooner
// after a click, but also costs more work per second.
constexpr int SYNTH_BUFFER = 1024;

// How long a note takes to come and to go, in seconds. Without them every
// note would start and end with a click.
constexpr float ATTACK_SECONDS = 0.004f;
constexpr float RELEASE_SECONDS = 0.05f;

// All voices together never reach the edge, so a chord does not clip
constexpr float MASTER_VOLUME = 0.35f;

// The pulse voice is on for this much of its period
constexpr float PULSE_DUTY = 0.25f;

// The A above the middle C, and its pitch
constexpr float TUNING_HERTZ = 440.0f;
constexpr int TUNING_PITCH = 69;

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

int Synth::Play(int pitch, Wave wave, float seconds, float volume) {
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
    voice.wave = wave;
    voice.phase = 0.0f;
    voice.step = FrequencyOf(pitch) / static_cast<float>(SAMPLE_RATE);
    voice.volume = std::clamp(volume, 0.0f, 1.0f);
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
    switch (voice.wave) {
        case Wave::Square:
            return voice.phase < 0.5f ? 1.0f : -1.0f;

        case Wave::Pulse:
            return voice.phase < PULSE_DUTY ? 1.0f : -1.0f;

        case Wave::Triangle:
            return 4.0f * std::abs(voice.phase - 0.5f) - 1.0f;

        case Wave::Noise:
            return voice.last;
    }

    return 0.0f;
}

void Synth::Render(short *samples, int count) {
    const float attack = 1.0f / (ATTACK_SECONDS * SAMPLE_RATE);
    const float release = 1.0f / (RELEASE_SECONDS * SAMPLE_RATE);

    for (int i = 0; i < count; i++) {
        float mixed = 0.0f;

        for (Voice &voice: voices) {
            if (voice.stage == Stage::Silent) {
                continue;
            }

            // The noise takes a new value every period, like a shift register
            if (voice.wave == Wave::Noise && voice.phase + voice.step >= 1.0f) {
                voice.noise = voice.noise * 1103515245u + 12345u;
                voice.last = static_cast<float>((voice.noise >> 16) & 1u) * 2.0f - 1.0f;
            }

            mixed += Shape(voice) * voice.level * voice.volume;

            voice.phase += voice.step;

            if (voice.phase >= 1.0f) {
                voice.phase -= 1.0f;
            }

            // Comes in, holds, and goes out again
            if (voice.stage == Stage::Attack) {
                voice.level += attack;

                if (voice.level >= 1.0f) {
                    voice.level = 1.0f;
                    voice.stage = Stage::Hold;
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
