#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "FontRenderer.h"
#include "Screen.h"
#include "Textures.h"
#include "Theme.h"
#include "View.h"
#include "Window.h"

struct AppOptions {
    std::string title = "Pingo Composer";

    // Fits on every screen, and the canvas grows with the window
    int windowWidth = 1280;
    int windowHeight = 720;

    // How many window pixels one drawn pixel takes, see Screen
    int scale = 2;

    // Font atlas and its cell size, see FontRenderer
    std::string font = "fonts/game_font.png";
    int letterWidth = 8;
    int letterHeight = 8;
};

// Window, canvas, font and the frame loop.
//
// This is the whole runtime of the composer: it holds what every view needs
// and runs Update and Draw of the open view. Everything else, e.g. tracks or
// sound, lives in the views.
class App {
public:
    explicit App(AppOptions options = {});

    App(const App &) = delete;

    App &operator=(const App &) = delete;

    // Opens the view, e.g. ChangeView<StudioView>(). The change happens at the
    // start of the next frame, so a view may replace itself while it runs.
    template <typename T, typename... Args>
    void ChangeView(Args &&... arguments) {
        pending = [this, ... arguments = std::forward<Args>(arguments)]() mutable {
            return std::make_unique<T>(*this, arguments...);
        };
    }

    // Runs until the window is closed
    void Run();

    // One frame: input, the open view, drawing. Run calls this, and so does
    // a test that wants to drive the program frame by frame.
    void RunFrame();

    // Ends the program after this frame
    void Quit();

    Screen &GetScreen();

    Textures &GetTextures();

    const FontRenderer &GetFont() const;

    const Theme &GetTheme() const;

    Theme &EditTheme();

    // Saves a picture of the program, e.g. for documentation. The name is
    // relative to the working directory. The picture is taken a few frames
    // later, the window needs a moment until it really shows something.
    void TakeShot(const std::string &file);

private:
    void SwitchView();

    // Ctrl and + or - change how large the pixels are drawn
    void UpdateScale();

    AppOptions options;

    // The window is first: it exists before any texture is loaded and is
    // closed after all of them are gone
    Window window;

    Screen screen;

    Textures textures;

    FontRenderer font;

    Theme theme;

    std::unique_ptr<View> view;

    std::function<std::unique_ptr<View>()> pending;

    bool running = true;

    // The name a screenshot is written to, and how many frames it waits
    std::string pendingShot;
    int shotDelay = 0;

    // When and where the last click was, for double clicks
    double lastClick = 0.0;
    Vector2 lastClickAt{0.0f, 0.0f};
};
