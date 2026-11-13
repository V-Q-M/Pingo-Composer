//! Pingo Composer: a tiny music tracker with a pixel UI.
//!
//! `pingo-compose --shot name.png` saves a picture of the window a moment
//! after it started, for documentation; the program keeps running.

mod arranger;
mod colors;
mod keys;
mod piano_roll;
mod prefs;
mod studio;

use std::process::ExitCode;

use pingo_gui::{GuiTheme, Window, WindowOptions};

use crate::studio::Studio;

fn main() -> ExitCode {
    let mut shot = None;
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--shot" => shot = args.next(),
            "-h" | "--help" => {
                println!("pingo-compose [--shot FILE.png]");
                return ExitCode::SUCCESS;
            }
            other => {
                eprintln!("pingo-compose: unknown argument '{other}'");
                return ExitCode::FAILURE;
            }
        }
    }

    let options = WindowOptions {
        title: "Pingo Composer".into(),
        ..WindowOptions::default()
    };
    let mut window = match Window::open(options) {
        Ok(window) => window,
        Err(e) => {
            eprintln!("pingo-compose: {e}");
            return ExitCode::FAILURE;
        }
    };
    window.set_theme(GuiTheme::pingo());
    if let Some(path) = shot {
        window.take_shot(path);
    }
    match window.run(&mut Studio::default()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("pingo-compose: {e}");
            ExitCode::FAILURE
        }
    }
}
