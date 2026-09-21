#include "Window.h"

#include "raylib.h"

Window::Window(int width, int height, const std::string &title) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(width, height, title.c_str());
    InitAudioDevice();

    SetTargetFPS(60);

    // Otherwise raylib quits on ESC. The key belongs to the program.
    SetExitKey(KEY_NULL);

    // A window this small would not show a single panel
    SetWindowMinSize(640, 360);
}

Window::~Window() {
    CloseAudioDevice();
    CloseWindow();
}
