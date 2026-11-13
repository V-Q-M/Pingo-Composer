//! The studio: the one screen the composer has.
//!
//! The transport bar on top, the channels and the instrument of the chosen
//! one on the left, the status line at the bottom. In the middle the
//! arrangement, or the piano roll of a pattern.
//!
//! Everything works with the mouse and with vim keys (see `keys.rs`): `tab`
//! walks the panes (a click focuses one too), `j` `k` `gg` `G` and counts
//! move in every pane, the arrangement and the roll have a cursor cell, `o`
//! opens the roll and `q` closes it, `?` lists every key, `:` takes commands.
//!
//! First iteration: everything can be looked at and walked through, but
//! nothing sounds and no notes are written. Play runs the playhead silently.

use compose_core::Song;
use compose_core::pattern::BEATS_PER_BAR;
use compose_core::song::{BARS, Setting};
use pingo_gui::GuiError;
use pingo_gui::font::FontChoice;
use pingo_gui::pingo_ui::components::focus::FocusRing;
use pingo_gui::pingo_ui::components::help::Help;
use pingo_gui::pingo_ui::components::list::VimList;
use pingo_gui::pingo_ui::components::settings::{SettingsAction, SettingsMenu};
use pingo_gui::pingo_ui::input::KeyStyle;
use pingo_gui::pingo_ui::input::keymap::{Keymap, KeysConfig, Lookup};
use pingo_gui::pingo_ui::input::vim::{self, Nav};
use pingo_gui::pingo_ui::input::{CmdAction, CmdLine, Pending, completion};
use pingo_gui::prelude::*;
use pingo_gui::theme::Look;
use pingo_gui::ui::PADDING;
use pingo_gui::widgets::draw_settings;
use pingo_gui::widgets::help::draw_help;
use pingo_gui::widgets::hints::{HintStyle, draw_hint_buttons};
use pingo_gui::widgets::input::{InputStyle, draw_input};
use pingo_gui::widgets::{Scrollbar, bar, button, panel, stepper, sunken, toggle};

use crate::arranger::Arranger;
use crate::colors;
use crate::keys::{self, Action, PANES, Pane};
use crate::piano_roll::PianoRoll;
use crate::prefs::{Pref, Prefs, SECTIONS};

/// Heights of the bars and the width of the channel list, in canvas pixels.
/// The transport holds two rows: files above, playing below.
pub const TRANSPORT_HEIGHT: f32 = 39.0;
pub const STATUS_HEIGHT: f32 = 15.0;
pub const CHANNEL_WIDTH: f32 = 104.0;
pub const GAP: f32 = 3.0;

/// How long an answer stays in the status line.
const REPORT_SECONDS: f32 = 4.0;

/// Names `:` completes.
const COMMANDS: [&str; 10] = [
    "help", "play", "quit", "stop", "tempo", "bar", "set", "look", "font", "keys",
];

/// Where everything sits, worked out from the canvas every frame.
struct Layout {
    transport: Rect,
    channels: Rect,
    instrument: Rect,
    middle: Rect,
    status: Rect,
}

/// What the keyboard talks to besides the panes.
enum Mode {
    Normal,
    Help(Help),
    Command,
    Settings(SettingsMenu<Pref>),
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
    /// The chosen row of the instrument panel.
    pub setting: usize,
    pub focus: FocusRing<Pane>,
    keymap: Keymap<Action>,
    pending: Pending,
    mode: Mode,
    cmd: CmdLine,
    roll: PianoRoll,
    arranger: Arranger,
    help_bar: Scrollbar,
    report: Option<(String, f32)>,
    quit: bool,
    /// Look, font and key style.
    pub prefs: Prefs,
    /// Look and font changes waiting for the next `update`, which can
    /// hand them to the window.
    apply: Vec<Pref>,
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
            setting: 0,
            focus: FocusRing::new(&PANES),
            keymap: keys::keymap(&KeysConfig::new(), KeyStyle::Vim)
                .expect("the default keys are valid"),
            pending: Pending::new(),
            mode: Mode::Normal,
            cmd: CmdLine::new(),
            roll: PianoRoll::default(),
            arranger: Arranger::default(),
            help_bar: Scrollbar::default(),
            report: None,
            quit: false,
            prefs: Prefs::default(),
            apply: Vec::new(),
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

    #[cfg(test)]
    pub fn help_open(&self) -> bool {
        matches!(self.mode, Mode::Help(_))
    }

    pub fn command_open(&self) -> bool {
        matches!(self.mode, Mode::Command)
    }

    fn layers(&self) -> [&'static str; 2] {
        keys::layers(self.focus.current(), self.roll_open)
    }

    fn open_roll(&mut self, channel: usize, pattern: usize) {
        self.current = channel;
        self.current_pattern = pattern;
        self.roll_open = true;
        self.focus.set(Pane::Middle);
    }

    fn not_yet(&mut self, what: &str) {
        self.say(format!("{what} comes in a later version"));
    }

    /// The pattern of the block under the arrangement cursor, if any.
    fn block_at_cursor(&self) -> Option<usize> {
        let bar = self.arranger.cursor_bar;
        self.song.channels[self.current]
            .blocks()
            .find(|(start, _, len)| (*start..start + len).contains(&bar))
            .map(|(_, pattern, _)| pattern)
    }

    /// A list motion on a list of `len` rows with `selected` chosen.
    fn walk(len: usize, selected: usize, nav: Nav, count: Option<usize>) -> usize {
        let Some(motion) = vim::list_motion(nav, count) else {
            return selected;
        };
        let mut list = VimList::new(len);
        list.set_height(len);
        list.select(selected);
        list.apply(motion);
        list.selected().unwrap_or(selected)
    }

    /// Does what a key or a clicked hint asks for.
    fn act(&mut self, action: Action, count: Option<usize>) {
        let n = count.unwrap_or(1).max(1);
        match action {
            Action::Nav(Nav::NextPane) => {
                self.focus.focus_next();
            }
            Action::Nav(Nav::PrevPane) => {
                self.focus.focus_prev();
            }
            Action::Nav(nav) => self.nav(nav, count),
            Action::Play => self.playing = !self.playing,
            Action::Rewind => self.position = 0.0,
            Action::Help => self.mode = Mode::Help(Help::new(self.keymap.help(&self.layers()))),
            Action::Command => self.mode = Mode::Command,
            Action::Quit if self.roll_open => self.roll_open = false,
            Action::Quit => self.quit = true,
            Action::Close => self.roll_open = false,
            Action::Save => self.not_yet("Saving"),
            Action::Export => self.not_yet("Exporting"),
            Action::Open => self.not_yet("Opening"),
            Action::Mute => {
                let channel = &mut self.song.channels[self.current];
                channel.muted = !channel.muted;
            }
            Action::OpenRoll => {
                let pattern = match self.focus.current() {
                    Pane::Middle => self.block_at_cursor().unwrap_or(0),
                    _ => self.current_pattern,
                };
                self.open_roll(self.current, pattern);
            }
            Action::Increase | Action::Decrease => {
                let turn = if action == Action::Increase {
                    n as i32
                } else {
                    -(n as i32)
                };
                self.step_setting(turn);
            }
            Action::ZoomIn => self.roll.zoom(n as i32),
            Action::ZoomOut => self.roll.zoom(-(n as i32)),
            Action::Settings => self.mode = Mode::Settings(SettingsMenu::new(SECTIONS)),
        }
    }

    fn step_setting(&mut self, turn: i32) {
        let setting = Setting::ALL[self.setting];
        let instrument = &mut self.song.channels[self.current].instrument;
        for _ in 0..turn.unsigned_abs() {
            instrument.step(setting, turn.signum());
        }
    }

    /// A vim motion in the focused pane.
    fn nav(&mut self, nav: Nav, count: Option<usize>) {
        let n = count.unwrap_or(1).max(1) as isize;
        let channels = self.song.channels.len();
        match self.focus.current() {
            Pane::Channels => self.current = Self::walk(channels, self.current, nav, count),
            Pane::Instrument => match nav {
                Nav::Left => self.step_setting(-(n as i32)),
                Nav::Right => self.step_setting(n as i32),
                _ => self.setting = Self::walk(Setting::ALL.len(), self.setting, nav, count),
            },
            Pane::Middle if self.roll_open => {
                let end = self.song.channels[self.current].patterns[self.current_pattern].length();
                self.roll.nav(nav, count, end);
            }
            Pane::Middle => {
                let cursor = self.arranger.cursor_bar as isize;
                let page = self.arranger.visible_bars() as isize;
                match nav {
                    Nav::Left => self.arranger.set_cursor(cursor - n),
                    Nav::Right => self.arranger.set_cursor(cursor + n),
                    Nav::LineStart => self.arranger.set_cursor(0),
                    Nav::LineEnd => {
                        let end = self
                            .song
                            .channels
                            .iter()
                            .flat_map(|c| c.blocks().map(|(start, _, len)| start + len))
                            .max()
                            .unwrap_or(BARS);
                        self.arranger.set_cursor(end as isize - 1);
                    }
                    Nav::HalfPageDown => self.arranger.set_cursor(cursor + page / 2),
                    Nav::HalfPageUp => self.arranger.set_cursor(cursor - page / 2),
                    Nav::PageDown => self.arranger.set_cursor(cursor + page),
                    Nav::PageUp => self.arranger.set_cursor(cursor - page),
                    Nav::AlignCenter => self.arranger.center_cursor(),
                    _ => self.current = Self::walk(channels, self.current, nav, count),
                }
            }
        }
    }

    fn command(&mut self, name: &str, args: &[String]) {
        match name {
            "q" | "quit" => self.quit = true,
            "help" => self.act(Action::Help, None),
            "set" => self.act(Action::Settings, None),
            "look" => match args.first().map(|a| a.parse::<Look>()) {
                Some(Ok(look)) => {
                    self.prefs.look = look;
                    self.changed(Pref::Look);
                }
                Some(Err(e)) => self.say(e),
                None => self.say("usage: :look pingo|thinkos|terminal|mono"),
            },
            "font" => match args.first().map(|a| a.parse::<FontChoice>()) {
                Some(Ok(font)) => {
                    self.prefs.font = font;
                    self.changed(Pref::Font);
                }
                Some(Err(e)) => self.say(e.to_string()),
                None => self.say("usage: :font auto|pingo|mono|sans|PATH"),
            },
            "keys" => match args.first().map(|a| a.parse::<KeyStyle>()) {
                Some(Ok(style)) => {
                    self.prefs.keys = style;
                    self.changed(Pref::Keys);
                }
                Some(Err(e)) => self.say(e),
                None => self.say("usage: :keys vim|simple"),
            },
            "play" => self.playing = true,
            "stop" => self.playing = false,
            "tempo" => match args.first().and_then(|a| a.parse::<u32>().ok()) {
                Some(tempo) => {
                    let tempo =
                        tempo.clamp(compose_core::song::TEMPO_MIN, compose_core::song::TEMPO_MAX);
                    self.song.tempo = tempo;
                }
                None => self.say("usage: :tempo 40-300"),
            },
            "bar" => match args.first().and_then(|a| a.parse::<usize>().ok()) {
                Some(bar) => self.jump_to_bar(bar),
                None => self.say("usage: :bar N"),
            },
            n if n.parse::<usize>().is_ok() => self.jump_to_bar(n.parse().unwrap_or(1)),
            other => self.say(format!("Unknown command :{other}")),
        }
    }

    /// `:12`: the playhead and the cursor to bar 12.
    fn jump_to_bar(&mut self, bar: usize) {
        let bar = bar.clamp(1, BARS) - 1;
        self.position = (bar as u32 * BEATS_PER_BAR) as f32;
        self.arranger.set_cursor(bar as isize);
    }

    /// A preference changed: keys apply now, look and font at the next
    /// `update` (the window loads the font).
    fn changed(&mut self, pref: Pref) {
        match pref {
            Pref::Keys => {
                self.keymap = keys::keymap(&KeysConfig::new(), self.prefs.keys)
                    .expect("the default keys are valid");
                self.pending.clear();
            }
            Pref::Look | Pref::Font => self.apply.push(pref),
        }
    }

    #[cfg(test)]
    pub fn key_style(&self) -> KeyStyle {
        self.keymap.style()
    }

    #[cfg(test)]
    pub fn settings_open(&self) -> bool {
        matches!(self.mode, Mode::Settings(_))
    }

    fn handle_key(&mut self, key: KeyEvent) {
        match &mut self.mode {
            Mode::Settings(menu) => match menu.handle_key(key, &mut self.prefs) {
                SettingsAction::Changed(pref) => self.changed(pref),
                SettingsAction::Close => self.mode = Mode::Normal,
                SettingsAction::None => {}
            },
            Mode::Help(help) => {
                let lookup = self.keymap.lookup(&["global"], key, &mut self.pending);
                match lookup {
                    Lookup::Action {
                        action: Action::Nav(Nav::Down),
                        count,
                    } => help.scroll_by(count.unwrap_or(1) as isize, 0),
                    Lookup::Action {
                        action: Action::Nav(Nav::Up),
                        count,
                    } => help.scroll_by(-(count.unwrap_or(1) as isize), 0),
                    Lookup::Action {
                        action: Action::Nav(Nav::Top),
                        ..
                    } => help.scroll_to(true, 0),
                    Lookup::Action {
                        action: Action::Nav(Nav::Bottom),
                        ..
                    } => help.scroll_to(false, 0),
                    Lookup::Pending => {}
                    _ => self.mode = Mode::Normal,
                }
            }
            Mode::Command => match self.cmd.handle_key(key) {
                CmdAction::Submit(command) => {
                    self.mode = Mode::Normal;
                    self.command(&command.name, &command.args);
                }
                CmdAction::Complete { .. } => {
                    if let Some(name) = completion::completion(&COMMANDS, self.cmd.input.text()) {
                        self.cmd.input.set_text(name);
                    }
                }
                CmdAction::Close => self.mode = Mode::Normal,
                CmdAction::Continue => {}
            },
            Mode::Normal => {
                let layers = self.layers();
                if let Lookup::Action { action, count } =
                    self.keymap.lookup(&layers, key, &mut self.pending)
                {
                    self.act(action, count);
                }
            }
        }
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
        Layout {
            transport,
            channels,
            instrument,
            middle,
            status,
        }
    }

    /// A panel with its title; the focused one has a highlighted border.
    fn pane(&self, ui: &mut Ui, rect: Rect, title: &str, pane: Pane) {
        panel(ui, rect);
        let focused = self.focus.is(pane);
        if focused {
            ui.outline(rect, ui.theme.active);
        }
        let color = if focused {
            ui.theme.title
        } else {
            ui.theme.muted
        };
        ui.text(Vec2::new(rect.x + PADDING, rect.y + PADDING), title, color);
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
            self.act(Action::Open, None);
        }
        x += open + 2.0;
        let export = ui.row_width("Export");
        if button(ui, Rect::new(x, files, export, row), "Export") {
            self.act(Action::Export, None);
        }
        x += export + 2.0;
        if button(ui, Rect::new(x, files, row, row), &icon::SAVE.to_string()) {
            self.act(Action::Save, None);
        }
        x += row + GAP * 2.0;
        let settings = ui.row_width("Settings");
        if button(ui, Rect::new(x, files, settings, row), "Settings") {
            self.act(Action::Settings, None);
        }

        // Playing in the lower row.
        let mut x = GAP;
        let play = if self.playing {
            icon::PAUSE
        } else {
            icon::PLAY
        };
        if button(ui, Rect::new(x, y, row, row), &play.to_string()) {
            self.act(Action::Play, None);
        }
        x += row + 2.0;
        if button(ui, Rect::new(x, y, row, row), &icon::CROSS.to_string()) {
            self.playing = false;
            self.position = 0.0;
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
        self.pane(ui, bounds, "Channels", Pane::Channels);
        let row = ui.row_height();
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
            ui.text(
                Vec2::new(name.x + PADDING, name.y + PADDING),
                &channel.name,
                color,
            );
            if hovered && ui.input.clicked {
                // A click chooses the channel; the second of a double click
                // opens or closes the roll.
                if self.current != i {
                    self.current_pattern = 0;
                }
                self.current = i;
                if ui.input.double_clicked {
                    if self.roll_open {
                        self.roll_open = false;
                    } else {
                        self.open_roll(i, self.current_pattern);
                    }
                }
            }
            let channel = &mut self.song.channels[i];
            let sound = if channel.muted {
                icon::SOUND_OFF
            } else {
                icon::SOUND
            };
            if toggle(ui, speaker, &sound.to_string(), !channel.muted) {
                channel.muted = !channel.muted;
            }
            y += row + 1.0;
        }
    }

    /// How the chosen channel sounds. The chosen row is what `+` `-` `h` `l`
    /// change.
    fn draw_instrument(&mut self, ui: &mut Ui, bounds: Rect) {
        let row = ui.row_height();
        if bounds.h < row * 2.0 {
            return;
        }
        self.pane(ui, bounds, "Instrument", Pane::Instrument);
        let focused = self.focus.is(Pane::Instrument);
        let mut y = bounds.y + row + 1.0;
        for (i, setting) in Setting::ALL.into_iter().enumerate() {
            if y + row > bounds.bottom() {
                break;
            }
            let line = Rect::new(bounds.x + 2.0, y, bounds.w - 4.0, row);
            if focused && i == self.setting {
                ui.fill(line, ui.theme.sunken);
                ui.outline(line, ui.theme.highlight);
            }
            let instrument = &mut self.song.channels[self.current].instrument;
            let turn = stepper(
                ui,
                line.inset(1.0),
                setting.label(),
                &instrument.value(setting),
            );
            if turn != 0 {
                instrument.step(setting, turn);
                self.setting = i;
            }
            y += row + 1.0;
        }
    }

    fn draw_arrangement(&mut self, ui: &mut Ui, bounds: Rect) {
        self.pane(ui, bounds, "Arrangement", Pane::Middle);
        let row = ui.row_height();
        let area = Rect::new(
            bounds.x + 1.0,
            bounds.y + row,
            bounds.w - 2.0,
            bounds.h - row - 1.0,
        );
        let cursor = self.focus.is(Pane::Middle).then_some(self.current);
        let asked = self
            .arranger
            .draw(ui, area, &self.song, self.position, cursor);
        if let Some((channel, _)) = asked.cursor {
            self.current = channel;
        }
        if let Some((channel, pattern)) = asked.open {
            self.open_roll(channel, pattern);
        }
        if let Some(beats) = asked.scrub {
            self.position = beats;
        }
    }

    fn draw_pattern(&mut self, ui: &mut Ui, bounds: Rect) {
        let row = ui.row_height();
        let channel = &mut self.song.channels[self.current];
        let colour = colors::channel(channel);
        let mut title = format!("{} {}", channel.name, self.current_pattern + 1);
        if channel.muted {
            title.push_str("  Muted");
        }
        let notes = channel.pattern(self.current_pattern).notes().len();
        self.pane(ui, bounds, &title, Pane::Middle);
        let count = format!("{notes} {}", if notes == 1 { "note" } else { "notes" });
        let muted = ui.theme.muted;
        ui.text_right(Rect::new(bounds.x, bounds.y, bounds.w, row), &count, muted);

        let area = Rect::new(
            bounds.x + 1.0,
            bounds.y + row,
            bounds.w - 2.0,
            bounds.h - row - 1.0,
        );
        let pattern = &self.song.channels[self.current].patterns[self.current_pattern];
        let focused = self.focus.is(Pane::Middle);
        if let Some(beats) = self.roll.draw(
            ui,
            area,
            pattern,
            colour,
            self.position,
            self.playing,
            focused,
        ) {
            self.position = beats;
        }
    }

    /// What the program is doing on the left, the keys of the focused pane
    /// on the right (clickable), or the `:` line.
    fn draw_status(&mut self, ui: &mut Ui, bounds: Rect) {
        bar(ui, bounds);
        if self.command_open() {
            let line = Rect::new(bounds.x, bounds.y + 1.0, bounds.w, bounds.h - 1.0);
            let style = InputStyle {
                prefix: ":",
                placeholder: "",
                ghost: None,
                focused: true,
            };
            draw_input(ui, line, &mut self.cmd.input, style);
            return;
        }
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
        let used = ui.row_width(&left) + 2.0 * GAP;
        let hints_rect = Rect::new(
            used,
            bounds.y + 1.0,
            (bounds.w - used).max(0.0),
            bounds.h - 1.0,
        );
        let wanted = keys::hinted(self.focus.current(), self.roll_open);
        let (hints, actions) = self.keymap.hints_with_actions(&self.layers(), wanted);
        let showcmd = self.pending.showcmd();
        if let Some(i) = draw_hint_buttons(ui, hints_rect, &hints, HintStyle::Inline, &showcmd)
            && let Some(action) = actions.get(i)
        {
            self.act(*action, None);
        }
    }

    /// Everything but the popups.
    fn draw_screen(&mut self, ui: &mut Ui, layout: &Layout) {
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

    /// A click on a pane gives it the keyboard.
    fn focus_on_click(&mut self, ui: &Ui, layout: &Layout) {
        if !ui.input.clicked {
            return;
        }
        let mouse = ui.input.mouse;
        for (rect, pane) in [
            (layout.channels, Pane::Channels),
            (layout.instrument, Pane::Instrument),
            (layout.middle, Pane::Middle),
        ] {
            if rect.contains(mouse) {
                self.focus.set(pane);
            }
        }
    }
}

impl App for Studio {
    type Message = ();

    fn update(&mut self, input: &Input, cx: &mut Context<()>) {
        if let Some((_, seconds)) = &mut self.report {
            *seconds -= input.dt;
            if *seconds <= 0.0 {
                self.report = None;
            }
        }
        for key in &input.keys {
            self.handle_key(*key);
        }
        for pref in std::mem::take(&mut self.apply) {
            match pref {
                Pref::Look => cx.set_look(self.prefs.look),
                _ => cx.set_font(self.prefs.font.clone()),
            }
        }
        if self.playing {
            self.position += input.dt * self.song.tempo as f32 / 60.0;
        }
        if self.quit {
            cx.quit();
        }
    }

    fn draw(&mut self, ui: &mut Ui) {
        let layout = self.layout(ui);
        if matches!(self.mode, Mode::Help(_) | Mode::Settings(_)) {
            // Under a popup nothing takes the mouse.
            ui.without_pointer(|ui| self.draw_screen(ui, &layout));
        } else {
            self.focus_on_click(ui, &layout);
            self.draw_screen(ui, &layout);
        }
        if let Mode::Help(help) = &mut self.mode {
            let area = ui.area();
            if draw_help(ui, area, help, &mut self.help_bar) {
                self.mode = Mode::Normal;
            }
        }
        if let Mode::Settings(menu) = &mut self.mode {
            let area = ui.area();
            match draw_settings(ui, area, menu, &mut self.prefs) {
                SettingsAction::Changed(pref) => self.changed(pref),
                SettingsAction::Close => self.mode = Mode::Normal,
                SettingsAction::None => {}
            }
        }
    }

    fn font_failed(&mut self, error: GuiError) {
        self.say(format!("Font: {error}"));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use compose_core::song::BARS;
    use pingo_gui::testing::Harness;

    /// The default window: 1280×720 at scale 2.
    fn harness() -> Harness<Studio> {
        Harness::new(Studio::default(), 640, 360)
    }

    /// Writes the frame to the folder in `PINGO_SHOTS`, for looking at it.
    fn shot(raster: &pingo_gui::testing::Raster, name: &str) {
        if let Some(dir) = std::env::var_os("PINGO_SHOTS") {
            raster
                .save_png(std::path::Path::new(&dir).join(name), 2)
                .unwrap();
        }
    }

    #[test]
    fn shows_the_arrangement() {
        let mut h = harness();
        let frame = h.frame();
        shot(&frame, "studio.png");
        for text in [
            "Open",
            "Export",
            "120 BPM",
            "Untitled song",
            "Channels",
            "Pulse 1",
            "Sample",
            "Instrument",
            "Square",
            "Vol",
            "Arrangement",
            "Stopped  Bar 1.1",
        ] {
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
        h.double_click(20.0, y);
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
        // Enter opens the roll in the channels pane; shift-backspace rewinds
        // everywhere.
        h.keys(" ");
        h.press(KeyEvent::new(KeyCode::Backspace, Modifiers::SHIFT));
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
        assert_eq!(
            h.app.song.channels[0].instrument.value(Setting::Volume),
            "65"
        );
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
        assert_eq!(
            h.app.roll.top_pitch,
            (top + 6).min(compose_core::pattern::HIGHEST_PITCH)
        );
        h.input.ctrl = true;
        h.wheel(400.0, 150.0, 1.0);
        assert!(h.app.roll.step_width > 8.0);
    }

    #[test]
    fn tab_and_clicks_move_the_focus() {
        let mut h = harness();
        assert!(h.app.focus.is(Pane::Channels));
        h.keys("<tab>");
        assert!(h.app.focus.is(Pane::Instrument));
        h.keys("<tab>");
        assert!(h.app.focus.is(Pane::Middle));
        h.keys("<s-tab><s-tab>");
        assert!(h.app.focus.is(Pane::Channels));
        h.click(400.0, 200.0);
        assert!(h.app.focus.is(Pane::Middle));
    }

    #[test]
    fn channels_by_vim_keys() {
        let mut h = harness();
        h.keys("3j");
        assert_eq!(h.app.current, 3);
        h.keys("G");
        assert_eq!(h.app.current, 4);
        h.keys("gg");
        assert_eq!(h.app.current, 0);
        h.keys("2G");
        assert_eq!(h.app.current, 1);
        h.keys("m");
        assert!(h.app.song.channels[1].muted);
        h.keys("o");
        assert!(h.app.roll_open);
        assert!(h.app.focus.is(Pane::Middle));
        h.keys("q");
        assert!(!h.app.roll_open);
        assert!(!h.quit);
        h.keys("q");
        assert!(h.quit);
    }

    #[test]
    fn instrument_by_vim_keys() {
        let mut h = harness();
        h.keys("<tab>j+");
        assert_eq!(h.app.setting, 1);
        assert_eq!(
            h.app.song.channels[0].instrument.value(Setting::Volume),
            "65"
        );
        h.keys("3h");
        assert_eq!(
            h.app.song.channels[0].instrument.value(Setting::Volume),
            "50"
        );
        h.keys("ggl");
        assert_eq!(
            h.app.song.channels[0].instrument.value(Setting::Wave),
            "Pulse"
        );
        let frame = h.frame();
        shot(&frame, "instrument-focus.png");
    }

    #[test]
    fn arrangement_cursor() {
        let mut h = harness();
        h.keys("<tab><tab>5l2j");
        assert_eq!((h.app.arranger.cursor_bar, h.app.current), (5, 2));
        h.keys("$");
        assert_eq!(h.app.arranger.cursor_bar, BARS - 1);
        assert!(h.app.arranger.scroll > 0.0, "the view follows the cursor");
        h.keys("^");
        assert_eq!((h.app.arranger.cursor_bar, h.app.arranger.scroll), (0, 0.0));
        let frame = h.keys("3l");
        shot(&frame, "arrangement-cursor.png");
        // No block under the cursor: the channel's first pattern opens.
        h.keys("<cr>");
        assert!(h.app.roll_open);
        assert_eq!((h.app.current, h.app.current_pattern), (2, 0));
    }

    #[test]
    fn roll_cursor_and_zoom() {
        let mut h = harness();
        h.keys("o");
        let pitch = h.app.roll.cursor_pitch;
        h.keys("10k4l");
        assert_eq!(h.app.roll.cursor_pitch, pitch + 10);
        assert_eq!(h.app.roll.cursor_step, 4);
        h.keys("+");
        assert!(h.app.roll.step_width > 8.0);
        let frame = h.keys("zz");
        shot(&frame, "roll-cursor.png");
        h.keys("<esc>");
        assert!(!h.app.roll_open);
    }

    #[test]
    fn help_and_commands() {
        let mut h = harness();
        let frame = h.keys("?");
        assert!(h.app.help_open());
        shot(&frame, "help.png");
        assert!(frame.find_text("half page down").is_some());
        h.keys("j");
        assert!(h.app.help_open(), "j scrolls the help");
        h.keys("q");
        assert!(!h.app.help_open());
        assert!(!h.quit, "q only closed the help");
        let frame = h.keys(":tempo 140");
        assert!(h.app.command_open());
        shot(&frame, "command.png");
        h.keys("<cr>");
        assert_eq!(h.app.song.tempo, 140);
        h.keys(":12<cr>");
        assert_eq!(h.app.position, 44.0);
        h.keys(":nope<cr>");
        assert_eq!(h.app.report(), Some("Unknown command :nope"));
    }

    #[test]
    fn clicks_do_not_fall_through_the_help() {
        let mut h = harness();
        h.keys("?");
        // The second channel row, left of the popup: while help is open it
        // must not react.
        let y = TRANSPORT_HEIGHT + GAP + 18.0 + 1.0 + 19.0 + 5.0;
        h.click(20.0, y);
        assert_eq!(h.app.current, 0);
        assert!(h.app.help_open());
    }

    #[test]
    fn hints_are_clickable() {
        let mut h = harness();
        let frame = h.frame();
        // The channels pane hints at "o open roll".
        let at = frame.find_text("open roll").expect("hint shown");
        h.click(at.x + 4.0, at.y + 4.0);
        assert!(h.app.roll_open);
    }

    #[test]
    fn settings_popup_changes_look_font_and_keys() {
        let mut h = harness();
        let frame = h.keys("<f2>");
        assert!(h.app.settings_open());
        shot(&frame, "settings.png");
        // Look is the first row: l cycles it, the window gets the theme.
        h.keys("l");
        h.frame();
        assert_eq!(h.app.prefs.look, Look::ALL[1]);
        assert_eq!(h.theme, Look::ALL[1].theme());
        // Vim keys off: the keymap drops the letters.
        h.keys("jjl");
        assert_eq!(h.app.key_style(), KeyStyle::Simple);
        h.keys("<esc>");
        assert!(!h.app.settings_open());
        h.keys("j");
        assert_eq!(h.app.current, 0, "j does nothing in simple keys");
        h.keys("<down>");
        assert_eq!(h.app.current, 1);
        let frame = h.frame();
        shot(&frame, "simple-keys.png");
        assert!(frame.find_text("open roll").is_some());
        assert!(frame.find_text("f1").is_some() || frame.find_text("help").is_some());
        h.keys("<cr>");
        assert!(h.app.roll_open, "enter opens the roll in simple keys");
        h.keys("<esc>");
        assert!(!h.app.roll_open);
    }

    #[test]
    fn the_settings_button_opens_the_popup() {
        let mut h = harness();
        let frame = h.frame();
        let at = frame.find_text("Settings").expect("button shown");
        h.click(at.x + 4.0, at.y + 4.0);
        assert!(h.app.settings_open());
        // Nothing under the popup reacts.
        let y = TRANSPORT_HEIGHT + GAP + 18.0 + 1.0 + 19.0 + 5.0;
        h.click(20.0, y);
        assert_eq!(h.app.current, 0);
    }

    #[test]
    fn look_and_font_commands() {
        let mut h = harness();
        h.keys(":look terminal<cr>");
        h.frame();
        assert_eq!(h.theme, "terminal".parse::<Look>().unwrap().theme());
        h.keys(":font /nowhere/missing.ttf<cr>");
        h.frame();
        let report = h.app.report().unwrap_or_default().to_string();
        assert!(report.starts_with("Font:"), "{report}");
        // A system font, where this machine has one: the smooth style.
        if pingo_gui::font::system_font(&FontChoice::Mono).is_some() {
            h.keys(":font mono<cr>");
            let frame = h.frame();
            shot(&frame, "mono-font.png");
            assert_eq!(h.app.prefs.font, FontChoice::Mono);
        }
        h.keys(":keys simple<cr>");
        assert_eq!(h.app.key_style(), KeyStyle::Simple);
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
