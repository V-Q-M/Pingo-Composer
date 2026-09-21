#include "App.h"

#include <utility>

// Frames between asking for a picture and taking it: the window of the system
// needs about a second until it really shows the program
constexpr int SHOT_DELAY = 60;

App::App(AppOptions options)
    : options(std::move(options)),
      window(this->options.windowWidth, this->options.windowHeight, this->options.title),
      screen(this->options.scale),
      font(textures.Get(this->options.font), this->options.letterWidth, this->options.letterHeight) {
}

void App::Run() {
    while (running && !WindowShouldClose()) {
        RunFrame();
    }
}

void App::RunFrame() {
    SwitchView();

    screen.Follow();
    UpdateScale();

    float dt = GetFrameTime();

    Ui ui{font, theme};
    ui.mouse = screen.MousePosition();
    ui.clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    ui.down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    ui.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    ui.rightClicked = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    ui.rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    ui.wheel = GetMouseWheelMove();
    ui.shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    ui.control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                 IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

    if (view) {
        view->Update(dt);
    }

    screen.BeginDraw(theme.background);

    if (view) {
        view->Draw(ui);
    }

    screen.EndDraw();

    // The window only really shows something after a few frames
    if (!pendingShot.empty() && --shotDelay <= 0) {
        TakeScreenshot(pendingShot.c_str());
        pendingShot.clear();
    }

    SetMouseCursor(ui.hovering ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
}

void App::SwitchView() {
    if (!pending) {
        return;
    }

    std::function<std::unique_ptr<View>()> create = std::move(pending);
    pending = nullptr;

    // First tear down the old view completely, then build the new one: that
    // way two views never exist at the same time
    view.reset();
    view = create();
}

// Like in a pixel art editor: Ctrl and + or - change how large everything is
// drawn, without changing what the program shows.
void App::UpdateScale() {
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

    if (!control) {
        return;
    }

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        screen.SetScale(screen.Scale() + 1);
    }

    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        screen.SetScale(screen.Scale() - 1);
    }
}

void App::Quit() {
    running = false;
}

Screen &App::GetScreen() {
    return screen;
}

Textures &App::GetTextures() {
    return textures;
}

const FontRenderer &App::GetFont() const {
    return font;
}

const Theme &App::GetTheme() const {
    return theme;
}

Theme &App::EditTheme() {
    return theme;
}

void App::TakeShot(const std::string &file) {
    pendingShot = file;
    shotDelay = SHOT_DELAY;
}
