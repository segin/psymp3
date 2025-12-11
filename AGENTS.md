# AGENTS.md - Project Context & Directives

> **CRITICAL**: Update this file when making architectural changes or completing major milestones.

## 1. Project Identity
**PsyMP3** is a cross-platform (Linux/Windows) C++ audio player with FFT visualization, currently in v2.x (rewrite).
- **License**: ISC
- **Maintainer**: Kirn Gill II <segin2005@gmail.com>
- **Standards**: C++17

---

## 2. Architecture Overview

### Audio Pipeline (VLC-Style, On-Demand Decoding)
*Decoded audio is NOT buffered. Compressed chunks are buffered and decoded on-demand.*
```
File → IOHandler → Demuxer → MediaChunk (Compressed) → Codec → AudioFrame (PCM)
```

### Subsystem Layout
```
src/
├── codecs/         # Audio codec implementations
│   ├── flac/       # Native FLAC decoder (RFC 9639 compliant, no libFLAC)
│   ├── mp3/        # MP3 codec (libmpg123 wrapper)
│   ├── opus/       # Opus codec (libopus wrapper)
│   ├── vorbis/     # Vorbis codec (libvorbis wrapper)
│   └── pcm/        # PCM codecs: Raw PCM, G.711 (μ-law, A-law)
├── demuxer/        # Container demuxer implementations
│   ├── ogg/        # Ogg container (Vorbis, Opus, FLAC)
│   ├── iso/        # ISO Base Media File Format (MP4/M4A)
│   ├── riff/       # RIFF/WAV container
│   ├── flac/       # Native FLAC container
│   └── raw/        # Raw/headerless streams
├── io/             # I/O abstraction layer
│   ├── file/       # Local file I/O
│   └── http/       # HTTP streaming
├── widget/         # UI widget system
├── lastfm/         # Last.fm scrobbling
└── mpris/          # MPRIS media player interface (D-Bus)
```

### Component Registration (Static Initializers)
- **Demuxers**: `DemuxerFactory::registerDemuxer()` in `DemuxerRegistry.cpp`
- **Codecs**: `AudioCodecFactory::registerCodec()` in `CodecRegistration.cpp`

### Key Base Classes
| Class | Location | Purpose |
|-------|----------|---------|
| `AudioCodec` | `include/codecs/AudioCodec.h` | Codec interface |
| `SimplePCMCodec` | `include/codecs/AudioCodec.h` | Base for PCM-type codecs |
| `Demuxer` | `include/demuxer/Demuxer.h` | Container parser interface |
| `IOHandler` | `include/io/IOHandler.h` | I/O abstraction |
| `StreamInfo` | `include/demuxer/Demuxer.h` | Stream metadata |
| `MediaChunk` | `include/demuxer/Demuxer.h` | Compressed data unit |
| `AudioFrame` | `include/codecs/AudioCodec.h` | Decoded PCM data |

---

## 3. Supported Formats

### Containers (Demuxers)
| Format | Files | Status |
|--------|-------|--------|
| Ogg | `src/demuxer/ogg/` | ✅ Complete |
| ISO (MP4/M4A) | `src/demuxer/iso/` | ✅ Complete |
| RIFF (WAV) | `src/demuxer/riff/` | ✅ Complete |
| FLAC | `src/demuxer/flac/` | ✅ Complete |
| Raw | `src/demuxer/raw/` | ✅ Complete |

### Codecs
| Codec | Implementation | External Lib | Status |
|-------|---------------|--------------|--------|
| FLAC | Native (RFC 9639) | None | ✅ Complete |
| MP3 | Wrapper | libmpg123 | ✅ Complete |
| Vorbis | Wrapper | libvorbis | ✅ Complete |
| Opus | Wrapper | libopus | ✅ Complete |
| PCM (16/24/32-bit) | Native | None | ✅ Complete |
| A-law (G.711) | Native | None | ✅ Complete |
| μ-law (G.711) | Native | None | ✅ Complete |

---

## 4. Build Configuration

### Core Dependencies (Required)
- SDL ≥ 1.2
- TagLib ≥ 1.6
- FreeType2
- OpenSSL
- libcurl

### Optional Codec Libraries
- `libmpg123` (MP3)
- `libogg` (Ogg container)
- `libvorbis` (Vorbis)
- `libopus` (Opus)

### Configure Options
```bash
# Codec toggles
--enable-flac        # Native FLAC (default: yes)
--enable-mp3         # MP3 support (default: yes)
--enable-vorbis      # Vorbis support (default: yes)
--enable-opus        # Opus support (default: yes)
--enable-g711        # G.711 A-law/μ-law (default: yes)

# Features
--enable-mpris       # MPRIS D-Bus interface (default: auto)
--enable-rapidcheck  # Property-based testing (default: auto)

# Sanitizers
--enable-asan        # AddressSanitizer
--enable-ubsan       # UndefinedBehaviorSanitizer
--enable-tsan        # ThreadSanitizer

# Build modes
 --enable-final       # Single compilation unit build
```

> **IMPORTANT**: When modifying `Makefile.am` source lists or adding new `.cpp` files,
> `src/psymp3.final.cpp` **MUST** be updated to include the new files. The final build
> requires all sources to be `#include`d in that file. Test with `./configure --enable-final && make`.

### Build Commands
```bash
./autogen.sh && ./configure && make -j$(nproc)
./configure --enable-rapidcheck && make check  # Run tests
```

---

## 5. Mandatory Directives

### Build System
- **make**: ALWAYS use `make`. Never use direct `gcc`/`g++` or `touch`.
- **Object Files**: Prefer `make clean` over manual `rm`.
- **Output**: **NEVER** pipe build output to `tail`, `head`, `less`, or similar. Always capture full output.

### Diagnostic Logging
- **Log Files**: ALWAYS use `--logfile=filename.log`.
- **StdOut/StdErr**: **FORBIDDEN** for diagnostics.
- **Channels**: Use named channels (`flac`, `ogg`, `demux`, `opus`, `codec`).

### Version Control
- **Renaming**: ALWAYS use `git mv`.
- **Commits**: ONE test unit per commit. `git commit && git push` after each.
- **Verification**: MANDATORY user verification after fixes.

### Code Quality
- **Threading**: Follow **Public/Private Lock Pattern**.
  - Public methods: Acquire lock, call `_unlocked` variant.
  - Private methods: `_unlocked` suffix, assume lock held.
  - Document lock acquisition order.
- **Error Handling**: Exceptions with context (not error codes).
- **Resource Management**: RAII everywhere.

---

## 6. Testing Infrastructure

### Test Harness (`tests/`)
- **Framework**: Custom test harness with discovery, execution, and reporting.
- **Property Tests**: RapidCheck for invariant testing.
- **Test Scripts**: `run_*.sh` scripts for specific subsystems.

### Key Test Files
| Test Category | Pattern |
|--------------|---------|
| FLAC | `test_flac_*.cpp` |
| Ogg | `test_ogg_*.cpp` |
| Codecs | `test_*_codec*.cpp` |
| Demuxers | `test_demuxer_*.cpp` |
| IOHandler | `test_iohandler_*.cpp` |
| MPRIS | `test_mpris_*.cpp` |
| Threading | `test_*_thread_safety*.cpp` |

### Debugging Tools
| Tool | Purpose |
|------|---------|
| `flacout` | FLAC decoder CLI |
| `mp3out` | MP3 decoder CLI |
| `opusout` | Opus decoder CLI |
| `vorbisout` | Vorbis decoder CLI |

---

## 7. Historical Milestones

- **Native FLAC**: RFC 9639 compliant decoder (no libFLAC dependency)
- **Windows Support**: Full MSYS2/MinGW compatibility
- **Last.fm**: Offline caching, multi-endpoint redundancy
- **MPRIS**: D-Bus media player interface for Linux
- **Unity Build**: Fixed `--enable-final` for single compilation unit support (2025-12-11)

---

**Last Updated**: December 11, 2025