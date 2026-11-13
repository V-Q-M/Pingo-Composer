//! The song: its channels and the tempo.

use crate::pattern::Pattern;

pub const TEMPO_MIN: u32 = 40;
pub const TEMPO_MAX: u32 = 300;
pub const TEMPO_STEP: u32 = 5;

/// Bars an arrangement has room for.
pub const BARS: usize = 64;

/// A color of the song itself (each channel draws its notes in one), not of
/// a theme.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Rgb(pub u8, pub u8, pub u8);

/// The waveform of an instrument.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Wave {
    /// Half, a quarter and an eighth of the period.
    #[default]
    Square,
    Pulse,
    Thin,
    Triangle,
    Saw,
    Noise,
    /// Short noise loops that sound metallic.
    Metal,
}

impl Wave {
    pub const ALL: [Wave; 7] = [
        Wave::Square,
        Wave::Pulse,
        Wave::Thin,
        Wave::Triangle,
        Wave::Saw,
        Wave::Noise,
        Wave::Metal,
    ];

    /// The name in song files.
    pub fn name(self) -> &'static str {
        match self {
            Wave::Square => "square",
            Wave::Pulse => "pulse",
            Wave::Thin => "thin",
            Wave::Triangle => "triangle",
            Wave::Saw => "saw",
            Wave::Noise => "noise",
            Wave::Metal => "metal",
        }
    }

    /// The next (`turn` 1) or previous (-1) wave, around the end.
    pub fn turned(self, turn: i32) -> Wave {
        let at = Wave::ALL.iter().position(|w| *w == self).unwrap_or(0) as i32;
        let n = Wave::ALL.len() as i32;
        Wave::ALL[(at + turn).rem_euclid(n) as usize]
    }
}

/// How a channel sounds.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Instrument {
    pub wave: Wave,
    /// Next to the other channels, 0 to 1.
    pub volume: f32,
    /// Seconds.
    pub attack: f32,
    pub decay: f32,
    /// What is left after the decay, 0 to 1.
    pub sustain: f32,
    pub release: f32,
    /// How far the pitch swings while held, in half steps.
    pub vibrato: f32,
    /// Half steps per second the pitch walks, e.g. down for a drum.
    pub sweep: f32,
}

impl Default for Instrument {
    fn default() -> Instrument {
        Instrument {
            wave: Wave::Square,
            volume: 0.6,
            attack: 0.004,
            decay: 0.0,
            sustain: 1.0,
            release: 0.05,
            vibrato: 0.0,
            sweep: 0.0,
        }
    }
}

/// One setting of an instrument, as the instrument panel lists them.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Setting {
    Wave,
    Volume,
    Attack,
    Decay,
    Sustain,
    Release,
    Vibrato,
    Sweep,
}

impl Setting {
    pub const ALL: [Setting; 8] = [
        Setting::Wave,
        Setting::Volume,
        Setting::Attack,
        Setting::Decay,
        Setting::Sustain,
        Setting::Release,
        Setting::Vibrato,
        Setting::Sweep,
    ];

    /// Short name; the wave has none, its value says it.
    pub fn label(self) -> &'static str {
        match self {
            Setting::Wave => "",
            Setting::Volume => "Vol",
            Setting::Attack => "Att",
            Setting::Decay => "Dec",
            Setting::Sustain => "Sus",
            Setting::Release => "Rel",
            Setting::Vibrato => "Vib",
            Setting::Sweep => "Swp",
        }
    }
}

/// A time grows by a quarter of itself, at least 5 ms: short ones can be set
/// exactly, long ones are reached in a few steps.
fn nudge(seconds: f32, turn: i32) -> f32 {
    let step = (seconds * 1000.0 * 0.25).round().max(5.0) / 1000.0;
    (seconds + turn as f32 * step).clamp(0.0, 4.0)
}

impl Instrument {
    /// The value of a setting as shown: a name, percent or milliseconds.
    pub fn value(&self, setting: Setting) -> String {
        let percent = |v: f32| format!("{}", (v * 100.0).round() as i32);
        let millis = |v: f32| format!("{}", (v * 1000.0).round() as i32);
        match setting {
            Setting::Wave => {
                let name = self.wave.name();
                let mut chars = name.chars();
                chars
                    .next()
                    .map(|c| c.to_ascii_uppercase().to_string() + chars.as_str())
                    .unwrap_or_default()
            }
            Setting::Volume => percent(self.volume),
            Setting::Attack => millis(self.attack),
            Setting::Decay => millis(self.decay),
            Setting::Sustain => percent(self.sustain),
            Setting::Release => millis(self.release),
            Setting::Vibrato => percent(self.vibrato),
            Setting::Sweep => format!("{}", self.sweep as i32),
        }
    }

    /// One step up (`turn` 1) or down (-1), kept in range.
    pub fn step(&mut self, setting: Setting, turn: i32) {
        let t = turn as f32;
        match setting {
            Setting::Wave => self.wave = self.wave.turned(turn),
            Setting::Volume => self.volume = (self.volume + t * 0.05).clamp(0.0, 1.0),
            Setting::Attack => self.attack = nudge(self.attack, turn),
            Setting::Decay => self.decay = nudge(self.decay, turn),
            Setting::Sustain => self.sustain = (self.sustain + t * 0.1).clamp(0.0, 1.0),
            Setting::Release => self.release = nudge(self.release, turn),
            Setting::Vibrato => self.vibrato = (self.vibrato + t * 0.05).clamp(0.0, 2.0),
            Setting::Sweep => self.sweep = (self.sweep + t * 4.0).clamp(-240.0, 240.0),
        }
    }
}

/// One voice of the song. It owns its patterns and says for every bar which
/// of them plays there.
#[derive(Clone, Debug, PartialEq)]
pub struct Channel {
    pub name: String,
    /// Its notes and blocks are drawn in this color.
    pub colour: Rgb,
    pub instrument: Instrument,
    /// A muted channel stays in the song but is not heard.
    pub muted: bool,
    /// At least one; they are named by their number, from 1.
    pub patterns: Vec<Pattern>,
    /// The pattern in each bar, `None` for an empty bar.
    pub bars: Vec<Option<usize>>,
}

impl Channel {
    pub fn new(name: &str, colour: Rgb, wave: Wave) -> Channel {
        Channel {
            name: name.to_string(),
            colour,
            instrument: Instrument {
                wave,
                ..Instrument::default()
            },
            muted: false,
            patterns: vec![Pattern::default()],
            bars: vec![None; BARS],
        }
    }

    /// The pattern with this index, made (empty) if it doesn't exist yet.
    pub fn pattern(&mut self, index: usize) -> &mut Pattern {
        if self.patterns.len() <= index {
            self.patterns.resize_with(index + 1, Pattern::default);
        }
        &mut self.patterns[index]
    }

    /// The blocks of the arrangement: the bar each starts in, its pattern and
    /// how many bars it covers.
    pub fn blocks(&self) -> impl Iterator<Item = (usize, usize, usize)> + '_ {
        self.bars.iter().enumerate().filter_map(|(bar, p)| {
            let p = (*p)?;
            let bars = self
                .patterns
                .get(p)
                .map_or(1, |pattern| pattern.bars() as usize);
            Some((bar, p, bars))
        })
    }
}

/// The whole song.
#[derive(Clone, Debug, PartialEq)]
pub struct Song {
    pub channels: Vec<Channel>,
    /// Beats per minute, `TEMPO_MIN..=TEMPO_MAX`.
    pub tempo: u32,
}

impl Default for Song {
    /// The voices of a classic sound chip, each in its own color.
    fn default() -> Song {
        Song {
            channels: vec![
                Channel::new("Pulse 1", Rgb(0, 249, 255), Wave::Square),
                Channel::new("Pulse 2", Rgb(61, 255, 20), Wave::Pulse),
                Channel::new("Triangle", Rgb(255, 229, 26), Wave::Triangle),
                Channel::new("Noise", Rgb(188, 190, 202), Wave::Noise),
                Channel::new("Sample", Rgb(255, 108, 34), Wave::Square),
            ],
            tempo: 120,
        }
    }
}

impl Song {
    /// One tempo step faster (`turn` 1) or slower (-1), kept in range.
    pub fn step_tempo(&mut self, turn: i32) {
        let tempo = self.tempo as i64 + i64::from(turn) * i64::from(TEMPO_STEP);
        self.tempo = tempo.clamp(i64::from(TEMPO_MIN), i64::from(TEMPO_MAX)) as u32;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_song() {
        let song = Song::default();
        assert_eq!(song.channels.len(), 5);
        assert_eq!(song.tempo, 120);
        assert_eq!(song.channels[1].instrument.wave, Wave::Pulse);
        assert_eq!(song.channels[0].blocks().count(), 0);
    }

    #[test]
    fn tempo_stays_in_range() {
        let mut song = Song::default();
        song.step_tempo(1);
        assert_eq!(song.tempo, 125);
        for _ in 0..100 {
            song.step_tempo(-1);
        }
        assert_eq!(song.tempo, TEMPO_MIN);
        for _ in 0..100 {
            song.step_tempo(1);
        }
        assert_eq!(song.tempo, TEMPO_MAX);
    }

    #[test]
    fn waves_turn_around() {
        assert_eq!(Wave::Square.turned(-1), Wave::Metal);
        assert_eq!(Wave::Metal.turned(1), Wave::Square);
        assert_eq!(Wave::Pulse.turned(2), Wave::Triangle);
    }

    #[test]
    fn instrument_settings() {
        let mut i = Instrument::default();
        assert_eq!(i.value(Setting::Wave), "Square");
        assert_eq!(i.value(Setting::Volume), "60");
        assert_eq!(i.value(Setting::Attack), "4");
        i.step(Setting::Attack, 1);
        assert_eq!(i.value(Setting::Attack), "9");
        i.step(Setting::Volume, 1);
        assert_eq!(i.value(Setting::Volume), "65");
        for _ in 0..70 {
            i.step(Setting::Sustain, 1);
            i.step(Setting::Sweep, -1);
        }
        assert_eq!((i.sustain, i.sweep), (1.0, -240.0));
        i.step(Setting::Wave, 1);
        assert_eq!(i.value(Setting::Wave), "Pulse");
        assert!(Setting::ALL.iter().all(|s| !i.value(*s).is_empty()));
    }

    #[test]
    fn blocks_of_the_arrangement() {
        let mut channel = Channel::new("A", Rgb(1, 2, 3), Wave::Saw);
        channel.bars[2] = Some(1);
        channel.pattern(1);
        assert_eq!(channel.patterns.len(), 2);
        assert_eq!(channel.blocks().collect::<Vec<_>>(), vec![(2, 1, 1)]);
    }
}
