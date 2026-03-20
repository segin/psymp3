# PsyMP3 2-CURRENT

A simplistic audio media player with a flashy Fourier transform.

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Building](#building)
4. [Usage](#usage)
5. [Testing](#testing)
6. [Notes](#notes)

## Overview

PsyMP3 2.x is a radical departure from the code of the 1.x series. Whereas 1.x was written in FreeBASIC, 2.x is written in C++17, and is portable!

> **Note**: The "2-CURRENT" version tag represents the active development branch. End users should be aware that this is pre-release software.

**Contact**: <segin2005@gmail.com>

## System Requirements

### Windows
- Windows 10 or later
- MSYS2 environment recommended for building

### Linux/BSD
**Core dependencies** (always required):
- SDL 1.2 or later (SDL 2.0 planned)
- taglib 1.6 or later
- FreeType 2.4 or later
- OpenSSL 1.0 or later
- libcurl 7.20.0 or later

**Optional codec dependencies** (can be disabled at build time):
- libmpg123 1.8 or later (for MP3 support)
- libvorbis (for Ogg Vorbis support)
- libopus (for Opus support)
- libogg (required for Vorbis, Opus, and Ogg FLAC)
- libFLAC++ 1.2 or later (for FLAC support)
- libFLAC (for FLAC support)

**Optional integration dependencies**:
- D-Bus 1.0 or later (for MPRIS desktop media control support)

### Switch / ARM
- Supported via DevkitPro (experimental)

### Build Requirements
- C++17 compliant compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Autotools (`autoconf`, `automake`, `libtool`, `autoconf-archive`)
- `pkg-config`

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
- `--enable-flac` - Enable FLAC support (default: yes)
- `--enable-mp3` - Enable MP3 support (default: yes)  
- `--enable-vorbis` - Enable Vorbis support (default: yes)
- `--enable-opus` - Enable Opus support (default: yes)
- `--enable-dbus` - Enable MPRIS integration (default: yes)
- `--enable-test-harness` - Build test harness (default: yes)
- `--enable-asan` - Enable AddressSanitizer (debug only)
- `--enable-tsan` - Enable ThreadSanitizer (debug only)

## Usage

Pass the paths to the audio files to be played as program arguments. Supported formats include MP3, Ogg Vorbis, Opus, FLAC, and RIFF WAVE. Audio files will be played in the order they are passed on the command line.

All user interaction, aside from clicking the 'close' button for the window, is done via the keyboard.

### Keyboard Controls

| Key | Action |
|-----|--------|
| `ESC`, `Q` | Quit PsyMP3 |
| `Space` | Pause (or resume) playback |
| `R` | Restart the current track from the beginning |
| `N` | Seek to the next track |
| `P` | Seek to the previous track |
| `H` | Toggle Test Window |

### Command-line Options

- `--debug` - Enable debug logging to the console (can specify comma-separated channels)
- `--logfile <file>` - Write debug logs to the specified file

### Last.fm Scrobbling

PsyMP3 includes built-in Last.fm scrobbling support to automatically track your music listening history.

**Configuration file locations:**
- **Linux/Unix**: `~/.config/psymp3/lastfm.conf`
- **Windows**: `%APPDATA%\PsyMP3\lastfm.conf`

**Configuration file format:**
```ini
# Last.fm configuration
username=your_lastfm_username
password=your_lastfm_password
```

### MPRIS Desktop Integration

PsyMP3 includes built-in MPRIS (Media Player Remote Interfacing Specification) support for seamless desktop media control integration (Linux/BSD only).

## Testing

To run the full test suite:

```bash
make check
```

For detailed testing information, see [TESTING.md](TESTING.md).

## Notes

**Unicode Support**: Unicode ID3 tags are supported. Please replace the included `vera.ttf` with a different font file containing the Unicode glyphs you desire.

---

*This README is auto-updated as part of the implementation plan.*