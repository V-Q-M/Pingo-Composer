//! The piano roll: the notes of a pattern on a grid of steps and pitches.
//!
//! First iteration: looking around. The wheel scrolls the pitches, Shift +
//! wheel or a trackpad the time, Ctrl + wheel zooms the time around the
//! mouse; both scrollbars drag; the keys light up under the mouse; the
//! ruler moves the playhead. Writing notes follows.

use compose_core::Pattern;
use compose_core::pattern::{HIGHEST_PITCH, LOWEST_PITCH, STEPS_PER_BAR, STEPS_PER_BEAT, is_sharp, pitch_name};
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

    /// Draws the roll into `bounds`. `playhead` is in beats; while
    /// `following`, the view scrolls along with it. Returns where the ruler
    /// asked the playhead to go, in beats.
    pub fn draw(
        &mut self,
        ui: &mut Ui,
        bounds: Rect,
        pattern: &Pattern,
        colour: Rgba,
        playhead: f32,
        following: bool,
    ) -> Option<f32> {
        let grid = Self::grid(bounds);
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
        self.limit_view(&grid);

        ui.clipped(grid.area, |ui| {
            self.draw_cells(ui, &grid);
            self.draw_notes(ui, &grid, pattern, colour);
            let x = self.x_of(&grid, head);
            ui.fill(Rect::new(x, grid.area.y, 1.0, grid.area.h), ui.theme.highlight);
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

    /// The keys light up under the mouse and while held.
    fn handle_keys(&mut self, ui: &mut Ui, grid: &Grid) {
        let inside = ui.hovered(grid.keys);
        let pitch = self.pitch_at(grid, ui.input.mouse.y);
        self.hovered_key = (inside && (LOWEST_PITCH..=HIGHEST_PITCH).contains(&pitch)).then_some(pitch);
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
        self.scroll = self
            .time_bar
            .show(ui, grid.time_bar, Axis::Horizontal, self.scroll, visible, content);

        let rows = Self::visible_rows(grid);
        let pitches = (HIGHEST_PITCH - LOWEST_PITCH + 1) as f32;
        let above = (HIGHEST_PITCH - self.top_pitch) as f32;
        let above = self.pitch_bar.show(ui, grid.pitch_bar, Axis::Vertical, above, rows, pitches);
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
                ui.fill(Rect::new(area.x, y, area.w, PITCH_HEIGHT), colors::ROW_SHARP);
            } else if pitch % 12 == 0 {
                // A line below every C, as a hold for the eye.
                ui.fill(Rect::new(area.x, y + PITCH_HEIGHT - 1.0, area.w, 1.0), colors::BEAT_LINE);
            }
            if Some(pitch) == self.held_key {
                ui.fill(Rect::new(area.x, y, area.w, PITCH_HEIGHT), colors::ROW_LIT);
            }
            pitch -= 1;
        }
        let mut step = self.scroll.floor() as u32;
        while self.x_of(grid, step as f32) < area.right() {
            if step % STEPS_PER_BEAT == 0 {
                let line = if step % STEPS_PER_BAR == 0 { colors::BAR_LINE } else { colors::BEAT_LINE };
                ui.fill(Rect::new(self.x_of(grid, step as f32), area.y, 1.0, area.h), line);
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
            let rect = Rect::new(left, self.y_of(grid, note.pitch), (right - left - 1.0).max(2.0), PITCH_HEIGHT - 1.0);
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
                let key = Rect::new(keys.x, y, if sharp { width * BLACK_KEY_SHARE } else { width }, PITCH_HEIGHT);
                let colour = if Some(pitch) == self.held_key {
                    ui.theme.highlight
                } else if Some(pitch) == self.hovered_key {
                    if sharp { colors::BLACK_KEY_HOVER } else { colors::WHITE_KEY_HOVER }
                } else if sharp {
                    colors::BLACK_KEY
                } else {
                    colors::WHITE_KEY
                };
                ui.fill(Rect::new(keys.x, y, width, PITCH_HEIGHT), colors::KEY_BORDER);
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
                ui.text(Vec2::new(x + 2.0, ruler.y + 1.0), &(bar + 1).to_string(), muted);
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
        let mut roll = PianoRoll { top_pitch: 500, scroll: -4.0, ..PianoRoll::default() };
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
}
