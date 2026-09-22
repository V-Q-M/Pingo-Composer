#include "MidiFile.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>

// Colours the read channels get, in the palette of the program
constexpr Color IMPORT_COLOURS[] = {
    {0, 249, 255, 255},
    {61, 255, 20, 255},
    {255, 229, 26, 255},
    {188, 190, 202, 255},
    {255, 108, 34, 255}
};

// The waves they start with: the first ones like a sound chip, the drums of
// midi channel ten as noise
constexpr Synth::Wave IMPORT_WAVES[] = {
    Synth::Wave::Square,
    Synth::Wave::Pulse,
    Synth::Wave::Triangle,
    Synth::Wave::Square,
    Synth::Wave::Pulse
};

// The drums of a midi file live on this channel
constexpr int DRUM_CHANNEL = 9;

constexpr int DEFAULT_TEMPO = 120;

// Reads the bytes of a file, empty when it cannot be read
static std::vector<std::uint8_t> ReadBytes(const std::string &file) {
    std::ifstream in(file, std::ios::binary);

    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Walks through the bytes and never reads past their end
class Reader {
public:
    Reader(const std::vector<std::uint8_t> &bytes, std::size_t at)
        : bytes(bytes), at(at) {
    }

    bool Done() const {
        return at >= bytes.size();
    }

    std::size_t At() const {
        return at;
    }

    std::uint8_t Byte() {
        return at < bytes.size() ? bytes[at++] : 0;
    }

    std::uint8_t Peek() const {
        return at < bytes.size() ? bytes[at] : 0;
    }

    std::uint32_t Big(int count) {
        std::uint32_t value = 0;

        for (int i = 0; i < count; i++) {
            value = (value << 8) | Byte();
        }

        return value;
    }

    // A number in seven bit pieces, as the format writes its times
    std::uint32_t Variable() {
        std::uint32_t value = 0;

        for (int i = 0; i < 4; i++) {
            std::uint8_t byte = Byte();

            value = (value << 7) | (byte & 0x7F);

            if ((byte & 0x80) == 0) {
                break;
            }
        }

        return value;
    }

    void Skip(std::size_t count) {
        at = std::min(at + count, bytes.size());
    }

private:
    const std::vector<std::uint8_t> &bytes;

    std::size_t at;
};

// A note while it is being read: it starts here and ends later
struct Sounding {
    int tick = 0;
    int velocity = 100;
};

bool MidiFile::Load(const std::string &file, std::vector<Channel> &channels, int &tempo) {
    std::vector<std::uint8_t> bytes = ReadBytes(file);

    if (bytes.size() < 14 || bytes[0] != 'M' || bytes[1] != 'T' || bytes[2] != 'h' || bytes[3] != 'd') {
        return false;
    }

    Reader header(bytes, 8);

    header.Big(2);                                          // how the tracks belong together
    int tracks = static_cast<int>(header.Big(2));
    int division = static_cast<int>(header.Big(2));

    // Files that count their time in frames instead of beats are not read
    if (division <= 0 || (division & 0x8000) != 0) {
        return false;
    }

    int readTempo = DEFAULT_TEMPO;

    // What every midi channel played, by its number
    struct Track {
        std::string name;
        std::vector<Note> notes;
    };

    std::map<int, Track> found;

    std::size_t at = 14;

    for (int track = 0; track < tracks && at + 8 <= bytes.size(); track++) {
        bool isTrack = bytes[at] == 'M' && bytes[at + 1] == 'T' && bytes[at + 2] == 'r' && bytes[at + 3] == 'k';

        Reader length(bytes, at + 4);
        std::size_t size = length.Big(4);

        std::size_t start = at + 8;
        std::size_t end = std::min(start + size, bytes.size());

        at = end;

        if (!isTrack) {
            continue;
        }

        Reader reader(bytes, start);

        int tick = 0;
        std::uint8_t status = 0;
        std::string name;

        // Notes that started and wait for their end, by channel and pitch
        std::map<std::pair<int, int>, Sounding> open;

        while (!reader.Done() && reader.At() < end) {
            tick += static_cast<int>(reader.Variable());

            std::uint8_t byte = reader.Peek();

            // Without a new status byte the one before keeps counting
            if ((byte & 0x80) != 0) {
                status = reader.Byte();
            }

            if (status == 0xFF) {
                std::uint8_t kind = reader.Byte();
                std::uint32_t size = reader.Variable();

                if (kind == 0x03) {
                    // The name of the track
                    for (std::uint32_t i = 0; i < size; i++) {
                        name += static_cast<char>(reader.Byte());
                    }
                } else if (kind == 0x51 && size == 3) {
                    std::uint32_t micros = reader.Big(3);

                    if (micros > 0) {
                        readTempo = static_cast<int>(60'000'000 / micros);
                    }
                } else {
                    reader.Skip(size);
                }

                continue;
            }

            if (status == 0xF0 || status == 0xF7) {
                reader.Skip(reader.Variable());
                continue;
            }

            int kind = status & 0xF0;
            int channel = status & 0x0F;

            if (kind == 0x90 || kind == 0x80) {
                int pitch = reader.Byte();
                int velocity = reader.Byte();

                // A start without loudness is really an end
                bool starts = kind == 0x90 && velocity > 0;

                if (starts) {
                    open[{channel, pitch}] = {tick, velocity * 100 / 127};
                } else {
                    auto sounding = open.find({channel, pitch});

                    if (sounding != open.end()) {
                        Note note;

                        // Ticks become steps, at least one step long
                        note.step = sounding->second.tick * Pattern::STEPS_PER_BEAT / division;
                        note.length = std::max(
                            (tick - sounding->second.tick) * Pattern::STEPS_PER_BEAT / division,
                            1
                        );
                        note.pitch = pitch;
                        note.velocity = sounding->second.velocity;

                        found[channel].notes.push_back(note);

                        if (found[channel].name.empty()) {
                            found[channel].name = name;
                        }

                        open.erase(sounding);
                    }
                }
            } else if (kind == 0xC0 || kind == 0xD0) {
                reader.Byte();
            } else if (kind >= 0x80) {
                reader.Byte();
                reader.Byte();
            } else {
                // Nothing that is understood here, the rest of the track is
                // not worth guessing at
                break;
            }
        }
    }

    if (found.empty()) {
        return false;
    }

    // Every midi channel becomes one of ours, its notes cut into bars
    const int stepsPerBar = Pattern::STEPS_PER_BEAT * Pattern::BEATS_PER_BAR;

    std::vector<Channel> read;

    for (auto &[number, track]: found) {
        Channel channel;

        std::size_t index = read.size();

        channel.name = track.name.empty() ? "TRACK " + std::to_string(number + 1) : track.name;
        channel.colour = IMPORT_COLOURS[index % std::size(IMPORT_COLOURS)];
        channel.wave = number == DRUM_CHANNEL ? Synth::Wave::Noise : IMPORT_WAVES[index % std::size(IMPORT_WAVES)];
        channel.patterns.clear();

        // One pattern per bar that has notes in it
        std::map<int, std::size_t> bars;

        for (const Note &note: track.notes) {
            int bar = note.step / stepsPerBar;

            if (bar >= Channel::BARS) {
                continue;
            }

            if (bars.find(bar) == bars.end()) {
                bars[bar] = channel.patterns.size();
                channel.patterns.push_back(Pattern{});
            }

            Note inside = note;
            inside.step = note.step % stepsPerBar;

            channel.patterns[bars[bar]].Add(inside);
        }

        if (channel.patterns.empty()) {
            channel.patterns.push_back(Pattern{});
        }

        for (const auto &[bar, pattern]: bars) {
            channel.Set(bar, static_cast<int>(pattern));
        }

        read.push_back(std::move(channel));
    }

    channels = std::move(read);
    tempo = readTempo;

    return true;
}
