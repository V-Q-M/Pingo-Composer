#include "SongFile.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

// ordered_json keeps the order of the keys, so a saved file stays as readable
// as it was written
using json = nlohmann::ordered_json;

// A colour as it is written, e.g. "#00f9ff"
static std::string ToHex(Color colour) {
    char text[8];

    std::snprintf(text, sizeof(text), "#%02x%02x%02x", colour.r, colour.g, colour.b);

    return text;
}

static Color FromHex(const std::string &text, Color fallback) {
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;

    if (std::sscanf(text.c_str(), "#%02x%02x%02x", &r, &g, &b) != 3) {
        return fallback;
    }

    return {
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b),
        255
    };
}

bool SongFile::Save(const std::string &file, const std::vector<Channel> &channels, int tempo) {
    json song;

    song["tempo"] = tempo;

    json written = json::array();

    for (const Channel &channel: channels) {
        json entry;

        entry["name"] = channel.name;
        entry["wave"] = Synth::WaveName(channel.wave);
        entry["colour"] = ToHex(channel.colour);
        entry["muted"] = channel.muted;

        json patterns = json::array();

        for (const Pattern &pattern: channel.patterns) {
            json notes = json::array();

            for (const Note &note: pattern.Notes()) {
                // The id is only for the program while it runs, it is made
                // again when the file is read
                notes.push_back(json{
                    {"step", note.step},
                    {"length", note.length},
                    {"pitch", note.pitch},
                    {"velocity", note.velocity}
                });
            }

            patterns.push_back(json{{"notes", notes}});
        }

        entry["patterns"] = patterns;

        // Only the bars that really have something in them, so the file stays
        // short and readable
        json blocks = json::array();

        for (std::size_t bar = 0; bar < channel.bars.size(); bar++) {
            if (channel.bars[bar] != Channel::EMPTY) {
                blocks.push_back(json{{"bar", bar}, {"pattern", channel.bars[bar]}});
            }
        }

        entry["blocks"] = blocks;

        written.push_back(entry);
    }

    song["channels"] = written;

    std::ofstream out(file);

    if (!out) {
        return false;
    }

    out << song.dump(2) << "\n";

    return out.good();
}

bool SongFile::Load(const std::string &file, std::vector<Channel> &channels, int &tempo) {
    std::ifstream in(file);

    if (!in) {
        return false;
    }

    json song = json::parse(in, nullptr, false);

    if (song.is_discarded() || !song.is_object() || !song.contains("channels")) {
        return false;
    }

    std::vector<Channel> read;

    for (const json &entry: song["channels"]) {
        Channel channel;

        channel.name = entry.value("name", std::string("CHANNEL"));
        channel.wave = Synth::WaveFromName(entry.value("wave", std::string("square")));
        channel.colour = FromHex(entry.value("colour", std::string()), Color{255, 255, 255, 255});
        channel.muted = entry.value("muted", false);

        channel.patterns.clear();

        for (const json &written: entry.value("patterns", json::array())) {
            Pattern pattern;

            for (const json &note: written.value("notes", json::array())) {
                Note read;

                read.step = note.value("step", 0);
                read.length = note.value("length", 1);
                read.pitch = note.value("pitch", 60);
                read.velocity = note.value("velocity", 100);

                pattern.Add(read);
            }

            channel.patterns.push_back(std::move(pattern));
        }

        // A channel always has at least one pattern to write into
        if (channel.patterns.empty()) {
            channel.patterns.push_back(Pattern{});
        }

        for (const json &block: entry.value("blocks", json::array())) {
            channel.Set(block.value("bar", -1), block.value("pattern", Channel::EMPTY));
        }

        read.push_back(std::move(channel));
    }

    if (read.empty()) {
        return false;
    }

    channels = std::move(read);
    tempo = song.value("tempo", tempo);

    return true;
}
