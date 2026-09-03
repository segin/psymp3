# PsyMP3 2-CURRENT

A simplistic audio media player with a flashy Fourier transform.

![PsyMP3 playing "Forty Six & 2" by TOOL, showing the spectrum analyzer, synced lyrics, and now-playing info](docs/psymp3-screenshot.png)

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Building](#building)
4. [Usage](#usage)
5. [Integrations](#integrations)
6. [Testing](#testing)
7. [Notes](#notes)

## Overview

PsyMP3 2.x is a radical departure from the code of the 1.x series. Whereas 1.x was written in FreeBASIC, 2.x is written in C++17, and is portable!

Highlights:

- Real-time FFT spectrum visualizer with adjustable intensity, decay, and draw modes
- A faithful Windows 3.1-style in-app UI: menu bar, movable windows (Playlist Manager, Equalizer, Media Information, About), buttons, scrollbars, and dialogs — all software-rendered
- Wide format support through a modular demuxer/codec architecture, with several codecs vendored so they work with no external libraries
- Synced lyrics display (`.lrc` files)
- Last.fm scrobbling (Web Services API 2.0), MPRIS desktop control, and Discord Rich Presence
- Session persistence: with Persist Playlist enabled, PsyMP3 reopens your playlist at the track you were playing

> **Note**: The "2-CURRENT" version tag represents the active development branch. End users should be aware that this is pre-release software.

**Contact**: <segin2005@gmail.com>

## System Requirements

### Windows
- Windows 10 or later
- Official builds are cross-compiled with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
  (x86_64, i686, and ARM64), statically linked against SDL3; building under
  MSYS2 is also possible

### Linux/BSD
**Core dependencies** (always required):
- SDL3 3.0 or later (`sdl3`)
- FreeType2 (`freetype2`)
- taglib 1.6 or later (`taglib`; taglib 2.x works too)
- OpenSSL 1.0 or later (`openssl`)
- libcurl 7.20.0 or later (`libcurl`)

**Optional codec dependencies** (auto-detected; each can be disabled at build time):
- libogg (`ogg`) — required for Vorbis, Opus, and Ogg FLAC (container parsing)
- libopus (`opus`) — Opus
- FDK-AAC (`fdk-aac`) — the whole AAC family: AAC-LC, HE-AACv1, HE-AACv2 and
  xHE-AAC / MPEG-D USAC
- speex 1.2 or later (`speex`) — Speex
- spandsp (`spandsp`) — G.722

**Bundled codecs** (vendored, no external dependency):
- FLAC — native decoder (no libFLAC needed)
- Vorbis — stb_vorbis (`third_party/stb`; needs only libogg for the container)
- MP3 — minimp3 (`third_party/minimp3`)
- MP2 — kjmp2 (`third_party/kjmp2`)
- ALAC — Apple's reference decoder (`third_party/alac`)
- MLP / Dolby TrueHD — bundled decoder (`third_party/mlp`), derived from
  [truehdd](https://github.com/truehdd/truehdd) © 2025 Rainbaby (Apache-2.0)
- G.711 µ-law/A-law, and raw PCM formats

**Optional integration dependencies**:
- D-Bus 1.0 or later (`dbus-1`) — MPRIS desktop media control
- A GUI toolkit for the native file-open/save dialog. Configure probes, in
  order, and uses the first one found:
  **Qt 6** (`Qt6Widgets`) → **Qt 5** (`Qt5Widgets`) → **Qt 4** (`QtGui`) →
  **Qt 3** (no pkg-config; opt in with `--with-qt3-dir=PREFIX`) →
  **GTK 4** (`gtk4`) → **GTK+ 3** (`gtk+-3.0`) → **GTK+ 2** (`gtk+-2.0`).
  Without any of these, the file dialogs are unavailable but PsyMP3 still
  builds and plays files given on the command line.

### Build Requirements
- C++17 compliant compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Autotools: `autoconf`, `automake`, `libtool`, and **`autoconf-archive`**
  (required — it supplies `AX_CXX_COMPILE_STDCXX_17`; without it `autoreconf`
  silently drops the C++17 check and the build fails with `-std=c++17` errors)
- `pkg-config`
- Optional, for `make check`: [RapidCheck](https://github.com/emil-e/rapidcheck)
  (property-based tests, enabled with `--enable-rapidcheck`)

## Building

**From a release tarball:**
```bash
./configure
make -j$(nproc)
```

**From git** (requires autoconf-archive):
```bash
./generate-configure.sh
./configure
make -j$(nproc)
```

**Build Options:**
- `--enable-flac` / `--enable-vorbis` / `--enable-opus` / `--enable-aac` /
  `--enable-speex` / `--enable-g722` / `--enable-g711` — per-codec toggles (default: yes)
- `--enable-mpris` — MPRIS desktop integration over D-Bus (default: auto)
- `--enable-final` — unity build: all sources in one translation unit (much
  faster full rebuilds; used for release builds)
- `--enable-static-binary` — fully static, self-contained executable (used for
  the Windows release builds)
- `--enable-test-harness` — build the test harness (default: yes)
- `--enable-asan` / `--enable-ubsan` / `--enable-tsan` — sanitizer builds (debug only)

### Distribution packages

`package/` holds native packaging, built for every push by the
[Linux packages](.github/workflows/packages.yml) workflow:

| Format | Targets |
|---|---|
| `.deb` | Debian 13 (trixie), Ubuntu 26.04 LTS |
| `.rpm` | Fedora, openSUSE Tumbleweed |

```bash
./package/dpkg/build-deb.sh      # -> package/dpkg/out/*.deb
./package/rpm/build-rpm.sh       # -> package/rpm/out/*.rpm
```

Both build from a clean export of `HEAD`, so commit before packaging. See
[package/README.md](package/README.md) for the dependency-installation
one-liners and how the version label is mapped to a legal package version.

## Usage

Pass the paths of audio files or playlists (`.m3u`/`.m3u8`) as program
arguments; they are played in order. Supported formats include MP3, MP2,
Ogg Vorbis, Opus, FLAC (native and Ogg), WAV/RIFF, AAC/M4A, xHE-AAC, ALAC, Speex,
MLP/Dolby TrueHD (`.thd`/`.truehd`/`.mlp`), G.711 (µ-law/A-law), G.722, and raw PCM.

PsyMP3 has a full mouse-driven UI — a menu bar (`File`, `Playback`,
`Settings`, `Help` — Alt+F/P/S mnemonics work) plus movable in-app windows
like the Playlist Manager, Equalizer, and Media Information — and everything
is also reachable from the keyboard.

### Keyboard Controls

| Key | Action |
|-----|--------|
| `ESC`, `Q` | Quit PsyMP3 |
| `Space` | Pause (or resume) playback |
| `R` | Restart the current track from the beginning |
| `N` / `P` | Next / previous track |
| `Up` / `Down` | Volume up / down |
| `E` | Cycle loop mode (`Shift+E` opens the Equalizer) |
| `M` | Playlist Manager |
| `F` | Cycle FFT draw mode |
| `G` | Toggle 2× zoom |
| `0`–`4` | Spectrum intensity |
| `Z` / `X` / `C` | Spectrum decay (fast/normal/slow) |
| `Ctrl+O` | Open tracks (replaces playlist) |
| `I` / `L` | Queue tracks next / play a track now |
| `Ctrl+S` | Save playlist |
| `Ctrl+F4` | Close the focused in-app window |

### Command-line Options

- `--version` - Print version and licensing information
- `--debug <channels>` - Enable debug logging (comma-separated channels, or `all`)
- `--logfile <file>` - Write debug logs to the specified file

## Integrations

### Last.fm Scrobbling

PsyMP3 scrobbles through the Last.fm Web Services API 2.0 with its own
registered API key. The easiest way to set it up is in the app:
**Settings → Last.fm Credentials...** — enter your username and password,
press **Test** to verify, and **OK** to save. The password is only needed
once: after the first successful authentication PsyMP3 stores a permanent
session key and wipes the password.

Configuration lives in:
- **Linux/Unix**: `~/.config/psymp3/lastfm.conf`
- **Windows**: `%APPDATA%\PsyMP3\lastfm.conf`

and can also be created by hand:
```ini
# Last.fm configuration
username=your_lastfm_username
password=your_lastfm_password
```

Scrobbles and now-playing updates include MusicBrainz recording IDs when
your files are tagged with them (e.g. by MusicBrainz Picard), and failed
submissions are cached and retried across sessions.

### MPRIS Desktop Integration

PsyMP3 implements MPRIS (Media Player Remote Interfacing Specification) for
desktop media-control integration — play/pause/seek from your desktop
environment, media keys, and now-playing applets (Linux/BSD only).

### Discord Rich Presence

With Discord running on the same machine, PsyMP3 shows what you're listening
to as a Discord activity: artist in the header, track and album on the card,
a live progress bar, and album art from the Cover Art Archive (via your
files' MusicBrainz tags, or a live MusicBrainz lookup for untagged files).
Toggle it under **Settings → Discord Presence**. No Discord SDK or account
linking is required.

## Testing

To run the full test suite:

```bash
make check
```

For detailed testing information, see [TESTING.md](TESTING.md).

## Notes

**Unicode Support**: Unicode ID3 tags are supported. PsyMP3 renders UI text through the built-in FreeType path; replace the bundled `vera.ttf` with a font file containing the glyph coverage you want. On Windows, dropping a `vera.ttf` next to `psymp3.exe` (or in the working directory) overrides the font embedded in the exe — no rebuild needed.
