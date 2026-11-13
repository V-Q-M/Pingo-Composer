//! The composer's preferences: look, font and keys, shown in the settings
//! popup (`f2`, `:set`, the Settings button). Look and font are separate:
//! the font `auto` follows the look (Pingo look → Pingo pixel font), any
//! other font choice wins whatever the look.

use pingo_gui::font::FontChoice;
use pingo_gui::pingo_ui::components::settings::{
    Direction, SavedSetting, SettingItem, SettingValue, cycle,
};
use pingo_gui::pingo_ui::input::KeyStyle;
use pingo_gui::theme::Look;

/// The fonts the popup cycles through; a file can be given with `:font PATH`.
pub const FONTS: [FontChoice; 4] = [
    FontChoice::Auto,
    FontChoice::Pingo,
    FontChoice::Mono,
    FontChoice::Sans,
];

#[derive(Clone, Debug, PartialEq)]
pub struct Prefs {
    pub look: Look,
    pub font: FontChoice,
    pub keys: KeyStyle,
}

impl Default for Prefs {
    fn default() -> Prefs {
        Prefs {
            look: Look::Pingo,
            font: FontChoice::Auto,
            keys: KeyStyle::Vim,
        }
    }
}

/// One line of the settings popup.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Pref {
    Look,
    Font,
    Keys,
}

pub const SECTIONS: &[(&str, &[Pref])] = &[
    ("Appearance", &[Pref::Look, Pref::Font]),
    ("Keyboard", &[Pref::Keys]),
];

impl SettingItem<Prefs> for Pref {
    fn label(&self) -> &'static str {
        match self {
            Pref::Look => "Look",
            Pref::Font => "Font",
            Pref::Keys => "Vim keys",
        }
    }

    fn help(&self) -> &'static str {
        match self {
            Pref::Look => {
                "The colors. The Pingo look comes with the Pingo pixel font when the font is auto."
            }
            Pref::Font => {
                "Auto follows the look. Pingo draws pixel art, Mono and Sans are system fonts."
            }
            Pref::Keys => {
                "Off: only arrows, tab, enter, esc and space; everything else with the mouse."
            }
        }
    }

    fn value(&self, prefs: &Prefs) -> String {
        match self {
            Pref::Look => prefs.look.name().to_string(),
            Pref::Font => prefs.font.to_string(),
            Pref::Keys => if prefs.keys == KeyStyle::Vim {
                "on"
            } else {
                "off"
            }
            .to_string(),
        }
    }

    fn change(&self, prefs: &mut Prefs, direction: Direction) {
        match self {
            Pref::Look => {
                let at = Look::ALL.iter().position(|l| *l == prefs.look).unwrap_or(0);
                prefs.look = Look::ALL[cycle(at, Look::ALL.len(), direction, true)];
            }
            Pref::Font => {
                let at = FONTS.iter().position(|f| *f == prefs.font).unwrap_or(0);
                prefs.font = FONTS[cycle(at, FONTS.len(), direction, true)].clone();
            }
            Pref::Keys => prefs.keys = prefs.keys.toggled(),
        }
    }

    fn save(&self, prefs: &Prefs) -> SavedSetting {
        let (section, key, value) = match self {
            Pref::Look => ("theme", "look", prefs.look.name().to_string()),
            Pref::Font => ("theme", "font", prefs.font.to_string()),
            Pref::Keys => ("keys", "style", prefs.keys.name().to_string()),
        };
        SavedSetting {
            section,
            key,
            value: SettingValue::Text(value),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_pref_cycles_and_comes_back() {
        let mut prefs = Prefs::default();
        for pref in [Pref::Look, Pref::Font, Pref::Keys] {
            let before = pref.value(&prefs);
            pref.change(&mut prefs, Direction::Next);
            assert_ne!(pref.value(&prefs), before, "{pref:?}");
            pref.change(&mut prefs, Direction::Prev);
            assert_eq!(pref.value(&prefs), before, "{pref:?}");
        }
        assert_eq!(Pref::Keys.save(&prefs).key, "style");
    }
}
