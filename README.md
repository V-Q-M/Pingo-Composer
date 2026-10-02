# Pingo Composer

A tiny music tracker with a pixel UI, written in C++20 with [raylib](https://www.raylib.com).
You write notes into a piano roll, arrange the patterns of every channel into a song and
hear it played by a small sound chip. It is the music companion of
[Pingo Engine](https://github.com/V-Q-M/Pingo-Engine) and shares its pixel font and look.

## Features

- **Piano roll.** Draw notes with the mouse, move them, change their length at both edges or with
  the keyboard, select several with a window and copy them. A new note is as long as the last one.
  The keys on the left play their sound when you click or slide over them.
- **Arrangement.** Every channel has patterns, and a pattern is placed into bars like a block.
  Double click a block to open its pattern in the roll, scroll the wheel over it to choose another one.
- **A small sound chip.** Seven waves (square, pulse, thin, triangle, saw, noise and metal) with
  volume, attack, decay, sustain, release, vibrato and sweep per channel. Eight voices play at once.
- **Save and open** songs in their own format (`.pingo`), **import** the notes of a midi file.
- **Export** the whole song, or only the open pattern, as sound (`.wav`) or as notes (`.mid`).
- Two songs to start from in `template/`: the tetris tune and a battle song.

## Getting started

You need a C++20 compiler, CMake 3.20 or newer, raylib and nlohmann/json.

### Terminal (macOS)

```bash
brew install cmake raylib nlohmann-json
git clone https://github.com/V-Q-M/Pingo-Composer.git
cd Pingo-Composer
cmake -S . -B build
cmake --build build
cd build && ./PingoComposer
```

Start the program from inside the build folder: it loads `assets/` relative to the working
directory, and every build copies the assets there.

### CLion

Install the libraries as shown above, open the project folder, select the **PingoComposer** run
configuration and press **Run**.

The composer is developed and tested on macOS. The open and save panels are the ones of the
system there; elsewhere the program saves into its working directory under the suggested name.

## Project structure

```
assets/
  fonts/         the pixel font atlas
  fontSpacing.json   how wide every character is
template/        songs to start from (.pingo), a midi and a wav export
src/
  Composer/      notes, channels, sound chip, files, and the screens
  Engine/        the small pixel UI the screens stand on: window, canvas, widgets, font
  main.cpp
```

## Keys

Ctrl means `Ctrl` or `Cmd`.

| Key | Action |
| --- | --- |
| `Space` | Play or pause |
| `Enter` / `Shift` + `Backspace` | Rewind to the start |
| `Ctrl` + `S` / `E` / `O` | Save / export / open |
| `Ctrl` + `+` / `-` | Make the pixels larger or smaller |

The floppy button always asks where to save, `Ctrl` + `S` writes to the file the song already has.

### Arrangement

| Mouse / key | Action |
| --- | --- |
| Click or drag on an empty bar | Place a block with the pattern of that channel |
| Right click or drag | Remove blocks |
| Wheel over a block | Choose another pattern (about six notches per step) |
| `1` to `9` over a block | Choose that pattern |
| Double click a block | Open its pattern in the roll |
| Wheel over empty bars, sideways wheel | Scroll through the bars |
| `Ctrl` + click | Pick a block (again to unpick) |
| `Del` / `Backspace` | Remove the picked blocks |
| `Ctrl` + `C` / `V` | Copy the pattern under the mouse, paste it into the bar under the mouse |
| Click or drag in the ruler | Move the playhead |

### Piano roll

| Mouse / key | Action |
| --- | --- |
| Click on an empty cell | Write a note, drag to the right to make it longer |
| Drag a note | Move it; at its left or right edge change its length |
| Right click or drag | Erase the notes it passes over |
| `Shift` + drag | Select the notes inside a window |
| `Ctrl` + click on a note | Add it to the selection or take it out |
| Arrows | Move the selected notes by a step or a semitone |
| `Shift` + left / right | Make the selected notes shorter or longer |
| `Shift` + up / down | Move the selected notes by an octave |
| `Del` / `Backspace` | Delete the selected notes |
| `Ctrl` + `C` / `V` | Copy the selected notes, paste them at the mouse (or at the playhead) |
| Wheel | Scroll the pitch |
| `Shift` + wheel, sideways wheel | Scroll the time |
| `Ctrl` + wheel | Zoom the time |
| Click or drag the keys | Hear a key |
| Click or drag in the ruler | Move the playhead |
| Double click on the channel | Close the roll |

Click a channel in the list to work on it, the speaker next to it mutes it, and the steppers
under the list set its instrument.

## Screenshots

`./PingoComposer --shot name.png` saves a picture of the program a moment after it started. The
name is relative to the working directory, and the program keeps running afterwards.
