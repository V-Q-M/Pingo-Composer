#pragma once

#include <map>
#include <string>

#include "raylib.h"

// Loads textures once and keeps them until the program ends.
//
// Paths are relative to the asset folder, e.g. "fonts/game_font.png". A file
// that cannot be read becomes a small magenta placeholder instead of a crash.
class Textures {
public:
    // Folder the paths start in, next to the program
    static constexpr const char *FOLDER = "assets";

    ~Textures();

    Textures(const Textures &) = delete;

    Textures &operator=(const Textures &) = delete;

    Textures() = default;

    Texture2D &Get(const std::string &path);

private:
    std::map<std::string, Texture2D> loaded;
};
