#include "Screen.h"

#include <algorithm>

Screen::Screen(int scale)
    : scale(std::clamp(scale, MIN_SCALE, MAX_SCALE)) {
    Resize(GetScreenWidth() / this->scale, GetScreenHeight() / this->scale);
}

Screen::~Screen() {
    UnloadRenderTexture(canvas);
}

int Screen::Width() const {
    return canvas.texture.width;
}

int Screen::Height() const {
    return canvas.texture.height;
}

int Screen::Scale() const {
    return scale;
}

void Screen::SetScale(int value) {
    scale = std::clamp(value, MIN_SCALE, MAX_SCALE);
}

Vector2 Screen::MousePosition() const {
    Vector2 mouse = GetMousePosition();

    return {
        std::clamp(std::floor(mouse.x / static_cast<float>(scale)), 0.0f, static_cast<float>(Width() - 1)),
        std::clamp(std::floor(mouse.y / static_cast<float>(scale)), 0.0f, static_cast<float>(Height() - 1))
    };
}

// The canvas covers the whole window. A window that does not divide evenly
// leaves at most one scaled pixel at the right and bottom edge, so the canvas
// is rounded up instead of leaving a black border.
void Screen::Follow() {
    int width = (GetScreenWidth() + scale - 1) / scale;
    int height = (GetScreenHeight() + scale - 1) / scale;

    if (width != Width() || height != Height()) {
        Resize(width, height);
    }
}

void Screen::Resize(int width, int height) {
    if (canvas.id != 0) {
        UnloadRenderTexture(canvas);
    }

    canvas = LoadRenderTexture(std::max(width, 1), std::max(height, 1));

    // Whole pixels, no smoothing: this is what makes the pixel look
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
}

void Screen::BeginDraw(Color background) {
    BeginTextureMode(canvas);
    ClearBackground(background);
}

void Screen::EndDraw() {
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);

    // The render texture stands on its head, so its height is negative here
    Rectangle source{0.0f, 0.0f, static_cast<float>(Width()), -static_cast<float>(Height())};
    Rectangle target{
        0.0f,
        0.0f,
        static_cast<float>(Width() * scale),
        static_cast<float>(Height() * scale)
    };

    DrawTexturePro(canvas.texture, source, target, {0.0f, 0.0f}, 0.0f, WHITE);

    EndDrawing();
}
