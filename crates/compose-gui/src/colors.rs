//! Colors only the composer's screens have (piano keys, arrangement rows).
//! Everything shared with other apps comes from the `GuiTheme`.

use compose_core::Channel;
use pingo_gui::Rgba;

/// Rows of the arrangement, alternating.
pub const ROW: Rgba = Rgba::new(0, 0, 0, 60);
pub const ROW_ALT: Rgba = Rgba::new(255, 255, 255, 8);

pub const BLOCK_BORDER: Rgba = Rgba::rgb(18, 16, 28);
pub const NOTE_BORDER: Rgba = Rgba::rgb(18, 16, 28);

/// The keys of the roll stay dark like the rest; only brightness tells white
/// from black keys.
pub const WHITE_KEY: Rgba = Rgba::rgb(72, 70, 92);
pub const BLACK_KEY: Rgba = Rgba::rgb(28, 26, 42);
pub const KEY_BORDER: Rgba = Rgba::rgb(18, 16, 28);
pub const WHITE_KEY_HOVER: Rgba = Rgba::rgb(116, 114, 140);
pub const BLACK_KEY_HOVER: Rgba = Rgba::rgb(60, 58, 80);

/// Rows of the black keys are a little darker; the row under the hand a
/// little brighter.
pub const ROW_SHARP: Rgba = Rgba::new(0, 0, 0, 40);
pub const ROW_LIT: Rgba = Rgba::new(255, 255, 255, 28);

/// Lines of the roll: beats, and brighter ones for bars.
pub const BEAT_LINE: Rgba = Rgba::new(255, 255, 255, 20);
pub const BAR_LINE: Rgba = Rgba::new(255, 255, 255, 60);

/// How dark a muted channel's blocks are.
const MUTED_SHADE: f32 = 0.4;

/// A channel's own color, darker while it is muted.
pub fn channel(channel: &Channel) -> Rgba {
    let c = channel.colour;
    let colour = Rgba::rgb(c.0, c.1, c.2);
    if channel.muted {
        colour.shade(MUTED_SHADE)
    } else {
        colour
    }
}
