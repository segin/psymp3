# AGENTS.md - Project Context & Directives

> **CRITICAL**: Update this file when making architectural changes or completing major milestones.

## 1. Project Identity
**PsyMP3** is a cross-platform (Linux/Windows) C++ audio player with FFT visualization, currently in v2.x (rewrite).
- **License**: ISC
- **Maintainer**: Kirn Gill <segin2005@gmail.com>

## 2. Active Development Context (Ogg Demuxer & Opus Codec)
**Current Goal**: Complete rewrite of Ogg Demuxer and implementation of container-agnostic Opus Codec.

### Recent Accomplishments
- **Opus Codec**: implemented TOC parsing, End Trimming, PLC, FEC.
- **Tools**: Created `tests/opusout` (cli debugger) and `tests/test_opus_codec_property` (RapidCheck).
- **Ogg Demuxer**: Added self-registration to `DemuxerFactory`.

### Active Tasks
- [ ] Ogg Demuxer Rewrite (per `.kiro/specs/ogg-demuxer-fix/tasks.md`)
    - [ ] Task 1: Project Setup and Infrastructure
    - [ ] Task 2: OggSyncManager
    - [ ] Task 3: OggStreamManager
    - [ ] ... verify spec for full list
- [ ] Opus Codec Implementation (per `.kiro/specs/opus-codec/tasks.md`)

## 3. Mandatory Directives (from Steering)

### Build System & Files
- **Make**: ALWAYS use `make`. Never use direct `gcc`/`g++` or `touch` to force rebuilds.
- **Object Files**: Use `deleteFile` (or `rm` via command) to remove object files if needed, but prefer `make clean`.
- **Output**: NEVER pipe build commands to `tail`/`head` with small counts (< 50). Use `head -100` or `tail -200` if needed, but prefer full output or logging.

### Diagnostic Logging
- **Log Files**: ALWAYS use `--logfile=filename.log` for diagnostics.
- **StdOut/Err**: **FORBIDDEN** to use stdout/stderr for diagnostics.
- **Channels**: Use specific debug channels (e.g., `flac`, `ogg`, `demux`, `opus`).

### Version Control & Workflow
- **Renaming**: **ALWAYS** use `git mv`.
- **Commits**: **ONE** test unit per commit. `git commit && git push` after *every* single test unit implementation.
- **Verification**: **MANDATORY** user verification after fixes. Do not assume success from clean logs.

### Code Quality & Threading
- **Threading**: Follow **Public/Private Lock Pattern**.
    - Public: Acquire lock, call `_unlocked`.
    - Private: `_unlocked` suffix, assume lock held.
    - Document lock order.
- **Error Handling**: Exceptions with context. No error codes (unless interacting with C libs).
- **Resource Management**: RAII everywhere.
- **Standards**: C++17.

## 4. Architecture Standards

### Audio Pipeline (VLC-Style)
*Decoded audio is NOT buffered. Compressed chunks are buffered and decoded on-demand.*
```
File -> IOHandler -> Demuxer -> MediaChunk (Compressed) -> Codec -> AudioFrame
```

### Component Registration
- **Demuxers**: `DemuxerFactory::registerDemuxer` via static initializer.
- **Codecs**: `AudioCodecFactory` registration.

### Build System (Autotools)
- **Windows**: `ws2_32`, `mswsock` linked. Resources via `windres`.
- **Testing**: `./configure --enable-rapidcheck && make check`

## 5. Historical Milestones (Reference)
- **Windows Support**: Full MSYS2/MinGW compatibility (Sockets, Unicode, Resources).
- **Last.fm**: Offline caching, multi-endpoint redundancy.
- **Sockets**: Unified cross-platform `closeSocket`, `getSocketError`.

---
**Last Updated**: December 7, 2025