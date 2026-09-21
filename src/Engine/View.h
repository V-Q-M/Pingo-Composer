#pragma once

#include "Widgets.h"

class App;

// One screen of the program, e.g. the studio with its tracks.
//
// A view gets the app in its constructor and reaches screen, font and theme
// through it. Update runs before Draw, both once per frame.
class View {
public:
    explicit View(App &app);

    virtual ~View() = default;

    View(const View &) = delete;

    View &operator=(const View &) = delete;

    virtual void Update(float dt);

    // Everything of this view, in canvas pixels. The Ui carries the mouse of
    // this frame, see Widgets.
    virtual void Draw(Ui &ui);

protected:
    App &app;
};
