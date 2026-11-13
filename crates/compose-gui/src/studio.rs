//! The studio: the one screen the composer has.
//!
//! The transport bar on top, the channels and the instrument of the chosen
//! one on the left, the status line at the bottom. In the middle the
//! arrangement, or the piano roll of a pattern: a double click on a channel
//! opens and closes it, Esc closes it too.
//!
//! First iteration: everything can be looked at and walked through, but
//! nothing sounds and no notes are written. Play runs the playhead silently.

use compose_core::Song;
use compose_core::pattern::BEATS_PER_BAR;
use compose_core::song::Setting;
use pingo_gui::prelude::*;
use pingo_gui::ui::PADDING;
use pingo_gui::widgets::{bar, button, panel, stepper, sunken, toggle};

use crate::arranger::Arranger;
use crate::colors;
use crate::piano_roll::PianoRoll;

/// Heights of the bars and the width of the channel list, in canvas pixels.
/// The transport holds two rows: files above, playing below.
pub const TRANSPORT_HEIGHT: f32 = 39.0;
pub const STATUS_HEIGHT: f32 = 15.0;
pub const CHANNEL_WIDTH: f32 = 104.0;
pub const GAP: f32 = 3.0;

/// How long an answer stays in the status line.
const REPORT_SECONDS: f32 = 4.0;

/// Where everything sits, worked out from the canvas every frame.
struct Layout {
    transport: Rect,
    channels: Rect,
    instrument: Rect,
    middle: Rect,
    status: Rect,
}

/// The composer's screen and everything it shows.
pub struct Studio {
    pub song: Song,
    /// The chosen channel and the pattern the roll shows.
    pub current: usize,
    pub current_pattern: usize,
    pub roll_open: bool,
    pub playing: bool,
    /// How far the song has run, in beats.
    pub position: f32,
    roll: PianoRoll,
    arranger: Arranger,
    report: Option<(String, f32)>,
}

impl Default for Studio {
    fn default() -> Studio {
        Studio::new(Song::default())
    }
}

impl Studio {
    pub fn new(song: Song) -> Studio {
        Studio {
            song,
            current: 0,
            current_pattern: 0,
            roll_open: false,
            playing: false,
            position: 0.0,
            roll: PianoRoll::default(),
            arranger: Arranger::default(),
            report: None,
        }
    }

    /// An answer in the status line, in place of the place in the song.
    pub fn say(&mut self, text: impl Into<String>) {
        self.report = Some((text.into(), REPORT_SECONDS));
    }

    /// The status line's text right now.
    pub fn report(&self) -> Option<&str> {
        self.report.as_ref().map(|(t, _)| t.as_str())
    }

    fn toggle_playback(&mut self) {
        self.playing = !self.playing;
    }

    fn rewind(&mut self) {
        self.position = 0.0;
    }

    fn open_roll(&mut self, channel: usize, pattern: usize) {
        self.current = channel;
        self.current_pattern = pattern;
        self.roll_open = true;
    }

    fn not_yet(&mut self, what: &str) {
        self.say(format!("{what} comes in a later version"));
    }

    fn layout(&self, ui: &Ui) -> Layout {
        let area = ui.area();
        let transport = Rect::new(0.0, 0.0, area.w, TRANSPORT_HEIGHT);
        let status = Rect::new(0.0, area.h - STATUS_HEIGHT, area.w, STATUS_HEIGHT);
        let top = transport.h + GAP;
        let bottom = status.y - GAP;
        // The list is only as tall as its rows, the instrument sits below.
        let row = ui.row_height();
        let list = row + self.song.channels.len() as f32 * (row + 1.0) + 2.0;
        let channels = Rect::new(GAP, top, CHANNEL_WIDTH, list.min(bottom - top).max(0.0));
        let instrument = Rect::new(
            GAP,
            channels.bottom() + GAP,
            CHANNEL_WIDTH,
            (bottom - channels.bottom() - GAP).max(0.0),
        );
        let middle = Rect::new(
            channels.right() + GAP,
            top,
            (area.w - CHANNEL_WIDTH - 3.0 * GAP).max(0.0),
            (bottom - top).max(0.0),
        );
        Layout { transport, channels, instrument, middle, status }
    }

    fn draw_transport(&mut self, ui: &mut Ui, bounds: Rect) {
        bar(ui, bounds);
        let row = ui.row_height();
        let files = bounds.y + 1.0;
        let y = files + row + 1.0;

        // Files at the left of the upper row.
        let mut x = GAP;
        let open = ui.row_width("Open");
        if button(ui, Rect::new(x, files, open, row), "Open") {
            self.not_yet("Opening");
        }
        x += open + 2.0;
        let export = ui.row_width("Export");
        if button(ui, Rect::new(x, files, export, row), "Export") {
            self.not_yet("Exporting");
        }
        x += export + 2.0;
        if button(ui, Rect::new(x, files, row, row), &icon::SAVE.to_string()) {
            self.not_yet("Saving");
        }

        // Playing in the lower row.
        let mut x = GAP;
        let play = if self.playing { icon::PAUSE } else { icon::PLAY };
        if button(ui, Rect::new(x, y, row, row), &play.to_string()) {
            self.toggle_playback();
        }
        x += row + 2.0;
        if button(ui, Rect::new(x, y, row, row), &icon::CROSS.to_string()) {
            self.playing = false;
            self.rewind();
        }
        x += row + GAP * 2.0;
        if button(ui, Rect::new(x, y, row, row), "-") {
            self.song.step_tempo(-1);
        }
        x += row + 1.0;
        let width = ui.row_width("300 BPM");
        let tempo = Rect::new(x, y, width, row);
        sunken(ui, tempo);
        let text = ui.theme.text;
        ui.text_centered(tempo, &format!("{} BPM", self.song.tempo), text);
        x += width + 1.0;
        if button(ui, Rect::new(x, y, row, row), "+") {
            self.song.step_tempo(1);
        }

        // The song's name at the right of the upper row.
        let title = "Untitled song";
        let at = Vec2::new(bounds.w - ui.row_width(title) - GAP, files + PADDING);
        let color = ui.theme.title;
        ui.text(at, title, color);
    }

    /// One row per channel: its name, and a speaker that mutes it.
    fn draw_channels(&mut self, ui: &mut Ui, bounds: Rect) {
        panel(ui, bounds);
        let row = ui.row_height();
        let muted = ui.theme.muted;
        ui.text(Vec2::new(bounds.x + PADDING, bounds.y + PADDING), "Channels", muted);
        let mut y = bounds.y + row + 1.0;
        for i in 0..self.song.channels.len() {
            if y + row > bounds.bottom() {
                break;
            }
            let name = Rect::new(bounds.x + 1.0, y, bounds.w - row - 3.0, row);
            let speaker = Rect::new(name.right() + 1.0, y, row, row);
            let hovered = ui.hovered(name);
            let chosen = i == self.current;
            if chosen {
                ui.fill(name, ui.theme.sunken);
                ui.outline(name, ui.theme.active);
            } else if hovered {
                ui.outline(name, ui.theme.highlight);
            }
            let channel = &self.song.channels[i];
            let color = if channel.muted {
                ui.theme.muted
            } else if chosen {
                ui.theme.title
            } else {
                ui.theme.text
            };
            ui.text(Vec2::new(name.x + PADDING, name.y + PADDING), &channel.name, color);
            if hovered && ui.input.clicked {
                // A click chooses the channel; the second of a double click
                // opens or closes the roll.
                if self.current != i {
                    self.current_pattern = 0;
                }
                self.current = i;
                if ui.input.double_clicked {
                    self.roll_open = !self.roll_open;
                }
            }
            let channel = &mut self.song.channels[i];
            let sound = if channel.muted { icon::SOUND_OFF } else { icon::SOUND };
            if toggle(ui, speaker, &sound.to_string(), !channel.muted) {
                channel.muted = !channel.muted;
            }
            y += row + 1.0;
        }
    }

    /// How the chosen channel sounds.
    fn draw_instrument(&mut self, ui: &mut Ui, bounds: Rect) {
        let row = ui.row_height();
        if bounds.h < row * 2.0 {
            return;
        }
        panel(ui, bounds);
        let muted = ui.theme.muted;
        ui.text(Vec2::new(bounds.x + PADDING, bounds.y + PADDING), "Instrument", muted);
        let instrument = &mut self.song.channels[self.current].instrument;
        let mut y = bounds.y + row + 1.0;
        for setting in Setting::ALL {
            if y + row > bounds.bottom() {
                break;
            }
            let line = Rect::new(bounds.x + 2.0, y, bounds.w - 4.0, row);
            let turn = stepper(ui, line, setting.label(), &instrument.value(setting));
            if turn != 0 {
                instrument.step(setting, turn);
            }
            y += row + 1.0;
        }
    }

    fn draw_arrangement(&mut self, ui: &mut Ui, bounds: Rect) {
        panel(ui, bounds);
        let row = ui.row_height();
        let muted = ui.theme.muted;
        ui.text(Vec2::new(bounds.x + PADDING, bounds.y + PADDING), "Arrangement", muted);
        let area = Rect::new(bounds.x + 1.0, bounds.y + row, bounds.w - 2.0, bounds.h - row - 1.0);
        let asked = self.arranger.draw(ui, area, &self.song, self.position);
        if let Some((channel, pattern)) = asked.open {
            self.open_roll(channel, pattern);
        }
        if let Some(beats) = asked.scrub {
            self.position = beats;
        }
    }

    fn draw_pattern(&mut self, ui: &mut Ui, bounds: Rect) {
        panel(ui, bounds);
        let row = ui.row_height();
        let channel = &mut self.song.channels[self.current];
        let colour = colors::channel(channel);
        let mut title = format!("{} {}", channel.name, self.current_pattern + 1);
        if channel.muted {
            title.push_str("  Muted");
        }
        let notes = channel.pattern(self.current_pattern).notes().len();
        let count = format!("{notes} {}", if notes == 1 { "note" } else { "notes" });
        let muted = ui.theme.muted;
        ui.text(Vec2::new(bounds.x + PADDING, bounds.y + PADDING), &title, muted);
        ui.text_right(Rect::new(bounds.x, bounds.y, bounds.w, row), &count, muted);

        let area = Rect::new(bounds.x + 1.0, bounds.y + row, bounds.w - 2.0, bounds.h - row - 1.0);
        let pattern = &self.song.channels[self.current].patterns[self.current_pattern];
        if let Some(beats) = self.roll.draw(ui, area, pattern, colour, self.position, self.playing) {
            self.position = beats;
        }
    }

    /// What the program is doing, and the keys worth knowing.
    fn draw_status(&mut self, ui: &mut Ui, bounds: Rect) {
        bar(ui, bounds);
        let beats = self.position as u32;
        let place = format!(
            "{}  Bar {}.{}",
            if self.playing { "Playing" } else { "Stopped" },
            beats / BEATS_PER_BAR + 1,
            beats % BEATS_PER_BAR + 1
        );
        let (left, color) = match self.report() {
            Some(report) => (report.to_string(), ui.theme.title),
            None if self.playing => (place, ui.theme.title),
            None => (place, ui.theme.muted),
        };
        ui.text(Vec2::new(GAP, bounds.y + 2.0), &left, color);
        let keys = if self.roll_open {
            "Wheel pitch   Shift wheel time   Ctrl wheel zoom   Esc close"
        } else {
            "Space play   Enter rewind   Double click channel opens roll"
        };
        let at = Vec2::new(bounds.w - ui.row_width(keys), bounds.y + 2.0);
        let muted = ui.theme.muted;
        ui.text(at, keys, muted);
    }
}

impl App for Studio {
    type Message = ();

    fn update(&mut self, input: &Input, _: &mut Context<()>) {
        if let Some((_, seconds)) = &mut self.report {
            *seconds -= input.dt;
            if *seconds <= 0.0 {
                self.report = None;
            }
        }
        if self.playing {
            self.position += input.dt * self.song.tempo as f32 / 60.0;
        }
        for key in &input.keys {
            match (key.code, key.modifiers) {
                (KeyCode::Char(' '), Modifiers::NONE) => self.toggle_playback(),
                (KeyCode::Enter, _) => self.rewind(),
                (KeyCode::Backspace, m) if m.contains(Modifiers::SHIFT) => self.rewind(),
                (KeyCode::Esc, _) => self.roll_open = false,
                (KeyCode::Char('s'), m) if m.contains(Modifiers::CTRL) => self.not_yet("Saving"),
                (KeyCode::Char('e'), m) if m.contains(Modifiers::CTRL) => self.not_yet("Exporting"),
                (KeyCode::Char('o'), m) if m.contains(Modifiers::CTRL) => self.not_yet("Opening"),
                _ => {}
            }
        }
    }

    fn draw(&mut self, ui: &mut Ui) {
        let layout = self.layout(ui);
        self.draw_transport(ui, layout.transport);
        self.draw_channels(ui, layout.channels);
        self.draw_instrument(ui, layout.instrument);
        if self.roll_open {
            self.draw_pattern(ui, layout.middle);
        } else {
            self.draw_arrangement(ui, layout.middle);
        }
        self.draw_status(ui, layout.status);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use pingo_gui::testing::Harness;

    /// The default window: 1280×720 at scale 2.
    fn harness() -> Harness<Studio> {
        Harness::new(Studio::default(), 640, 360)
    }

    /// Writes the frame to the folder in `PINGO_SHOTS`, for looking at it.
    fn shot(raster: &pingo_gui::testing::Raster, name: &str) {
        if let Some(dir) = std::env::var_os("PINGO_SHOTS") {
            raster.save_png(std::path::Path::new(&dir).join(name), 2).unwrap();
        }
    }

    #[test]
    fn shows_the_arrangement() {
        let mut h = harness();
        let frame = h.frame();
        shot(&frame, "studio.png");
        for text in ["Open", "Export", "120 BPM", "Untitled song", "Channels", "Pulse 1", "Sample", "Instrument", "Square", "Vol", "Arrangement", "Stopped  Bar 1.1"] {
            assert!(frame.find_text(text).is_some(), "{text} is missing");
        }
    }

    #[test]
    fn double_click_on_a_channel_opens_the_roll() {
        let mut h = harness();
        // The second channel's row.
        let y = TRANSPORT_HEIGHT + GAP + 18.0 + 1.0 + 19.0 + 5.0;
        h.click(20.0, y);
        assert_eq!(h.app.current, 1);
        assert!(!h.app.roll_open);
        let frame = h.double_click(20.0, y);
        assert!(h.app.roll_open);
        let frame = h.frame();
        shot(&frame, "roll.png");
        assert!(frame.find_text("Pulse 2 1").is_some());
        assert!(frame.find_text("0 notes").is_some());
        assert!(frame.find_text("C6").is_some(), "the keys are named");
        let _ = frame;
        h.press(KeyEvent::plain(KeyCode::Esc));
        assert!(!h.app.roll_open);
    }

    #[test]
    fn transport_and_keys() {
        let mut h = harness();
        // The + of the tempo, the far end of the lower row.
        let frame = h.frame();
        let at = frame.find_text("120 BPM").unwrap();
        let plus_x = at.x + 60.0;
        let _ = plus_x;
        h.keys(" ");
        assert!(h.app.playing);
        h.frame();
        assert!(h.app.position > 0.0);
        h.press(KeyEvent::plain(KeyCode::Enter));
        assert_eq!(h.app.position, 0.0);
        h.press(KeyEvent::ctrl('s'));
        assert_eq!(h.app.report(), Some("Saving comes in a later version"));
    }

    #[test]
    fn tempo_buttons_and_mute() {
        let mut h = harness();
        let row = 18.0;
        // Lower transport row: play, stop, then - at GAP + 2 * (row + 2) + 2 * GAP.
        let y = 1.0 + row + 1.0 + 5.0;
        let minus = GAP + (row + 2.0) * 2.0 + GAP * 2.0 + 4.0;
        h.click(minus, y);
        assert_eq!(h.app.song.tempo, 115);
        // The speaker of the first channel mutes it.
        let speaker_x = GAP + CHANNEL_WIDTH - 10.0;
        let first = TRANSPORT_HEIGHT + GAP + row + 1.0 + 5.0;
        h.click(speaker_x, first);
        assert!(h.app.song.channels[0].muted);
    }

    #[test]
    fn instrument_steppers_change_the_chosen_channel() {
        let mut h = harness();
        let frame = h.frame();
        let vol = frame.find_text("Vol").unwrap();
        // The + is the last button of the row.
        let plus = Vec2::new(GAP + CHANNEL_WIDTH - 2.0 - 5.0, vol.y);
        h.click(plus.x, plus.y);
        assert_eq!(h.app.song.channels[0].instrument.value(Setting::Volume), "65");
    }

    #[test]
    fn scrolling_the_roll_and_the_arrangement() {
        let mut h = harness();
        h.wheel(400.0, 100.0, -3.0);
        let before = h.app.arranger.scroll;
        assert!(before > 0.0);
        h.app.roll_open = true;
        h.frame();
        let top = h.app.roll.top_pitch;
        h.wheel(400.0, 150.0, 2.0);
        assert_eq!(h.app.roll.top_pitch, (top + 6).min(compose_core::pattern::HIGHEST_PITCH));
        h.input.ctrl = true;
        h.wheel(400.0, 150.0, 1.0);
        assert!(h.app.roll.step_width > 8.0);
    }

    #[test]
    fn survives_tiny_windows() {
        pingo_gui::testing::draw_at_all_sizes(|ui| {
            let mut studio = Studio::default();
            studio.draw(ui);
            studio.roll_open = true;
            studio.draw(ui);
        });
    }
}
