#pragma once

#include <string>
#include <vector>

#include "Engine/View.h"

// The studio: the screen the whole program happens in.
//
// At the top the transport bar with play and tempo, on the left the channels,
// in the middle their pattern and at the bottom the status line. For now it
// only draws the frame, the tracks and the sound follow later.
class StudioView : public View {
public:
    explicit StudioView(App &app);

    void Update(float dt) override;

    void Draw(Ui &ui) override;

private:
    // A channel of the song, e.g. one of the two pulse voices of a NES
    struct Channel {
        std::string name;

        // A muted channel stays in the song but is not heard
        bool muted = false;
    };

    // Where everything sits, worked out from the canvas every frame
    struct Layout {
        Rectangle transport;
        Rectangle channels;
        Rectangle pattern;
        Rectangle status;
    };

    Layout LayoutFor(const Ui &ui) const;

    void DrawTransport(Ui &ui, Rectangle bounds);

    void DrawChannels(Ui &ui, Rectangle bounds);

    void DrawPattern(Ui &ui, Rectangle bounds);

    void DrawStatus(Ui &ui, Rectangle bounds);

    std::vector<Channel> channels;

    // The channel the pattern belongs to
    std::size_t current = 0;

    bool playing = false;

    // Beats per minute and how far the song has run, in beats
    int tempo = 120;
    float position = 0.0f;
};
