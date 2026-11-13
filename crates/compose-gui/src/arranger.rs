//! The arrangement: one row per channel, one column per bar. A block in a
//! cell means that pattern of the channel plays in that bar.
//!
//! First iteration: looking around. The wheel and a trackpad scroll through
//! the bars, the ruler moves the playhead, a click puts the cursor on a cell,
//! and a double click on a block or a channel's name opens its pattern in the
//! roll. With the keyboard the cursor walks the cells (`h` `j` `k` `l`, `^`
//! `$`, `ctrl-d` `ctrl-u`, `zz`). Placing blocks follows.

use compose_core::Song;
use compose_core::pattern::BEATS_PER_BAR;
use compose_core::song::BARS;
use pingo_gui::prelude::*;

use crate::colors;

/// Height of a channel row (as high as one in the channel list) and width of
/// a bar.
pub const ROW_HEIGHT: f32 = 18.0;
pub const BAR_WIDTH: f32 = 22.0;
pub const NAMES_WIDTH: f32 = 70.0;
pub const RULER_HEIGHT: f32 = 13.0;

/// Every this many bars a brighter line helps counting.
const BARS_PER_GROUP: usize = 4;

/// How far one wheel step scrolls, in bars.
const SCROLL_BARS: f32 = 2.0;

/// What the arranger asks of the studio after a frame.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Asked {
    /// Open this channel's pattern in the roll.
    pub open: Option<(usize, usize)>,
    /// A click put the cursor on this channel and bar.
    pub cursor: Option<(usize, usize)>,
    /// Move the playhead here, in beats.
    pub scrub: Option<f32>,
}

/// Where the arrangement looks; the song itself belongs to the studio.
#[derive(Clone, Debug, Default)]
pub struct Arranger {
    /// The first bar on the left.
    pub scroll: f32,
    /// The bar the keyboard cursor is in; its row is the chosen channel.
    pub cursor_bar: usize,
    /// How many bars fit, from the last frame.
    visible: f32,
}

struct Grid {
    area: Rect,
    names: Rect,
    ruler: Rect,
}

impl Arranger {
    fn grid(bounds: Rect) -> Grid {
        Grid {
            ruler: Rect::new(
                bounds.x + NAMES_WIDTH,
                bounds.y,
                bounds.w - NAMES_WIDTH,
                RULER_HEIGHT,
            ),
            names: Rect::new(
                bounds.x,
                bounds.y + RULER_HEIGHT,
                NAMES_WIDTH,
                bounds.h - RULER_HEIGHT,
            ),
            area: Rect::new(
                bounds.x + NAMES_WIDTH,
                bounds.y + RULER_HEIGHT,
                bounds.w - NAMES_WIDTH,
                bounds.h - RULER_HEIGHT,
            ),
        }
    }

    fn x_of(&self, grid: &Grid, bar: f32) -> f32 {
        grid.area.x + (bar - self.scroll) * BAR_WIDTH
    }

    fn y_of(grid: &Grid, row: usize) -> f32 {
        grid.area.y + row as f32 * ROW_HEIGHT
    }

    fn bar_at(&self, grid: &Grid, x: f32) -> i32 {
        ((x - grid.area.x) / BAR_WIDTH + self.scroll).floor() as i32
    }

    fn row_at(grid: &Grid, y: f32) -> i32 {
        ((y - grid.area.y) / ROW_HEIGHT).floor() as i32
    }

    /// How many whole bars fit, from the last frame (at least one).
    pub fn visible_bars(&self) -> usize {
        (self.visible.floor() as usize).max(1)
    }

    /// Puts the cursor on `bar` (clamped) and scrolls it into view.
    pub fn set_cursor(&mut self, bar: isize) {
        self.cursor_bar = bar.clamp(0, BARS as isize - 1) as usize;
        let bar = self.cursor_bar as f32;
        let visible = self.visible_bars() as f32;
        if bar < self.scroll {
            self.scroll = bar;
        } else if bar + 1.0 > self.scroll + visible {
            self.scroll = bar + 1.0 - visible;
        }
    }

    /// Scrolls so the cursor is in the middle (`zz`).
    pub fn center_cursor(&mut self) {
        let half = self.visible_bars() as f32 / 2.0;
        self.scroll = (self.cursor_bar as f32 - half)
            .clamp(0.0, BARS as f32 - 1.0)
            .floor();
    }

    /// Draws the arrangement into `bounds`; `playhead` is in beats. The
    /// cursor shows on channel `cursor_row` when the pane is focused.
    pub fn draw(
        &mut self,
        ui: &mut Ui,
        bounds: Rect,
        song: &Song,
        playhead: f32,
        cursor_row: Option<usize>,
    ) -> Asked {
        let grid = Self::grid(bounds);
        self.visible = grid.area.w / BAR_WIDTH;
        let mut asked = Asked::default();
        self.handle_mouse(ui, &grid, song, &mut asked);
        ui.clipped(grid.area, |ui| {
            self.draw_cells(ui, &grid, song, playhead);
            if let Some(row) = cursor_row {
                let cell = Rect::new(
                    self.x_of(&grid, self.cursor_bar as f32),
                    Self::y_of(&grid, row),
                    BAR_WIDTH + 1.0,
                    ROW_HEIGHT,
                );
                ui.outline(cell, ui.theme.highlight);
            }
        });
        ui.clipped(grid.names, |ui| Self::draw_names(ui, &grid, song));
        self.draw_ruler(ui, &grid, playhead, &mut asked);
        asked
    }

    fn handle_mouse(&mut self, ui: &mut Ui, grid: &Grid, song: &Song, asked: &mut Asked) {
        let input = ui.input;
        if ui.hovered(grid.area) {
            // A trackpad scrolls sideways on its own; the wheel too.
            let delta = input.wheel_x * SCROLL_BARS - input.wheel * SCROLL_BARS;
            self.scroll = (self.scroll + delta).clamp(0.0, BARS as f32 - 1.0);

            let bar = self.bar_at(grid, input.mouse.x);
            let row = Self::row_at(grid, input.mouse.y);
            if input.clicked
                && let (Ok(b), Ok(r)) = (usize::try_from(bar), usize::try_from(row))
                && r < song.channels.len()
                && b < BARS
            {
                self.cursor_bar = b;
                asked.cursor = Some((r, b));
            }
            if input.double_clicked
                && let (Ok(bar), Ok(row)) = (usize::try_from(bar), usize::try_from(row))
                && let Some(channel) = song.channels.get(row)
                && let Some((_, pattern, _)) = channel
                    .blocks()
                    .find(|(start, _, len)| (*start..start + len).contains(&bar))
            {
                asked.open = Some((row, pattern));
            }
        }
        // A double click on a name opens the channel's first pattern.
        if ui.hovered(grid.names) && input.double_clicked {
            let row = Self::row_at(grid, input.mouse.y);
            if let Ok(row) = usize::try_from(row)
                && row < song.channels.len()
            {
                asked.open = Some((row, 0));
            }
        }
    }

    fn draw_cells(&self, ui: &mut Ui, grid: &Grid, song: &Song, playhead: f32) {
        let area = grid.area;
        // The rows first, so empty bars can still be seen.
        for i in 0..song.channels.len() {
            let y = Self::y_of(grid, i);
            let color = if i % 2 == 0 {
                colors::ROW
            } else {
                colors::ROW_ALT
            };
            ui.fill(Rect::new(area.x, y, area.w, ROW_HEIGHT - 1.0), color);
        }
        let mut bar = self.scroll.floor() as usize;
        while self.x_of(grid, bar as f32) < area.right() && bar <= BARS {
            let line = if bar.is_multiple_of(BARS_PER_GROUP) {
                ui.theme.grid_accent
            } else {
                ui.theme.grid
            };
            ui.fill(
                Rect::new(self.x_of(grid, bar as f32), area.y, 1.0, area.h),
                line,
            );
            bar += 1;
        }
        // The blocks, as wide as their pattern is long, with its number.
        for (i, channel) in song.channels.iter().enumerate() {
            let colour = colors::channel(channel);
            for (start, pattern, bars) in channel.blocks() {
                let block = Rect::new(
                    self.x_of(grid, start as f32) + 1.0,
                    Self::y_of(grid, i) + 1.0,
                    bars as f32 * BAR_WIDTH - 2.0,
                    ROW_HEIGHT - 3.0,
                );
                if block.right() < area.x || block.x > area.right() {
                    continue;
                }
                ui.fill(block, colour);
                ui.outline(block, colors::BLOCK_BORDER);
                // An empty pattern is only an outline.
                if channel.patterns.get(pattern).is_none_or(|p| p.is_empty()) {
                    ui.fill(block.inset(1.0), ui.theme.surface);
                }
                let text = ui.theme.text;
                ui.text_centered(block, &(pattern + 1).to_string(), text);
            }
        }
        // The playhead across everything.
        let head = self.x_of(grid, playhead / BEATS_PER_BAR as f32);
        ui.fill(Rect::new(head, area.y, 1.0, area.h), ui.theme.highlight);
    }

    fn draw_names(ui: &mut Ui, grid: &Grid, song: &Song) {
        for (i, channel) in song.channels.iter().enumerate() {
            let y = Self::y_of(grid, i);
            if y > grid.names.bottom() {
                break;
            }
            let row = Rect::new(grid.names.x, y, grid.names.w - 1.0, ROW_HEIGHT - 1.0);
            let color = if i % 2 == 0 {
                colors::ROW
            } else {
                colors::ROW_ALT
            };
            ui.fill(row, color);
            // A short line in the channel's color, so rows and blocks match.
            ui.fill(
                Rect::new(row.x, row.y, 2.0, row.h),
                colors::channel(channel),
            );
            let text = if channel.muted {
                ui.theme.muted
            } else {
                ui.theme.text
            };
            let at = Vec2::new(row.x + 5.0, row.y + (row.h - ui.font.line_height()) / 2.0);
            ui.text(at, &channel.name, text);
        }
    }

    fn draw_ruler(&self, ui: &mut Ui, grid: &Grid, playhead: f32, asked: &mut Asked) {
        let ruler = grid.ruler;
        ui.fill(ruler, ui.theme.bar);
        ui.clipped(ruler, |ui| {
            let mut bar = self.scroll.floor() as usize;
            while self.x_of(grid, bar as f32) < ruler.right() && bar < BARS {
                if bar.is_multiple_of(BARS_PER_GROUP) {
                    let x = self.x_of(grid, bar as f32);
                    ui.fill(Rect::new(x, ruler.y, 1.0, ruler.h), ui.theme.surface_border);
                    let muted = ui.theme.muted;
                    ui.text(
                        Vec2::new(x + 2.0, ruler.y + 1.0),
                        &(bar + 1).to_string(),
                        muted,
                    );
                }
                bar += 1;
            }
            let head = self.x_of(grid, playhead / BEATS_PER_BAR as f32);
            ui.fill(Rect::new(head - 1.0, ruler.y, 3.0, 3.0), ui.theme.highlight);
        });
        // A click into the ruler jumps to that bar.
        if ui.hovered(ruler) && ui.input.down {
            let bar = self.bar_at(grid, ui.input.mouse.x).max(0);
            asked.scrub = Some((bar as u32 * BEATS_PER_BAR) as f32);
        }
    }
}
