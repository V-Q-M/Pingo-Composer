#include <string>

#include "Composer/StudioView.h"
#include "Engine/App.h"

// A picture of the first frame, for documentation:
//   PingoComposer --shot studio.png
constexpr const char *SHOT_OPTION = "--shot";

int main(int argc, char **argv) {
    AppOptions options;
    options.title = "Pingo Composer";

    App app(options);

    app.ChangeView<StudioView>();

    for (int i = 1; i + 1 < argc; i++) {
        if (std::string(argv[i]) == SHOT_OPTION) {
            app.TakeShot(argv[i + 1]);
        }
    }

    app.Run();

    return 0;
}
