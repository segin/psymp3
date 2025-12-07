# AGENTS.md - Project Context & Directives

> **CRITICAL**: Update this file when making architectural changes or completing major milestones.

## 1. Project Identity
**PsyMP3** is a cross-platform (Linux/Windows) C++ audio player with FFT visualization, currently in v2.x (rewrite).
- **License**: ISC
- **Maintainer**: Kirn Gill <segin2005@gmail.com>

## 2. Active Development Context (Opus & Testing)
**Current Goal**: RFC-compliant Opus support and distinct component property testing.

### Recent Accomplishments
- **Opus Codec**: implemented TOC parsing, End Trimming, PLC, FEC.
- **Tools**: Created `tests/opusout` (cli debugger) and `tests/test_opus_codec_property` (RapidCheck).
- **Ogg Demuxer**: Added self-registration to `DemuxerFactory`.

### Active Tasks
- [x] Pass `test_opus_codec_property`
- [ ] Verify End Trimming with accurate granule positions

## 3. Mandatory Directives

### Version Control
- **Renaming**: **ALWAYS** use `git mv`. Never use regular `mv`.
- **Commits**: ONE unit test per commit. `git commit && git push` after *every* single test unit implementation.

### Code Quality
- **Error Handling**: Use exceptions with context. No error codes.
- **Resource Management**: RAII everywhere.
- **Modern C++**: C++17 standards.

## 4. Architecture Standards

### Audio Pipeline (VLC-Style)
*Decoded audio is NOT buffered. Compressed chunks are buffered and decoded on-demand.*
```
File -> IOHandler -> Demuxer -> MediaChunk (Compressed) -> Codec -> AudioFrame
```

### Component Registration
- **Demuxers**: Must use `DemuxerFactory::registerDemuxer` via static initializer (see `src/demuxer/ogg/OggDemuxer.cpp`).

### Build System (Autotools)
- **Windows**: `ws2_32`, `mswsock` linked. Resources via `windres`.
- **Testing**: `./configure --enable-rapidcheck && make check`

## 5. Historical Milestones (Reference)
- **Windows Support**: Full MSYS2/MinGW compatibility (Sockets, Unicode, Resources).
- **Last.fm**: Offline caching, multi-endpoint redundancy.
- **Sockets**: Unified cross-platform `closeSocket`, `getSocketError`.

---
**Last Updated**: December 7, 2025