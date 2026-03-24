# Architecture Overview

This document serves as a critical, living template designed to equip agents and developers with a rapid and comprehensive understanding of the PsyMP3 codebase's architecture, enabling efficient navigation and effective contribution. Update this document as the codebase evolves.

## 1. Project Structure

This section provides a high-level overview of the project's directory and file structure, categorized by architectural layer or major functional area.

```
[Project Root]/
├── src/            # Main source code for PsyMP3
│   ├── codecs/     # Audio codec implementations (AAC, FLAC, MP3, Opus, Vorbis, PCM)
│   ├── demuxer/    # Container demuxer implementations (Ogg, ISO, RIFF, FLAC, Raw)
│   ├── io/         # I/O abstraction layer (File, HTTP)
│   ├── widget/     # UI widget system
│   ├── lastfm/     # Last.fm scrobbling integration
│   └── mpris/      # MPRIS media player interface (D-Bus)
├── include/        # Header files defining the core interfaces (Demuxer, AudioCodec, etc.)
├── tests/          # Custom test harness and RapidCheck property-based tests
├── docs/           # Project documentation and task specifications
├── .github/        # GitHub CI/CD configurations
├── Makefile.am     # Automake configuration
├── configure.ac    # Autoconf configuration
├── README.md       # Project overview, requirements, and quick start guide
└── ARCHITECTURE.md # This document
```

## 2. High-Level System Diagram

PsyMP3 utilizes a VLC-style on-demand decoding pipeline. Decoded audio is NOT fully buffered in memory; instead, compressed chunks are buffered and decoded asynchronously on-demand as they are needed for playback.

```text
[Audio File / HTTP Stream]
           |
           v
     [IOHandler] (Read bytes)
           |
           v
      [Demuxer] (Parse container, extract chunks)
           |
           v
     [MediaChunk] (Compressed data buffer)
           |
           v
     [AudioCodec] (Decode chunks)
           |
           v
     [AudioFrame] (Raw PCM data)
           |
           +-----------------+
           |                 |
           v                 v
[Audio Output System]    [FFT Visualization]
```

## 3. Core Components

### 3.1. Codec Subsystem (`src/codecs/`)
- **Name**: Audio Codecs
- **Description**: Responsible for decoding compressed audio data into raw PCM frames. Implements wrappers for external libraries (`faad2`, `libmpg123`, `libvorbis`, `libopus`) and native decoders (RFC 9639 FLAC, PCM, G.711). AAC-in-MP4 uses `StreamInfo.codec_data` to carry the ISO `esds` AudioSpecificConfig from the demuxer into the decoder.
- **Technologies**: C++, `faad2`, `libmpg123`, `libvorbis`, `libopus`.

### 3.2. Demuxer Subsystem (`src/demuxer/`)
- **Name**: Container Demuxers
- **Description**: Parses media container formats to extract raw metadata (`StreamInfo`) and compressed data chunks (`MediaChunk`). Supports Ogg, ISO Base Media (MP4/M4A), RIFF (WAV), native FLAC, and raw streams. The ISO demuxer extracts AAC `DecoderSpecificInfo` from `esds` boxes and stores it in `StreamInfo.codec_data` for the AAC decoder, and it propagates `moov/udta/meta/ilst` metadata into per-stream `artist`/`title`/`album` fields so `DemuxedStream` can surface `m4a` tags without falling back to TagLib. Its sample-table path must preserve exact `stco`/`co64` chunk offsets and exact `stsz` per-sample sizes so codecs receive individual access units rather than chunk-aligned aggregates or approximated offsets. ISO sample-time lookups operate in the track `mdhd` timescale, not milliseconds, and chunk timestamps must be converted back into decoded PCM sample positions before they are handed to codecs like AAC. ISO seeking and chunk reads must operate on the same `AudioTrackInfo` instances; otherwise UI restart/seek requests can update stale state while playback continues from a different copy. In the Ogg path, demuxer-facing transport position is always milliseconds even though packet granules stay codec-specific, and codecs must still receive setup headers even after the demuxer consumed them during container parsing. Ogg therefore preserves header packets and concatenates them into `StreamInfo.codec_data` for codecs like Opus that need `OpusHead` during initialization. Opus frame-start timestamps must be derived from packet granules after subtracting `pre_skip` and any end trimming rather than by treating raw granules as audible PCM positions. Opus packet-loss concealment also has to request the prior decoded frame size from libopus; otherwise concealed output can jump to the decoder's maximum frame size and distort pacing.
- **Technologies**: C++, `libogg`.

### 3.3. I/O Layer (`src/io/`)
- **Name**: IOHandler
- **Description**: An abstraction layer for input sources, allowing the engine to transparently read from local files (`file/`) or remote HTTP streams (`http/`).
- **Technologies**: C++, `libcurl` (for HTTP).

### 3.4. Media Control & Scrobbling (`src/mpris/`, `src/lastfm/`)
- **Name**: Desktop Integration & Tracking
- **Description**: `mpris/` provides D-Bus integration for Linux desktop environments to control playback natively. `lastfm/` provides background scrobbling of played tracks to the Last.fm service, including offline caching.
- **Technologies**: C++, D-Bus, HTTP/libcurl, OpenSSL.

### 3.5. Widget Input Routing (`src/widget/`)
- **Name**: Hierarchical Widget Event Dispatch
- **Description**: Mouse input is routed top-down through the widget tree using parent-relative coordinates. Hit testing against children must use the parent-visible intersection of each child, but child-local coordinates must still be computed from the child’s true origin so partially clipped widgets receive correct local positions. Mouse capture is global, yet dispatch remains hierarchical: ancestors route motion and button-up events to the captured descendant by translating coordinates through the full parent chain, and `ApplicationWidget` first identifies the containing top-level window before handing control back to that window subtree. `WindowFrameWidget` owns titlebar/resize behavior, then delegates remaining input to its client-area child via the normal widget tree rather than swallowing those events itself. Player-managed test windows also treat capture as descendant-based instead of frame-only, and the frame cursor layer explicitly restores the default arrow whenever the pointer leaves resize affordances. `TextInputWidget` adds a focused single-line entry control; the player forwards SDL keypresses into the focused text box after enabling SDL Unicode translation during initialization.
- **Technologies**: C++, SDL 1.2, custom widget foundation/windowing layers.

## 4. Data Stores

### 4.1. Local Caching
- **Name**: Last.fm Offline Cache
- **Type**: Local file-based queue (typically stored in `~/.config/psymp3/lastfm.conf` or `%APPDATA%\PsyMP3\lastfm.conf`).
- **Purpose**: Stores scrobbles locally when an internet connection is unavailable, submitting them automatically when connectivity is restored.

## 5. External Integrations / APIs

- **Last.fm API**: Used for scrobbling audio tracks to Last.fm user profiles.
- **MPRIS (Media Player Remote Interfacing Specification)**: D-Bus interface for desktop media controls on Linux and BSD environments.

## 6. Deployment & Infrastructure

- **Target Platforms**: Cross-platform (Linux, BSD, Windows). macOS via MacPorts (unofficial).
- **Build System**: GNU Autotools (`autoconf`, `automake`). Includes support for single compilation unit builds (`--enable-final`).
- **CI/CD Pipeline**: Standard `make check` combined with GitHub Actions.
- **Testing Framework**: Custom C++ test harness with RapidCheck for invariant/property-based testing.

## 7. Security Considerations

- **Threading Security**: Strict adherence to the "Public/Private Lock Pattern". Public methods acquire the lock and call `_unlocked` variants. Order of lock acquisition must be strictly documented to avoid deadlocks.
- **Sanitizers**: Build system supports strict memory and concurrency diagnostics via AddressSanitizer (`--enable-asan`), ThreadSanitizer (`--enable-tsan`), and UndefinedBehaviorSanitizer (`--enable-ubsan`).
- **Network Security**: Uses OpenSSL and libcurl for secure HTTPS communication (e.g., with Last.fm API).

## 8. Development & Testing Environment

- **Local Setup Instructions**:
  ```bash
  ./autogen.sh
  ./configure --enable-test-harness --enable-rapidcheck
  make -j$(nproc)
  ```
- **Testing**: Test suite uses a custom framework located in `tests/`. Run all tests using `make check`. Subsystem tests (e.g., `test_flac_*.cpp`, `test_mpris_*.cpp`) can be run via provided scripts (`run_*.sh`).

## 9. Future Considerations / Roadmap

- **UI Development**: PsyMP3 2.x is currently in an early state ("2-CURRENT") where much of the front-end user interface and visualization features are incomplete. Finishing the `widget/` system and FFT visualization are major roadmap items.
- **Codec Enhancements**: Continued refinement of native decoders (such as the RFC 9639 compliant FLAC decoder) to reduce external dependencies.

## 10. Project Identification

- **Project Name**: PsyMP3
- **Repository URL**: https://github.com/segin/psymp3
- **Primary Contact/Team**: Kirn Gill II <segin2005@gmail.com>
- **Date of Last Update**: 2026-03-23

## 11. Glossary / Acronyms

- **PCM**: Pulse-Code Modulation (raw, uncompressed audio format).
- **FFT**: Fast Fourier Transform (math algorithm used here for audio visualization).
- **MPRIS**: Media Player Remote Interfacing Specification.
- **Scrobbling**: The act of automatically sending the name of a song you are listening to to Last.fm for tracking.
- **VLC-Style Pipeline**: Architecture where files are read and decoded concurrently in chunks rather than buffering the entire decoded audio into memory.
