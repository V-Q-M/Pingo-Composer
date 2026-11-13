//! The song of Pingo Composer: channels with their patterns, instruments and
//! arrangement, and the tempo. Plain data and the rules that keep it valid;
//! no colors of a theme, no keys, no pixels (PingoCore docs/FRONTENDS.md).
//!
//! First iteration: the model the screens show. Writing notes, playing and
//! files follow.

pub mod pattern;
pub mod song;

pub use pattern::{Note, Pattern};
pub use song::{Channel, Instrument, Rgb, Setting, Song, Wave};

#[cfg(test)]
mod tests {
    /// The core never imports a frontend.
    #[test]
    fn core_has_no_ui() {
        for entry in std::fs::read_dir(concat!(env!("CARGO_MANIFEST_DIR"), "/src")).unwrap() {
            let path = entry.unwrap().path();
            let text = std::fs::read_to_string(&path).unwrap();
            for banned in ["raylib", "pingo_gui", "pingo_tui", "ratatui"] {
                let needle = format!("use {banned}");
                assert!(!text.contains(&needle), "{} uses {banned}", path.display());
            }
        }
    }
}
