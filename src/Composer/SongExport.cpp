#include "SongExport.h"

#include <algorithm>
#include <cstdint>
#include <fstream>

#include "raylib.h"

// Silence behind the last note, so nothing is cut off while it fades out
constexpr int TAIL_SECONDS = 1;

// Ticks of a quarter note in the written notes. 96 divides by four, so our
// steps land on whole ticks.
constexpr int MIDI_DIVISION = 96;

// How loud a note is in the file, from its velocity
constexpr int MIDI_MAX_VELOCITY = 127;

std::vector<SongExport::Event> SongExport::Song(const std::vector<Channel> &channels) {
    const int stepsPerBar = Pattern::STEPS_PER_BEAT * Pattern::BEATS_PER_BAR;

    std::vector<Event> events;

    for (std::size_t i = 0; i < channels.size(); i++) {
        const Channel &channel = channels[i];

        if (channel.muted) {
            continue;
        }

        for (int bar = 0; bar < static_cast<int>(channel.bars.size()); bar++) {
            const Pattern *pattern = channel.At(bar);

            if (pattern == nullptr) {
                continue;
            }

            for (const Note &note: pattern->Notes()) {
                events.push_back({bar * stepsPerBar + note.step, note.length, note.pitch, note.velocity, i});
            }
        }
    }

    return events;
}

std::vector<SongExport::Event> SongExport::OnePattern(const Pattern &pattern) {
    std::vector<Event> events;

    for (const Note &note: pattern.Notes()) {
        events.push_back({note.step, note.length, note.pitch, note.velocity, 0});
    }

    return events;
}

int SongExport::Steps(const std::vector<Event> &events) {
    int end = 0;

    for (const Event &event: events) {
        end = std::max(end, event.step + event.length);
    }

    return end;
}

// Plays the events through a Synth without a sound card and keeps what it
// makes. Step by step, so a note starts exactly where it stands.
std::vector<short> SongExport::Render(const std::vector<Event> &events,
                                      const std::vector<Synth::Instrument> &instruments,
                                      int tempo) {
    Synth synth;

    const float secondsPerStep = 60.0f / static_cast<float>(tempo) / static_cast<float>(Pattern::STEPS_PER_BEAT);
    const int samplesPerStep = std::max(static_cast<int>(secondsPerStep * Synth::SAMPLE_RATE), 1);

    const int steps = Steps(events);

    std::vector<short> samples;
    samples.reserve(static_cast<std::size_t>((steps + 1) * samplesPerStep));

    std::vector<short> chunk(static_cast<std::size_t>(samplesPerStep));

    for (int step = 0; step < steps; step++) {
        for (const Event &event: events) {
            if (event.step != step) {
                continue;
            }

            Synth::Instrument instrument = event.channel < instruments.size()
                                               ? instruments[event.channel]
                                               : Synth::Instrument{};

            synth.Play(
                event.pitch,
                instrument,
                static_cast<float>(event.length) * secondsPerStep,
                static_cast<float>(event.velocity) / 100.0f
            );
        }

        synth.Render(chunk.data(), samplesPerStep);

        samples.insert(samples.end(), chunk.begin(), chunk.end());
    }

    // The last notes fade out after the last step
    std::vector<short> tail(static_cast<std::size_t>(Synth::SAMPLE_RATE * TAIL_SECONDS));

    synth.Render(tail.data(), static_cast<int>(tail.size()));

    samples.insert(samples.end(), tail.begin(), tail.end());

    return samples;
}

bool SongExport::WriteWave(const std::string &file,
                           const std::vector<Event> &events,
                           const std::vector<Synth::Instrument> &instruments,
                           int tempo) {
    std::vector<short> samples = Render(events, instruments, tempo);

    if (samples.empty()) {
        return false;
    }

    Wave wave{};

    wave.frameCount = static_cast<unsigned int>(samples.size());
    wave.sampleRate = Synth::SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples.data();

    // raylib writes the file, the samples stay ours
    return ExportWave(wave, file.c_str());
}

// A number in the way the format wants it: seven bits per byte, the last one
// without its top bit
static void WriteVariable(std::vector<std::uint8_t> &track, int value) {
    std::uint32_t buffer = static_cast<std::uint32_t>(value) & 0x7F;

    for (value >>= 7; value > 0; value >>= 7) {
        buffer <<= 8;
        buffer |= 0x80 | (static_cast<std::uint32_t>(value) & 0x7F);
    }

    while (true) {
        track.push_back(static_cast<std::uint8_t>(buffer & 0xFF));

        if ((buffer & 0x80) == 0) {
            break;
        }

        buffer >>= 8;
    }
}

static void WriteBig(std::vector<std::uint8_t> &bytes, std::uint32_t value, int count) {
    for (int i = count - 1; i >= 0; i--) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

static void WriteChunk(std::ofstream &file, const char *name, const std::vector<std::uint8_t> &body) {
    file.write(name, 4);

    std::vector<std::uint8_t> length;
    WriteBig(length, static_cast<std::uint32_t>(body.size()), 4);

    file.write(reinterpret_cast<const char *>(length.data()), static_cast<long>(length.size()));
    file.write(reinterpret_cast<const char *>(body.data()), static_cast<long>(body.size()));
}

bool SongExport::WriteMidi(const std::string &file,
                           const std::vector<Event> &events,
                           const std::vector<std::string> &names,
                           int tempo) {
    // One track per channel, plus the first one with the tempo
    std::size_t tracks = names.empty() ? 1 : names.size();

    std::ofstream out(file, std::ios::binary);

    if (!out) {
        return false;
    }

    std::vector<std::uint8_t> header;

    WriteBig(header, 1, 2);                                        // several tracks at once
    WriteBig(header, static_cast<std::uint32_t>(tracks + 1), 2);   // the tempo track comes first
    WriteBig(header, MIDI_DIVISION, 2);

    WriteChunk(out, "MThd", header);

    // The tempo track: microseconds per quarter note
    std::vector<std::uint8_t> timing;

    WriteVariable(timing, 0);
    timing.insert(timing.end(), {0xFF, 0x51, 0x03});
    WriteBig(timing, static_cast<std::uint32_t>(60'000'000 / std::max(tempo, 1)), 3);
    WriteVariable(timing, 0);
    timing.insert(timing.end(), {0xFF, 0x2F, 0x00});

    WriteChunk(out, "MTrk", timing);

    const int ticksPerStep = MIDI_DIVISION / Pattern::STEPS_PER_BEAT;

    for (std::size_t channel = 0; channel < tracks; channel++) {
        // Every note gives two moments: where it starts and where it ends
        struct Moment {
            int tick;
            bool on;
            int pitch;
            int velocity;
        };

        std::vector<Moment> moments;

        for (const Event &event: events) {
            if (event.channel != channel) {
                continue;
            }

            moments.push_back({event.step * ticksPerStep, true, event.pitch, event.velocity});
            moments.push_back({(event.step + event.length) * ticksPerStep, false, event.pitch, event.velocity});
        }

        // In time, and an ending note before a starting one on the same tick
        std::stable_sort(moments.begin(), moments.end(), [](const Moment &a, const Moment &b) {
            return a.tick != b.tick ? a.tick < b.tick : (!a.on && b.on);
        });

        std::vector<std::uint8_t> track;

        if (channel < names.size() && !names[channel].empty()) {
            WriteVariable(track, 0);
            track.insert(track.end(), {0xFF, 0x03});
            WriteVariable(track, static_cast<int>(names[channel].size()));
            track.insert(track.end(), names[channel].begin(), names[channel].end());
        }

        int written = 0;

        for (const Moment &moment: moments) {
            WriteVariable(track, moment.tick - written);

            written = moment.tick;

            track.push_back(static_cast<std::uint8_t>((moment.on ? 0x90 : 0x80) | (channel & 0x0F)));
            track.push_back(static_cast<std::uint8_t>(std::clamp(moment.pitch, 0, 127)));
            track.push_back(static_cast<std::uint8_t>(
                moment.on ? std::clamp(moment.velocity * MIDI_MAX_VELOCITY / 100, 1, MIDI_MAX_VELOCITY) : 0
            ));
        }

        WriteVariable(track, 0);
        track.insert(track.end(), {0xFF, 0x2F, 0x00});

        WriteChunk(out, "MTrk", track);
    }

    return out.good();
}

// The ending of the name decides what is written
bool SongExport::Write(const std::string &file,
                       const std::vector<Event> &events,
                       const std::vector<Channel> &channels,
                       int tempo) {
    if (events.empty()) {
        return false;
    }

    bool midi = file.size() > 4 && file.compare(file.size() - 4, 4, ".mid") == 0;

    if (midi) {
        std::vector<std::string> names;

        for (const Channel &channel: channels) {
            names.push_back(channel.name);
        }

        return WriteMidi(file, events, names, tempo);
    }

    std::vector<Synth::Instrument> instruments;

    for (const Channel &channel: channels) {
        instruments.push_back(channel.instrument);
    }

    return WriteWave(file, events, instruments, tempo);
}
