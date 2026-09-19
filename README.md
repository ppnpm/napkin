# Napkin

A persistent scratch surface for your computer.

Napkin is somewhere to put things before you know whether they matter. Paste
text and images into it; nothing needs a name, a folder or a tag. Everything
stays on this machine.

> **Status: early development (0.1.x), Linux only.** Expect rough edges.
> Windows and macOS are planned, not started.

## What it does

- **Napkins, not documents.** `Ctrl+N` gives you a fresh napkin. Paste onto
  it, type on it, and move on. Nothing asks for a title.
- **Text and images, and nothing else.** Text, where a link on its own shows as
  a chip you can open and a link inside prose opens with `Ctrl`+click; and
  images: PNG, JPEG, WebP, SVG and animated GIF.
- **Search everything** with `Ctrl+F`: text, links and image filenames.
- **Nothing is deleted on your behalf.** Napkins you have not touched in a while
  move to an *Older* section. *Clean up* moves them to the trash only when you
  ask, and it can be undone. *Pin* keeps a napkin at the top; *Keep* means Clean
  up never touches it.
- **Deleting is always recoverable.** A deleted napkin, or items deleted from
  one, wait in the trash for 30 days (adjustable in *File → Settings…*).
- **Export** one napkin or everything to a folder of ordinary files at any
  time (*File → Export this napkin…* / *Export everything…*).

## Privacy

- No account, no sync, no telemetry, and no network access of any kind. The
  only socket Napkin opens is a local one, so that launching it again raises
  the window already open.
- Your data lives in `~/.local/share/napkin/napkin/`: a SQLite database plus
  image files. The directory is readable only by you (`0700`, database `0600`),
  and it is marked so that desktop search (Baloo, Tracker) does not index it.
- **Keep is retention, not encryption. There is no at-rest encryption.** Anyone
  who can read your home directory can read what you pasted. A scratch surface
  will end up holding tokens and passwords, so treat it like one.

## Install

Releases publish an AppImage on the
[releases page](https://github.com/sudomonas/napkin/releases):

```sh
chmod +x napkin-v*-x86_64.AppImage
./napkin-v*-x86_64.AppImage
```

A Flatpak manifest is in `packaging/`; it has not been submitted to Flathub yet.

## Build from source

Requirements:

- A C++20 compiler. CI builds with GCC 11 (Ubuntu 22.04) and GCC 13 (Ubuntu 24.04).
- CMake 3.24 or newer, and Ninja (recommended)
- Qt 6.5 or newer: Core, Gui, Widgets, Network and Test, plus the
  `qtimageformats` and `qtsvg` plugins at runtime for WebP and SVG
- SQLite 3 with FTS5, 3.31 or newer. CI tests 3.31.1 and 3.37.2 as well as the
  runner's own.

On Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-imageformats qt6-svg sqlite
```

On Debian and Ubuntu, check that your release's Qt is 6.5 or newer; Ubuntu
24.04 ships 6.4, which is too old.

Then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build        # or --prefix ~/.local
```

The test suites need no display: the GUI tests run on Qt's offscreen platform.

## Keyboard

| Key | Action |
|---|---|
| `Ctrl+N` | New napkin |
| `Ctrl+V` | Paste onto this napkin |
| `Ctrl+T` | New note on this napkin |
| `Ctrl+Shift+I` | Add an image from a file |
| `Ctrl+F` / `Ctrl+K` | Search |
| `Ctrl+P` / `Ctrl+D` | Pin or keep the selected napkin |
| `Ctrl+Z` | Undo the last delete, pin or keep |
| `Delete` | Move the selected napkin to the trash (list focused) |

The full list is under *Help → Keyboard shortcuts…*. Typing on an empty napkin,
or double-clicking empty space on one, also starts a note, and right-clicking
an item offers Edit, Copy, Cut and Delete.

## Known limitations

- There is no global "capture" hotkey yet. It needs the XDG GlobalShortcuts
  portal and is planned.
- A single napkin holding around a thousand items is slow to open. Typical use
  (hundreds of napkins, each with up to about a hundred items) meets every
  performance target in [SPEC.md](SPEC.md) §12.
- Linux only for now.

## Design

[SPEC.md](SPEC.md) is the design document and the project's record of
decisions, measurements and corrected mistakes. Read it before proposing a
feature; §21 is the test every feature has to pass.

## License

[MIT](LICENSE). The toolbar icons are from [Lucide](https://lucide.dev), under
the ISC and MIT licences in
[`resources/icons/lucide/LICENSE`](resources/icons/lucide/LICENSE).
