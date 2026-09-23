#include "Arranger.h"

#include <algorithm>
#include <cmath>

#include "Engine/Pixels.h"

// Colors of the arrangement, in the palette of the program
constexpr Color ARRANGE_ROW{0, 0, 0, 60};
constexpr Color ARRANGE_ROW_ALT{255, 255, 255, 8};

constexpr Color ARRANGE_BAR_LINE{255, 255, 255, 20};
constexpr Color ARRANGE_GROUP_LINE{255, 255, 255, 60};

constexpr Color BLOCK_BORDER{18, 16, 28, 255};

// How dark a block of a muted channel is
constexpr float MUTED_SHADE = 0.4f;

// Every this many bars a brighter line helps counting
constexpr int BARS_PER_GROUP = 4;

static Color Shade(Color colour, float factor) {
    return {
        static_cast<unsigned char>(static_cast<float>(colour.r) * factor),
        static_cast<unsigned char>(static_cast<float>(colour.g) * factor),
        static_cast<unsigned char>(static_cast<float>(colour.b) * factor),
        colour.a
    };
}

bool Arranger::Block::operator==(const Block &other) const {
    return row == other.row && bar == other.bar;
}

bool Arranger::IsChosen(Block block) const {
    return std::find(chosen.begin(), chosen.end(), block) != chosen.end();
}

void Arranger::SetPlayhead(float beats) {
    playhead = beats;
}

int Arranger::OpenedChannel() const {
    return openedChannel;
}

int Arranger::OpenedBar() const {
    return openedBar;
}

float Arranger::ScrubbedBeats() const {
    return scrubbed;
}

Arranger::Grid Arranger::LayoutFor(Rectangle bounds) const {
    Grid grid;

    grid.ruler = {bounds.x + NAMES_WIDTH, bounds.y, bounds.width - NAMES_WIDTH, RULER_HEIGHT};
    grid.names = {bounds.x, bounds.y + RULER_HEIGHT, NAMES_WIDTH, bounds.height - RULER_HEIGHT};
    grid.area = {
        bounds.x + NAMES_WIDTH,
        bounds.y + RULER_HEIGHT,
        bounds.width - NAMES_WIDTH,
        bounds.height - RULER_HEIGHT
    };

    return grid;
}

int Arranger::BarAt(const Grid &grid, float x) const {
    return static_cast<int>(std::floor((x - grid.area.x) / BAR_WIDTH + scroll));
}

int Arranger::RowAt(const Grid &grid, float y) const {
    return static_cast<int>(std::floor((y - grid.area.y) / ROW_HEIGHT));
}

float Arranger::XOf(const Grid &grid, int bar) const {
    return grid.area.x + (static_cast<float>(bar) - scroll) * BAR_WIDTH;
}

float Arranger::YOf(const Grid &grid, int row) const {
    return grid.area.y + static_cast<float>(row) * ROW_HEIGHT;
}

void Arranger::Draw(Ui &ui, Rectangle bounds, std::vector<Channel> &channels, NoteClipboard &clipboard) {
    Grid grid = LayoutFor(bounds);

    openedChannel = NOTHING;
    openedBar = NOTHING;
    scrubbed = -1.0f;

    lastPattern.resize(channels.size(), 0);

    HandleClipboard(ui, grid, channels, clipboard);
    HandleMouse(ui, grid, channels);

    BeginScissorMode(
        static_cast<int>(grid.area.x),
        static_cast<int>(grid.area.y),
        static_cast<int>(grid.area.width),
        static_cast<int>(grid.area.height)
    );

    DrawBlocks(ui, grid, channels);

    EndScissorMode();

    DrawNames(ui, grid, channels);
    DrawRuler(ui, grid);
}

// The names of the channels, in their own colour
void Arranger::DrawNames(Ui &ui, const Grid &grid, const std::vector<Channel> &channels) const {
    BeginScissorMode(
        static_cast<int>(grid.names.x),
        static_cast<int>(grid.names.y),
        static_cast<int>(grid.names.width),
        static_cast<int>(grid.names.height)
    );

    for (std::size_t i = 0; i < channels.size(); i++) {
        float y = YOf(grid, static_cast<int>(i));

        if (y > grid.names.y + grid.names.height) {
            break;
        }

        Rectangle row{grid.names.x, y, grid.names.width - 1.0f, ROW_HEIGHT - 1.0f};

        DrawRectangleRec(row, i % 2 == 0 ? ARRANGE_ROW : ARRANGE_ROW_ALT);

        // A short line in the colour of the channel, so rows and blocks match
        DrawRectangle(
            static_cast<int>(row.x),
            static_cast<int>(row.y),
            2,
            static_cast<int>(row.height),
            channels[i].muted ? Shade(channels[i].colour, MUTED_SHADE) : channels[i].colour
        );

        ui.font.Draw(
            channels[i].name,
            SnapToPixel({row.x + 5.0f, row.y + (row.height - static_cast<float>(ui.font.LetterHeight())) / 2.0f}),
            channels[i].muted ? ui.theme.mutedVariant : ui.theme.textVariant,
            TextSpacing::Narrow
        );
    }

    EndScissorMode();
}

// The bars above, and where the song stands
void Arranger::DrawRuler(Ui &ui, const Grid &grid) {
    DrawRectangleRec(grid.ruler, ui.theme.bar);

    BeginScissorMode(
        static_cast<int>(grid.ruler.x),
        static_cast<int>(grid.ruler.y),
        static_cast<int>(grid.ruler.width),
        static_cast<int>(grid.ruler.height)
    );

    for (int bar = static_cast<int>(scroll); XOf(grid, bar) < grid.ruler.x + grid.ruler.width; bar++) {
        if (bar % BARS_PER_GROUP != 0) {
            continue;
        }

        float x = XOf(grid, bar);

        DrawRectangle(static_cast<int>(x), static_cast<int>(grid.ruler.y), 1,
                      static_cast<int>(grid.ruler.height), ui.theme.surfaceBorder);

        ui.font.Draw(std::to_string(bar + 1), {x + 2.0f, grid.ruler.y + 1.0f}, FontVariant::Grey, TextSpacing::Narrow);
    }

    // Where the song stands
    float head = XOf(grid, 0) + playhead / static_cast<float>(Pattern::BEATS_PER_BAR) * BAR_WIDTH;

    DrawRectangle(static_cast<int>(head) - 1, static_cast<int>(grid.ruler.y), 3, 3, ui.theme.highlight);

    EndScissorMode();

    // A click into the ruler jumps to that bar
    if (Widgets::Hovered(ui, grid.ruler) && ui.down) {
        scrubbed = std::max(static_cast<float>(BarAt(grid, ui.mouse.x)) * Pattern::BEATS_PER_BAR, 0.0f);
    }
}

void Arranger::DrawBlocks(Ui &ui, const Grid &grid, const std::vector<Channel> &channels) const {
    // The rows first, so empty bars can still be seen
    for (std::size_t i = 0; i < channels.size(); i++) {
        float y = YOf(grid, static_cast<int>(i));

        DrawRectangle(
            static_cast<int>(grid.area.x),
            static_cast<int>(y),
            static_cast<int>(grid.area.width),
            static_cast<int>(ROW_HEIGHT - 1.0f),
            i % 2 == 0 ? ARRANGE_ROW : ARRANGE_ROW_ALT
        );
    }

    for (int bar = static_cast<int>(scroll); XOf(grid, bar) < grid.area.x + grid.area.width; bar++) {
        DrawRectangle(
            static_cast<int>(XOf(grid, bar)),
            static_cast<int>(grid.area.y),
            1,
            static_cast<int>(grid.area.height),
            bar % BARS_PER_GROUP == 0 ? ARRANGE_GROUP_LINE : ARRANGE_BAR_LINE
        );
    }

    // Then the blocks, with the number of their pattern. A block starting
    // left of the view still reaches into it, so the first one is looked for
    // a few bars earlier.
    for (std::size_t i = 0; i < channels.size(); i++) {
        const Channel &channel = channels[i];

        int first = std::max(static_cast<int>(scroll) - Channel::BARS, 0);

        for (int bar = first; XOf(grid, bar) < grid.area.x + grid.area.width; bar++) {
            if (bar >= static_cast<int>(channel.bars.size()) || channel.bars[static_cast<std::size_t>(bar)] < 0) {
                continue;
            }

            int pattern = channel.bars[static_cast<std::size_t>(bar)];

            // As wide as the pattern is long, so four bars of notes look like
            // four bars
            float bars = static_cast<float>(channel.BarsOf(pattern)) * BAR_WIDTH;

            Rectangle block{XOf(grid, bar) + 1.0f, YOf(grid, static_cast<int>(i)) + 1.0f,
                            bars - 2.0f, ROW_HEIGHT - 3.0f};

            Color colour = channel.muted ? Shade(channel.colour, MUTED_SHADE) : channel.colour;

            DrawRectangleRec(block, colour);
            DrawRectangleLinesEx(block, 1.0f,
                                 IsChosen({static_cast<int>(i), bar}) ? ui.theme.highlight : BLOCK_BORDER);

            // An empty pattern is only an outline, so it can be told apart
            if (channel.patterns[static_cast<std::size_t>(pattern)].Notes().empty()) {
                DrawRectangleRec({block.x + 1.0f, block.y + 1.0f, block.width - 2.0f, block.height - 2.0f},
                                 ui.theme.surface);
            }

            std::string name = std::to_string(pattern + 1);
            float width = static_cast<float>(ui.font.Measure(name, TextSpacing::Narrow));

            ui.font.Draw(
                name,
                SnapToPixel({block.x + (block.width - width) / 2.0f,
                             block.y + (block.height - static_cast<float>(ui.font.LetterHeight())) / 2.0f}),
                FontVariant::White,
                TextSpacing::Narrow
            );
        }
    }

    // The line of the playhead across everything
    float head = XOf(grid, 0) + playhead / static_cast<float>(Pattern::BEATS_PER_BAR) * BAR_WIDTH;

    DrawRectangle(
        static_cast<int>(head),
        static_cast<int>(grid.area.y),
        1,
        static_cast<int>(grid.area.height),
        ui.theme.highlight
    );
}

// Delete takes the picked blocks out of the song, Control C and V copy the
// notes of a pattern into another bar. What is copied is a copy of its own, so
// writing in it later does not change the pattern it came from.
void Arranger::HandleClipboard(Ui &ui, const Grid &grid, std::vector<Channel> &channels, NoteClipboard &clipboard) {
    if (IsKeyPressed(KEY_DELETE) || (IsKeyPressed(KEY_BACKSPACE) && !ui.shift)) {
        for (const Block &block: chosen) {
            if (block.row < static_cast<int>(channels.size())) {
                channels[static_cast<std::size_t>(block.row)].Set(block.bar, Channel::EMPTY);
            }
        }

        chosen.clear();
    }

    if (!ui.control) {
        return;
    }

    bool inside = Widgets::Hovered(ui, grid.area);

    int bar = BarAt(grid, ui.mouse.x);
    int row = RowAt(grid, ui.mouse.y);

    bool known = inside && bar >= 0 && bar < Channel::BARS && row >= 0 && row < static_cast<int>(channels.size());

    // The block under the mouse is copied, or the one that was picked first
    if (IsKeyPressed(KEY_C)) {
        Block block{row, bar};

        if (!known && !chosen.empty()) {
            block = chosen.front();
        } else if (!known) {
            return;
        }

        const Pattern *pattern = channels[static_cast<std::size_t>(block.row)].At(block.bar);

        if (pattern != nullptr) {
            clipboard.Put(pattern->Notes());
        }
    }

    // Writing it again makes a pattern of its own in that channel
    if (IsKeyPressed(KEY_V) && known && !clipboard.Empty()) {
        Channel &channel = channels[static_cast<std::size_t>(row)];

        int index = channel.Reserve(static_cast<int>(channel.patterns.size()));

        Pattern &pattern = channel.patterns[static_cast<std::size_t>(index)];

        pattern.Clear();

        for (const Note &note: clipboard.Notes()) {
            pattern.Add(note);
        }

        channel.Set(bar, index);
    }
}

void Arranger::HandleMouse(Ui &ui, const Grid &grid, std::vector<Channel> &channels) {
    bool inside = Widgets::Hovered(ui, grid.area);

    if (!inside) {
        return;
    }

    // A trackpad scrolls sideways on its own
    if (ui.wheelSideways != 0.0f) {
        scroll = std::clamp(scroll + ui.wheelSideways * 2.0f, 0.0f, static_cast<float>(Channel::BARS) - 1.0f);
    }

    int bar = BarAt(grid, ui.mouse.x);
    int row = RowAt(grid, ui.mouse.y);

    bool known = bar >= 0 && bar < Channel::BARS && row >= 0 && row < static_cast<int>(channels.size());

    if (!known) {
        return;
    }

    Channel &channel = channels[static_cast<std::size_t>(row)];
    int &wanted = lastPattern[static_cast<std::size_t>(row)];

    // Bars behind a long block belong to it, so everything works on the bar
    // the block starts in
    int start = channel.StartOf(bar);
    int here = start == Channel::EMPTY ? Channel::EMPTY : channel.bars[static_cast<std::size_t>(start)];

    // A number says which pattern plays here, without any aiming
    if (here >= 0) {
        for (int key = KEY_ONE; key <= KEY_NINE; key++) {
            if (IsKeyPressed(key)) {
                wanted = key - KEY_ONE;

                Place(channel, start, wanted);

                return;
            }
        }
    }

    // The wheel over a block changes which pattern plays there. It takes a
    // whole turn per step: a song needs a handful of patterns, and one flick
    // of a trackpad should not leave fifty empty ones behind.
    if (ui.wheel != 0.0f && here >= 0) {
        Block under{row, start};

        if (!(turning == under)) {
            turning = under;
            turned = 0.0f;
        }

        turned += ui.wheel;

        if (std::abs(turned) >= WHEEL_RESISTANCE) {
            wanted = std::max(here + (turned > 0.0f ? 1 : -1), 0);
            turned = 0.0f;

            Place(channel, start, wanted);
        }

        return;
    }

    // Sideways through the song
    if (ui.wheel != 0.0f) {
        scroll = std::clamp(scroll - ui.wheel * 2.0f, 0.0f, static_cast<float>(Channel::BARS) - 1.0f);

        return;
    }

    // Control picks single blocks, so several of them can be taken away or
    // copied at once
    if (ui.clicked && ui.control) {
        Block block{row, bar};

        if (IsChosen(block)) {
            chosen.erase(std::remove(chosen.begin(), chosen.end(), block), chosen.end());
        } else if (here >= 0) {
            chosen.push_back(block);
        }

        return;
    }

    // A double click opens the pattern of this bar in the roll
    if (ui.doubleClicked && here >= 0) {
        openedChannel = row;
        openedBar = start;

        return;
    }

    // Dragging paints blocks, the right button takes them away again. A bar
    // that already belongs to a block is left alone, so painting never cuts
    // one in half.
    if (ui.down && !ui.control && here == Channel::EMPTY) {
        Place(channel, bar, wanted);
    } else if (ui.rightDown && here >= 0) {
        channel.Set(start, Channel::EMPTY);
    }
}

// Puts a pattern into a bar and keeps the bars it covers free, so no other
// block starts inside it
void Arranger::Place(Channel &channel, int bar, int pattern) {
    int index = channel.Reserve(pattern);

    channel.Set(bar, index);

    for (int covered = bar + 1; covered < bar + channel.BarsOf(index); covered++) {
        channel.Set(covered, Channel::EMPTY);
    }
}
