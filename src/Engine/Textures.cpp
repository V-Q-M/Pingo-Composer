#include "Textures.h"

Textures::~Textures() {
    for (auto &[path, texture]: loaded) {
        UnloadTexture(texture);
    }
}

Texture2D &Textures::Get(const std::string &path) {
    auto found = loaded.find(path);

    if (found != loaded.end()) {
        return found->second;
    }

    std::string file = std::string(FOLDER) + "/" + path;

    Texture2D texture = LoadTexture(file.c_str());

    // A missing file leaves an empty texture behind: a garish square shows
    // that something is missing, instead of crashing
    if (texture.id == 0) {
        TraceLog(LOG_WARNING, "TEXTURES: [%s] nicht gefunden", file.c_str());

        Image placeholder = GenImageColor(8, 8, Color{255, 0, 220, 255});

        texture = LoadTextureFromImage(placeholder);

        UnloadImage(placeholder);
    }

    // Pixel art is never smoothed
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    return loaded.emplace(path, texture).first->second;
}
