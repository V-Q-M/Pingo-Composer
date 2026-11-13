//! The piano roll: the notes of a pattern on a grid of steps and pitches.
//!
//! First iteration: looking around. The wheel scrolls the pitches, Shift +
//! wheel or a trackpad the time, Ctrl + wheel zooms the time around the
//! mouse; both scrollbars drag; the keys light up under the mouse; the
//! ruler moves the playhead; a click puts the cursor on a cell. With the
//! keyboard the cursor walks the grid with the vim motions ([`PianoRoll::nav`])
//! and the view follows it. Writing notes follows.

use compose_core::Pattern;
use compose_core::pattern::{
    HIGHEST_PITCH, LOWEST_PITCH, STEPS_PER_BAR, STEPS_PER_BEAT, is_sharp, pitch_name,
};
use pingo_gui::pingo_ui::input::vim::Nav;
use pingo_gui::prelude::*;
use pingo_gui::widgets::{Axis, Scrollbar};

use crate::colors;

/// Width of the keys on the left, height of the ruler on top.
pub const KEYS_WIDTH: f32 = 25.0;
pub const RULER_HEIGHT: f32 = 13.0;
/// One half step is as high as a letter, so key names fit.
pub const PITCH_HEIGHT: f32 = 12.0;
pub const MIN_STEP_WIDTH: f32 = 3.0;
pub const MAX_STEP_WIDTH: f32 = 24.0;
pub const SCROLLBAR_SIZE: f32 = 4.0;
/// Room behind the last note, so there is always space to write.
pub const TAIL_STEPS: u32 = 16;

/// How much darker a quiet note is.
const NOTE_MIN_SHADE: f32 = 0.45;
const SCROLL_STEPS: f32 = 2.0;
const SCROLL_PITCHES: i32 = 3;
const ZOOM_STEP: f32 = 1.25;
/// Share of a black key's width.
const BLACK_KEY_SHARE: f32 = 0.6;

/// Where the roll looks. The notes belong to the pattern, so one roll can
/// show one channel after another.
#[derive(Clone, Debug)]
pub struct PianoRoll {
    /// The first step on the left.
    pub scroll: f32,
    /// The pitch in the top row.
    pub top_pitch: i32,
    pub step_width: f32,
    time_bar: Scrollbar,
    pitch_bar: Scrollbar,
    hovered_key: Option<i32>,
    held_key: Option<i32>,
    /// The keyboard cursor: a step and a pitch.
    pub cursor_step: u32,
    pub cursor_pitch: i32,
    /// Steps and rows that fit, from the last frame.
    visible: (f32, f32),
}

impl Default for PianoRoll {
    fn default() -> PianoRoll {
        PianoRoll {
            scroll: 0.0,
            top_pitch: 84,
            step_width: 8.0,
            time_bar: Scrollbar::default(),
            pitch_bar: Scrollbar::default(),
            hovered_key: None,
            held_key: None,
            cursor_step: 0,
            cursor_pitch: 72,
            visible: (32.0, 10.0),
        }
    }
}

struct Grid {
    area: Rect,
    keys: Rect,
    ruler: Rect,
    time_bar: Rect,
    pitch_bar: Rect,
}

impl PianoRoll {
    fn grid(bounds: Rect) -> Grid {
        let w = (bounds.w - KEYS_WIDTH - SCROLLBAR_SIZE).max(0.0);
        let h = (bounds.h - RULER_HEIGHT - SCROLLBAR_SIZE).max(0.0);
        let area = Rect::new(bounds.x + KEYS_WIDTH, bounds.y + RULER_HEIGHT, w, h);
        Grid {
            ruler: Rect::new(area.x, bounds.y, w, RULER_HEIGHT),
            keys: Rect::new(bounds.x, area.y, KEYS_WIDTH, h),
            time_bar: Rect::new(area.x, area.bottom(), w, SCROLLBAR_SIZE),
            pitch_bar: Rect::new(area.right(), area.y, SCROLLBAR_SIZE, h),
            area,
        }
    }

    fn visible_steps(&self, grid: &Grid) -> f32 {
        grid.area.w / self.step_width
    }

    fn visible_rows(grid: &Grid) -> f32 {
        grid.area.h / PITCH_HEIGHT
    }

    fn x_of(&self, grid: &Grid, step: f32) -> f32 {
        grid.area.x + (step - self.scroll) * self.step_width
    }

    fn y_of(&self, grid: &Grid, pitch: i32) -> f32 {
        grid.area.y + (self.top_pitch - pitch) as f32 * PITCH_HEIGHT
    }

    fn step_at(&self, grid: &Grid, x: f32) -> f32 {
        ((x - grid.area.x) / self.step_width + self.scroll).floor()
    }

    fn pitch_at(&self, grid: &Grid, y: f32) -> i32 {
        self.top_pitch - ((y - grid.area.y) / PITCH_HEIGHT).floor() as i32
    }

    /// Moves the cursor by a vim motion, `count` times where that means
    /// something, and scrolls it into view. `end` is where the pattern ends,
    /// in steps.
    pub fn nav(&mut self, nav: Nav, count: Option<usize>, end: u32) {
        let n = count.unwrap_or(1).max(1) as i64;
        let (steps, rows) = (
            self.visible.0.max(1.0) as i64,
            self.visible.1.max(1.0) as i64,
        );
        let step = i64::from(self.cursor_step);
        let pitch = i64::from(self.cursor_pitch);
        let top = i64::from(self.top_pitch);
        let (step, pitch) = match nav {
            Nav::Left => (step - n, pitch),
            Nav::Right => (step + n, pitch),
            Nav::Up => (step, pitch + n),
            Nav::Down => (step, pitch - n),
            Nav::Top => (step, i64::from(HIGHEST_PITCH)),
            Nav::Bottom => (step, i64::from(LOWEST_PITCH)),
            Nav::LineStart => (0, pitch),
            Nav::LineEnd => (i64::from(end.max(1)) - 1, pitch),
            Nav::HalfPageDown => (step, pitch - rows / 2),
            Nav::HalfPageUp => (step, pitch + rows / 2),
            Nav::PageDown => (step, pitch - rows),
            Nav::PageUp => (step, pitch + rows),
            Nav::ViewTop => (step, top - (n - 1)),
            Nav::ViewMiddle => (step, top - rows / 2),
            Nav::ViewBottom => (step, top - rows + n),
            Nav::AlignTop => {
                self.top_pitch = self.cursor_pitch;
                return;
            }
            Nav::AlignCenter => {
                self.top_pitch = self.cursor_pitch + (rows / 2) as i32;
                self.scroll = (self.cursor_step as f32 - steps as f32 / 2.0)
                    .max(0.0)
                    .floor();
                return;
            }
            Nav::AlignBottom => {
                self.top_pitch = self.cursor_pitch + rows as i32 - 1;
                return;
            }
            Nav::ScrollDown | Nav::ScrollUp | Nav::NextPane | Nav::PrevPane => return,
        };
        self.cursor_step = step.max(0) as u32;
        self.cursor_pitch = pitch.clamp(i64::from(LOWEST_PITCH), i64::from(HIGHEST_PITCH)) as i32;
        self.follow_cursor();
    }

    /// Zooms the time in (`+`) or out (`-`), keeping the cursor in view.
    pub fn zoom(&mut self, steps: i32) {
        let factor = ZOOM_STEP.powi(steps);
        self.step_width = (self.step_width * factor).clamp(MIN_STEP_WIDTH, MAX_STEP_WIDTH);
        self.visible.0 /= factor;
        self.follow_cursor();
    }

    /// Scrolls just enough that the cursor is visible.
    fn follow_cursor(&mut self) {
        let (steps, rows) = (
            self.visible.0.max(1.0).floor(),
            self.visible.1.max(1.0).floor(),
        );
        let step = self.cursor_step as f32;
        if step < self.scroll {
            self.scroll = step;
        } else if step + 1.0 > self.scroll + steps {
            self.scroll = step + 1.0 - steps;
        }
        if self.cursor_pitch > self.top_pitch {
            self.top_pitch = self.cursor_pitch;
        } else if self.cursor_pitch <= self.top_pitch - rows as i32 {
            self.top_pitch = self.cursor_pitch + rows as i32 - 1;
        }
    }

    /// Draws the roll into `bounds`. `playhead` is in beats; while
    /// `following`, the view scrolls along with it. The cursor shows when
    /// `focused`. Returns where the ruler asked the playhead to go, in beats.
    #[allow(clippy::too_many_arguments)]
    pub fn draw(
        &mut self,
        ui: &mut Ui,
        bounds: Rect,
        pattern: &Pattern,
        colour: Rgba,
        playhead: f32,
        following: bool,
        focused: bool,
    ) -> Option<f32> {
        let grid = Self::grid(bounds);
        self.visible = (self.visible_steps(&grid), Self::visible_rows(&grid));
        let head = playhead * STEPS_PER_BEAT as f32;
        if following {
            let visible = self.visible_steps(&grid);
            if head < self.scroll || head > self.scroll + visible {
                self.scroll = (head - visible / 4.0).max(0.0);
            }
        }
        self.handle_wheel(ui, &grid);
        self.handle_keys(ui, &grid);
        self.handle_scrollbars(ui, &grid, pattern);
        self.handle_click(ui, &grid);
        self.limit_view(&grid);

        ui.clipped(grid.area, |ui| {
            self.draw_cells(ui, &grid);
            self.draw_notes(ui, &grid, pattern, colour);
            let x = self.x_of(&grid, head);
            ui.fill(
                Rect::new(x, grid.area.y, 1.0, grid.area.h),
                ui.theme.highlight,
            );
            if focused {
                let cell = Rect::new(
                    self.x_of(&grid, self.cursor_step as f32).round(),
                    self.y_of(&grid, self.cursor_pitch),
                    self.step_width.round().max(2.0) + 1.0,
                    PITCH_HEIGHT,
                );
                ui.outline(cell, ui.theme.highlight);
            }
        });
        self.draw_keys(ui, &grid);
        self.draw_ruler(ui, &grid, head)
    }

    fn handle_wheel(&mut self, ui: &mut Ui, grid: &Grid) {
        if !ui.hovered(grid.area) {
            return;
        }
        let input = ui.input;
        if input.wheel_x != 0.0 {
            self.scroll += input.wheel_x * SCROLL_STEPS;
        }
        if input.wheel == 0.0 {
            return;
        }
        if input.ctrl {
            // Zoom around the mouse, so what is under it stays under it.
            let step = self.step_at(grid, input.mouse.x);
            let width = if input.wheel > 0.0 {
                self.step_width * ZOOM_STEP
            } else {
                self.step_width / ZOOM_STEP
            };
            self.step_width = width.clamp(MIN_STEP_WIDTH, MAX_STEP_WIDTH);
            self.scroll = step - (input.mouse.x - grid.area.x) / self.step_width;
        } else if input.shift {
            self.scroll -= input.wheel * SCROLL_STEPS;
        } else {
            self.top_pitch += input.wheel as i32 * SCROLL_PITCHES;
        }
    }

    /// A click on the grid puts the cursor there.
    fn handle_click(&mut self, ui: &mut Ui, grid: &Grid) {
        if ui.input.clicked && grid.area.contains(ui.input.mouse) {
            self.cursor_step = self.step_at(grid, ui.input.mouse.x).max(0.0) as u32;
            self.cursor_pitch = self
                .pitch_at(grid, ui.input.mouse.y)
                .clamp(LOWEST_PITCH, HIGHEST_PITCH);
        }
    }

    /// The keys light up under the mouse and while held.
    fn handle_keys(&mut self, ui: &mut Ui, grid: &Grid) {
        let inside = ui.hovered(grid.keys);
        let pitch = self.pitch_at(grid, ui.input.mouse.y);
        self.hovered_key =
            (inside && (LOWEST_PITCH..=HIGHEST_PITCH).contains(&pitch)).then_some(pitch);
        if inside && ui.input.down && self.hovered_key.is_some() {
            self.held_key = self.hovered_key;
        }
        if !ui.input.down {
            self.held_key = None;
        }
    }

    fn handle_scrollbars(&mut self, ui: &mut Ui, grid: &Grid, pattern: &Pattern) {
        let visible = self.visible_steps(grid);
        let content = ((pattern.length() + TAIL_STEPS) as f32).max(self.scroll + visible);
        self.scroll = self.time_bar.show(
            ui,
            grid.time_bar,
            Axis::Horizontal,
            self.scroll,
            visible,
            content,
        );

        let rows = Self::visible_rows(grid);
        let pitches = (HIGHEST_PITCH - LOWEST_PITCH + 1) as f32;
        let above = (HIGHEST_PITCH - self.top_pitch) as f32;
        let above = self
            .pitch_bar
            .show(ui, grid.pitch_bar, Axis::Vertical, above, rows, pitches);
        self.top_pitch = HIGHEST_PITCH - above.round() as i32;
    }

    /// Keeps the view inside the pitch range and after the first step.
    fn limit_view(&mut self, grid: &Grid) {
        let rows = (grid.area.h / PITCH_HEIGHT) as i32;
        let lowest_top = (LOWEST_PITCH + rows - 1).min(HIGHEST_PITCH);
        self.top_pitch = self.top_pitch.clamp(lowest_top, HIGHEST_PITCH);
        self.scroll = self.scroll.max(0.0);
    }

    /// The rows of the half steps and the lines of the beats.
    fn draw_cells(&self, ui: &mut Ui, grid: &Grid) {
        let area = grid.area;
        let mut pitch = self.top_pitch;
        while pitch >= LOWEST_PITCH && self.y_of(grid, pitch) < area.bottom() {
            let y = self.y_of(grid, pitch);
            if is_sharp(pitch) {
                ui.fill(
                    Rect::new(area.x, y, area.w, PITCH_HEIGHT),
                    colors::ROW_SHARP,
                );
            } else if pitch % 12 == 0 {
                // A line below every C, as a hold for the eye.
                ui.fill(
                    Rect::new(area.x, y + PITCH_HEIGHT - 1.0, area.w, 1.0),
                    colors::BEAT_LINE,
                );
            }
            if Some(pitch) == self.held_key {
                ui.fill(Rect::new(area.x, y, area.w, PITCH_HEIGHT), colors::ROW_LIT);
            }
            pitch -= 1;
        }
        let mut step = self.scroll.floor() as u32;
        while self.x_of(grid, step as f32) < area.right() {
            if step.is_multiple_of(STEPS_PER_BEAT) {
                let line = if step.is_multiple_of(STEPS_PER_BAR) {
                    colors::BAR_LINE
                } else {
                    colors::BEAT_LINE
                };
                ui.fill(
                    Rect::new(self.x_of(grid, step as f32), area.y, 1.0, area.h),
                    line,
                );
            }
            step += 1;
        }
    }

    fn draw_notes(&self, ui: &mut Ui, grid: &Grid, pattern: &Pattern, colour: Rgba) {
        for note in pattern.notes() {
            let x = self.x_of(grid, note.step as f32);
            let width = note.length as f32 * self.step_width;
            if x + width < grid.area.x || x > grid.area.right() {
                continue;
            }
            // Both edges rounded on their own: notes stay on the pixel grid
            // after zooming, their borders one pixel thick.
            let (left, right) = (x.round(), (x + width).round());
            let rect = Rect::new(
                left,
                self.y_of(grid, note.pitch),
                (right - left - 1.0).max(2.0),
                PITCH_HEIGHT - 1.0,
            );
            let shade = NOTE_MIN_SHADE + (1.0 - NOTE_MIN_SHADE) * f32::from(note.velocity) / 100.0;
            ui.fill(rect, colour.shade(shade));
            ui.outline(rect, colors::NOTE_BORDER);
        }
    }

    /// The keyboard on the left, so every row reads as a note.
    fn draw_keys(&self, ui: &mut Ui, grid: &Grid) {
        let keys = grid.keys;
        ui.clipped(keys, |ui| {
            let mut pitch = self.top_pitch;
            while pitch >= LOWEST_PITCH && self.y_of(grid, pitch) < keys.bottom() {
                let y = self.y_of(grid, pitch);
                let sharp = is_sharp(pitch);
                let width = keys.w - 1.0;
                // Black keys are shorter, like on a real keyboard.
                let key = Rect::new(
                    keys.x,
                    y,
                    if sharp {
                        width * BLACK_KEY_SHARE
                    } else {
                        width
                    },
                    PITCH_HEIGHT,
                );
                let colour = if Some(pitch) == self.held_key {
                    ui.theme.highlight
                } else if Some(pitch) == self.hovered_key {
                    if sharp {
                        colors::BLACK_KEY_HOVER
                    } else {
                        colors::WHITE_KEY_HOVER
                    }
                } else if sharp {
                    colors::BLACK_KEY
                } else {
                    colors::WHITE_KEY
                };
                ui.fill(
                    Rect::new(keys.x, y, width, PITCH_HEIGHT),
                    colors::KEY_BORDER,
                );
                ui.fill(key, colour);
                ui.outline(key, colors::KEY_BORDER);
                // Only the C of an octave is named; the rest would be noise.
                if pitch % 12 == 0 {
                    let text = ui.theme.text;
                    ui.text(Vec2::new(key.x + 2.0, key.y), &pitch_name(pitch), text);
                }
                pitch -= 1;
            }
        });
    }

    /// Bar numbers, the head of the playhead, and scrubbing.
    fn draw_ruler(&self, ui: &mut Ui, grid: &Grid, head: f32) -> Option<f32> {
        let ruler = grid.ruler;
        ui.fill(ruler, ui.theme.bar);
        ui.clipped(ruler, |ui| {
            let mut bar = self.scroll as u32 / STEPS_PER_BAR;
            while self.x_of(grid, (bar * STEPS_PER_BAR) as f32) < ruler.right() {
                let x = self.x_of(grid, (bar * STEPS_PER_BAR) as f32);
                ui.fill(Rect::new(x, ruler.y, 1.0, ruler.h), ui.theme.surface_border);
                let muted = ui.theme.muted;
                ui.text(
                    Vec2::new(x + 2.0, ruler.y + 1.0),
                    &(bar + 1).to_string(),
                    muted,
                );
                bar += 1;
            }
            let x = self.x_of(grid, head);
            ui.fill(Rect::new(x - 1.0, ruler.y, 3.0, 3.0), ui.theme.highlight);
        });
        (ui.hovered(ruler) && ui.input.down)
            .then(|| (self.step_at(grid, ui.input.mouse.x) / STEPS_PER_BEAT as f32).max(0.0))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn grid() -> Grid {
        PianoRoll::grid(Rect::new(0.0, 0.0, 229.0, 137.0))
    }

    #[test]
    fn layout() {
        let g = grid();
        assert_eq!(g.area, Rect::new(25.0, 13.0, 200.0, 120.0));
        assert_eq!(g.pitch_bar.x, 225.0);
        assert_eq!(g.time_bar.y, 133.0);
    }

    #[test]
    fn view_stays_in_range() {
        let mut roll = PianoRoll {
            top_pitch: 500,
            scroll: -4.0,
            ..PianoRoll::default()
        };
        roll.limit_view(&grid());
        assert_eq!((roll.top_pitch, roll.scroll), (HIGHEST_PITCH, 0.0));
        roll.top_pitch = 0;
        roll.limit_view(&grid());
        // Ten rows visible: the lowest pitch sits in the bottom row.
        assert_eq!(roll.top_pitch, LOWEST_PITCH + 9);
    }

    #[test]
    fn cells_under_the_mouse() {
        let roll = PianoRoll::default();
        let g = grid();
        assert_eq!(roll.pitch_at(&g, 13.0), 84);
        assert_eq!(roll.pitch_at(&g, 25.0), 83);
        assert_eq!(roll.step_at(&g, 25.0 + 17.0), 2.0);
    }

    #[test]
    fn the_cursor_walks_and_the_view_follows() {
        let mut roll = PianoRoll {
            visible: (25.0, 10.0),
            ..PianoRoll::default()
        };
        roll.nav(Nav::Right, Some(30), 0);
        assert_eq!(roll.cursor_step, 30);
        assert_eq!(roll.scroll, 6.0);
        roll.nav(Nav::LineStart, None, 0);
        assert_eq!((roll.cursor_step, roll.scroll), (0, 0.0));
        roll.nav(Nav::Up, Some(20), 0);
        assert_eq!((roll.cursor_pitch, roll.top_pitch), (92, 92));
        roll.nav(Nav::Bottom, None, 0);
        assert_eq!(roll.cursor_pitch, LOWEST_PITCH);
        assert_eq!(roll.top_pitch, LOWEST_PITCH + 9);
        roll.nav(Nav::Top, None, 0);
        assert_eq!(roll.cursor_pitch, HIGHEST_PITCH);
        roll.nav(Nav::Left, Some(5), 0);
        assert_eq!(roll.cursor_step, 0, "never left of the start");
        roll.nav(Nav::LineEnd, None, 48);
        assert_eq!(roll.cursor_step, 47);
        roll.nav(Nav::ViewBottom, None, 0);
        assert_eq!(roll.cursor_pitch, roll.top_pitch - 9);
    }
}
