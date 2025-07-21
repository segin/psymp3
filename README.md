# PsyMP3 2-CURRENT

A simplistic audio media player with a flashy Fourier transform.

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Building and Testing](#building-and-testing)
4. [Usage](#usage)
5. [Notes](#notes)

## Overview

PsyMP3 2.x is a radical departure from the code of the 1.x series. Whereas 1.x was written in FreeBASIC, 2.x is written in C++, and is portable!

> **Note**: The "2-CURRENT" version tag represents the "bleeding edge" of development. End users shouldn't be using this; please use the last version tagged as -RELEASE. -CURRENT means "This is what I am working on, in whatever state it happens to be in, complete or not."

**Contact**: <segin2005@gmail.com>

## System Requirements

### Windows
- Windows 10 or later
- Windows 7 through 8.1 might work (YMMV)

### Linux/BSD
**Core dependencies** (always required):
- SDL 1.2 or later
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
- D-Bus 1.0 or later (optional, for MPRIS support)

### macOS
Not officially supported, but building the dependencies for Linux/BSD with MacPorts might work.

### Build Requirements
Building from git requires `autoconf-archive` for the extended macros.

> **Note**: The build system now supports optional codec dependencies. If a codec library is not found, that codec will be disabled but the build will continue. Use configure options like `--disable-flac`, `--disable-mp3`, `--disable-vorbis`, or `--disable-opus` to explicitly disable codec support.

## Building and Testing

### Building PsyMP3

**From a release tarball:**
```bash
./configure
make
```

**From git** (requires autoconf-archive):
```bash
./generate-configure.sh
./configure
make
```

**Build Options:**
- `--enable-flac` - Enable FLAC support (default: yes)
- `--enable-mp3` - Enable MP3 support (default: yes)  
- `--enable-vorbis` - Enable Vorbis support (default: yes)
- `--enable-opus` - Enable Opus support (default: yes)
- `--enable-test-harness` - Build test harness (default: yes)
- `--enable-release` - Enable release optimizations (default: no)

### Testing PsyMP3

**Run all tests with the unified test harness:**
```bash
make check
```

This will build all test executables and run them through the test harness, providing a comprehensive report of test results.

#### Test Harness Options

**Basic Usage:**
```bash
cd tests && ./test-harness                    # Run all tests
cd tests && ./test-harness -v                # Run with verbose output
cd tests && ./test-harness -l                # List available tests
cd tests && ./test-harness -q                # Quiet mode (summary only)
```

**Filtering and Selection:**
```bash
cd tests && ./test-harness -f "*rect*"       # Run only rectangle tests
cd tests && ./test-harness -f "*containment*" # Run containment tests
cd tests && ./test-harness -s                # Stop on first failure
```

**Parallel Execution:**
```bash
cd tests && ./test-harness -p                # Run tests in parallel
cd tests && ./test-harness -p -j 8           # Use 8 parallel processes
```

**Output Formats:**
```bash
cd tests && ./test-harness -o xml > results.xml    # XML output for CI
cd tests && ./test-harness -o json > results.json  # JSON output
cd tests && ./test-harness -o console              # Console output (default)
```

**Performance Analysis:**
```bash
cd tests && ./test-harness --track-performance     # Enable performance tracking
cd tests && ./test-harness --show-performance      # Show performance report
cd tests && ./test-harness --show-detailed-perf    # Detailed performance metrics
cd tests && ./test-harness --analyze-trends        # Analyze performance trends
```

**Advanced Options:**
```bash
cd tests && ./test-harness -t 60               # Set 60-second timeout per test
cd tests && ./test-harness -d /path/to/tests   # Specify test directory
```

#### Running Individual Tests

If you prefer to run tests individually (legacy method):
```bash
cd tests
make test_rect_containment
./test_rect_containment
```

**Available individual test executables:**
- `test_rect_area_validation`
- `test_rect_containment`
- `test_rect_intersection`
- `test_rect_union`
- `test_rect_centering`
- `test_rect_centering_overflow`
- `test_rect_expansion`
- `test_rect_transformation`
- `test_rect_normalization`
- `test_rect_modern_cpp`

#### Migration from Old Testing Methods

**Old way:**
```bash
cd tests
gcc -o test_rect_containment test_rect_containment.cpp -I../include
./test_rect_containment
```

**New way:**
```bash
make check
```

The new test harness provides:
- Automatic test discovery and execution
- Standardized reporting across all tests
- Performance tracking and analysis
- Multiple output formats for CI integration
- Parallel execution for faster testing
- Better error handling and timeout management

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

### Command-line Options

- `--debug` - Enable debug logging to the console
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

**Features:**
- Submit "Now Playing" status when you start a track
- Scrobble tracks after you've listened to >50% or >4 minutes (whichever comes first)
- Only scrobble tracks longer than 30 seconds
- Cache scrobbles offline and submit when connection is restored

For detailed Last.fm setup and troubleshooting, see the included `LASTFM_SETUP.md` file.

## Notes

At this time, PsyMP3 2.x is incomplete. There's very little UI code, and most features are missing at this time. If you are a developer and are interested in helping, please email me above.

If you are an end user and you don't like this program, feel free to use something else, and check back later for improvements.

**Unicode Support**: Unicode ID3 tags are supported. Please replace the included `vera.ttf` with a different font file containing the Unicode glyphs you desire. Advanced text layout is not supported (e.g. connected Arabic characters).

---

*This README was last updated on July 20, 2025.*