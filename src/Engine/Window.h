#pragma once

#include <string>

// Opens the window and the audio device and closes both again.
//
// As the first member of the app this class ensures the right order: the
// window exists before textures are loaded, and is only closed after all
// textures and sounds have been released.
class Window {
public:
    Window(int width, int height, const std::string &title);

    ~Window();

    Window(const Window &) = delete;

    Window &operator=(const Window &) = delete;
};
