//! Notes and patterns. Time is counted in steps, pitch like in MIDI: 60 is
//! C4.

/// A beat is divided into this many steps: the smallest note is a
/// sixteenth at four quarters per bar.
pub const STEPS_PER_BEAT: u32 = 4;
pub const BEATS_PER_BAR: u32 = 4;
pub const STEPS_PER_BAR: u32 = STEPS_PER_BEAT * BEATS_PER_BAR;

/// The range of the roll, about that of a piano.
pub const LOWEST_PITCH: i32 = 24;
pub const HIGHEST_PITCH: i32 = 107;

const PITCH_NAMES: [&str; 12] = [
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
];
const SHARP: [bool; 12] = [
    false, true, false, true, false, false, true, false, true, false, true, false,
];

/// The name of a pitch, e.g. "C4" or "F#3".
pub fn pitch_name(pitch: i32) -> String {
    let index = pitch.rem_euclid(12) as usize;
    let octave = pitch.div_euclid(12) - 1;
    format!("{}{octave}", PITCH_NAMES[index])
}

/// Is this one of the black keys?
pub fn is_sharp(pitch: i32) -> bool {
    SHARP[pitch.rem_euclid(12) as usize]
}

/// One note of a pattern.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Note {
    /// Names the note as long as it exists; ids are never reused in a pattern.
    pub id: u32,
    pub step: u32,
    /// At least one step.
    pub length: u32,
    pub pitch: i32,
    /// How hard it is played, 0 to 100.
    pub velocity: u8,
}

impl Note {
    pub fn end(&self) -> u32 {
        self.step + self.length
    }
}

/// The notes of one channel.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Pattern {
    notes: Vec<Note>,
}

impl Pattern {
    pub fn notes(&self) -> &[Note] {
        &self.notes
    }

    pub fn is_empty(&self) -> bool {
        self.notes.is_empty()
    }

    /// Where the last note ends, 0 for an empty pattern.
    pub fn length(&self) -> u32 {
        self.notes.iter().map(Note::end).max().unwrap_or(0)
    }

    /// How many bars the notes fill, at least one: a pattern written over
    /// four bars takes four bars of the song.
    pub fn bars(&self) -> u32 {
        self.length().div_ceil(STEPS_PER_BAR).max(1)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pitch_names() {
        assert_eq!(pitch_name(60), "C4");
        assert_eq!(pitch_name(69), "A4");
        assert_eq!(pitch_name(66), "F#4");
        assert_eq!(pitch_name(24), "C1");
        assert_eq!(pitch_name(107), "B7");
        assert!(is_sharp(61));
        assert!(!is_sharp(60));
    }

    #[test]
    fn an_empty_pattern_takes_one_bar() {
        let pattern = Pattern::default();
        assert!(pattern.is_empty());
        assert_eq!((pattern.length(), pattern.bars()), (0, 1));
    }
}
