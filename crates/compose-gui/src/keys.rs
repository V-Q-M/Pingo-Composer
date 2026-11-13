//! The composer's keys: the shared vim motions of PingoCore plus its own
//! actions, one layer per pane. Every binding can be changed by name, and
//! the help popup and the hints in the status line come from this table.

use pingo_gui::pingo_ui::input::keymap::{Binding, Keymap, KeymapError, KeysConfig, bind};
use pingo_gui::pingo_ui::input::vim::{self, Nav};

/// What a key (or a click on a hint) asks for.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Action {
    Nav(Nav),
    Play,
    Rewind,
    Help,
    Command,
    /// Closes the roll if it is open, quits otherwise.
    Quit,
    /// Closes whatever is open: the roll, a popup.
    Close,
    Save,
    Export,
    Open,
    /// The chosen channel on or off.
    Mute,
    /// The pattern under the cursor in the roll.
    OpenRoll,
    /// The chosen instrument setting one step up or down.
    Increase,
    Decrease,
    ZoomIn,
    ZoomOut,
}

/// The panes that can have the keyboard, in tab order.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Pane {
    Channels,
    Instrument,
    /// The arrangement, or the roll while it is open.
    Middle,
}

pub const PANES: [Pane; 3] = [Pane::Channels, Pane::Instrument, Pane::Middle];

const GLOBAL: &[Binding<Action>] = &[
    bind("play", Action::Play, &["space"], "play / pause", "Song"),
    bind(
        "rewind",
        Action::Rewind,
        &["enter", "shift-backspace"],
        "rewind",
        "Song",
    ),
    bind("save", Action::Save, &["ctrl-s"], "save", "Files"),
    bind("export", Action::Export, &["ctrl-e"], "export", "Files"),
    bind("open", Action::Open, &["ctrl-o"], "open", "Files"),
    bind("help", Action::Help, &["?"], "help", "General"),
    bind("command", Action::Command, &[":"], "command", "General"),
    bind("close", Action::Close, &["esc"], "close", "General"),
    bind("quit", Action::Quit, &["q"], "close roll / quit", "General"),
];

const CHANNELS: &[Binding<Action>] = &[
    bind("mute", Action::Mute, &["m"], "mute", "Channels"),
    bind(
        "open_roll",
        Action::OpenRoll,
        &["o", "enter"],
        "open roll",
        "Channels",
    ),
];

const INSTRUMENT: &[Binding<Action>] = &[
    bind(
        "increase",
        Action::Increase,
        &["+", "="],
        "more",
        "Instrument",
    ),
    bind("decrease", Action::Decrease, &["-"], "less", "Instrument"),
];

const ARRANGEMENT: &[Binding<Action>] = &[
    bind("mute", Action::Mute, &["m"], "mute", "Arrangement"),
    bind(
        "open_roll",
        Action::OpenRoll,
        &["o", "enter"],
        "open roll",
        "Arrangement",
    ),
];

const ROLL: &[Binding<Action>] = &[
    bind("zoom_in", Action::ZoomIn, &["+", "="], "zoom in", "Roll"),
    bind("zoom_out", Action::ZoomOut, &["-"], "zoom out", "Roll"),
];

/// The layer names, the pane's first.
pub fn layers(pane: Pane, roll_open: bool) -> [&'static str; 2] {
    let first = match pane {
        Pane::Channels => "channels",
        Pane::Instrument => "instrument",
        Pane::Middle if roll_open => "roll",
        Pane::Middle => "arrangement",
    };
    [first, "global"]
}

/// The keymap with the user's `[keys.*]` changes.
pub fn keymap(overrides: &KeysConfig) -> Result<Keymap<Action>, KeymapError> {
    // ctrl-e / ctrl-y stay with export, as in every version of the composer.
    let motions: Vec<Nav> = vim::NAV
        .iter()
        .map(|b| b.action)
        .filter(|n| !matches!(n, Nav::ScrollDown | Nav::ScrollUp))
        .collect();
    let mut global = vim::nav_bindings_for(Action::Nav, &motions);
    global.extend_from_slice(GLOBAL);
    Keymap::builder()
        .layer("channels", CHANNELS)
        .layer("instrument", INSTRUMENT)
        .layer("arrangement", ARRANGEMENT)
        .layer("roll", ROLL)
        .layer("global", global)
        .build(overrides)
}

/// The actions the status line hints at, per pane.
pub fn hinted(pane: Pane, roll_open: bool) -> &'static [Action] {
    match (pane, roll_open) {
        (Pane::Channels, _) => &[
            Action::Nav(Nav::Down),
            Action::OpenRoll,
            Action::Mute,
            Action::Nav(Nav::NextPane),
            Action::Help,
        ],
        (Pane::Instrument, _) => &[
            Action::Nav(Nav::Down),
            Action::Increase,
            Action::Decrease,
            Action::Help,
        ],
        (Pane::Middle, false) => &[
            Action::Nav(Nav::Right),
            Action::OpenRoll,
            Action::Play,
            Action::Help,
        ],
        (Pane::Middle, true) => &[
            Action::Nav(Nav::Right),
            Action::Nav(Nav::Down),
            Action::ZoomIn,
            Action::Quit,
            Action::Help,
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use pingo_gui::pingo_ui::input::dsl::keys;
    use pingo_gui::pingo_ui::input::{Lookup, Pending};

    #[test]
    fn the_default_keymap_builds() {
        keymap(&KeysConfig::new()).unwrap();
    }

    #[test]
    fn panes_shadow_global_keys() {
        let keymap = keymap(&KeysConfig::new()).unwrap();
        let mut pending = Pending::new();
        let enter = keys("<cr>")[0];
        let lookup =
            |layers: [&str; 2], pending: &mut Pending| keymap.lookup(&layers, enter, pending);
        assert_eq!(
            lookup(layers(Pane::Channels, false), &mut pending),
            Lookup::Action {
                action: Action::OpenRoll,
                count: None
            }
        );
        assert_eq!(
            lookup(layers(Pane::Instrument, false), &mut pending),
            Lookup::Action {
                action: Action::Rewind,
                count: None
            }
        );
    }

    #[test]
    fn every_hint_has_a_key() {
        let keymap = keymap(&KeysConfig::new()).unwrap();
        for pane in PANES {
            for open in [false, true] {
                let hints = keymap.hints(&layers(pane, open), hinted(pane, open));
                assert_eq!(hints.len(), hinted(pane, open).len(), "{pane:?} {open}");
            }
        }
    }
}
