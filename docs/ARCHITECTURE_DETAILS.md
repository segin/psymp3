# Architecture Details

This file holds the extended subsystem notes that would otherwise bloat [ARCHITECTURE.md](/home/segin/psymp3/ARCHITECTURE.md). The top-level architecture document stays short on purpose; this file keeps the implementation-heavy context close at hand for contributors and reviewers.

## Codec Subsystem

- Audio codecs live in `src/codecs/`.
- External-library-backed paths currently include `faad2`, `libmpg123`, `libvorbis`, `libopus`, and `spandsp`.
- Native decoders cover RFC 9639 FLAC plus PCM-family and G.711 paths.
- FLAC is native-only; the old `libFLAC` wrapper path and legacy `Flac` stream class are gone.
- `include/codecs/FLACCodec.h` is now only a compatibility include over the native FLAC codec.
- The native FLAC decoder accepts the full RFC 9639 sample-rate range up to `1048575 Hz`.
- Corrupt native FLAC frames are rejected instead of being replaced with fabricated silence.
- MD5 integrity tracking only includes accepted FLAC frames so end-of-stream validation reflects the PCM actually returned.
- AAC-in-MP4 depends on demuxer-supplied `StreamInfo.codec_data` carrying the `esds` AudioSpecificConfig.
- Raw telephony formats rely on extension-driven detection and must preserve the original file path through raw-demuxer construction.
- Raw G.722 transport bytes do not map 1:1 to decoded PCM output, so the raw demuxer tracks encoded bytes and decoded sample counts separately for duration, timestamps, and seeks.

## Demuxer Subsystem

- Container demuxers live in `src/demuxer/`.
- Supported container families are Ogg, ISO BMFF, RIFF/WAV, native FLAC, and raw streams.

### Native FLAC demuxer

- Treats core RFC 9639 structural requirements as hard validation.
- Requires the first metadata block to be `STREAMINFO`.
- Rejects duplicate `STREAMINFO` blocks.
- Rejects reserved frame-header bits.
- Rejects non-final uncommon block sizes below 16 samples.
- Validates both frame header CRC-8 and footer CRC-16 before emitting a `MediaChunk`.

### ISO demuxer

- Extracts AAC `DecoderSpecificInfo` from `esds` boxes into `StreamInfo.codec_data`.
- Propagates `moov/udta/meta/ilst` metadata into per-stream `artist`, `title`, and `album`.
- Must preserve exact chunk offsets and exact per-sample sizes so codecs receive discrete access units rather than chunk-aligned approximations.
- Sample-time lookups operate in the track `mdhd` timescale, not milliseconds.
- Chunk timestamps are converted back into decoded PCM sample positions before reaching codecs like AAC.
- Seek and read operations must update the same live `AudioTrackInfo` instances.

### Ogg demuxer

- Exposes transport position in milliseconds even though packet granules remain codec-specific.
- Preserves consumed setup headers in `StreamInfo.codec_data` for codecs that still need them during initialization.
- Rewinds and rebuilds stream state after parse/duration scan so playback starts from the real beginning.
- Carries explicit `end_of_stream` packet metadata so codecs can distinguish terminal packets from ordinary page-ending packets.
- Opus granule handling subtracts `pre_skip` for frame-start timing.
- Packet gaps surface as `MediaChunk.packet_lost`.
- Opus packet-loss concealment must reuse the prior decoded frame size to avoid pacing distortion.
- Ogg-delivered Opus packets bypass speculative partial-packet reconstruction.

## I/O Layer

- I/O lives under `src/io/`.
- Large-file support is a build invariant.
- The public I/O contract uses a single `filesize_t` alias for logical positions and sizes.
- Platform-specific 64-bit seek/tell details stay hidden inside the file/HTTP handlers.

## Widget Input Routing

- Input is routed top-down through the widget tree using parent-relative coordinates.
- Hit testing uses the visible parent/child intersection, but child-local coordinates are still computed from the child’s true origin.
- Mouse capture is global, but dispatch remains hierarchical through the ancestor chain.
- `ApplicationWidget` first identifies the owning top-level window before routing into that subtree.
- `WindowFrameWidget` owns titlebar and resize behavior, then forwards client-area input into the normal widget tree.
- Player-managed floating windows treat capture as descendant-based, not frame-only.
- Resize cursor handling explicitly restores the default arrow when leaving resize affordances.
- `TextInputWidget` uses `SDL_KEYDOWN` for navigation/editing keys and `SDL_TEXTINPUT` for printable text.
- The main `Player` event loop handles SDL2 `SDL_WINDOWEVENT` messages and refreshes the wrapped window surface on expose/resize.

## Display and Presentation

- PsyMP3 remains software-rendered.
- `Display` and `Surface` now live under `PsyMP3::Core`.
- `Display` owns the SDL window and wraps the active window surface from `SDL_GetWindowSurface()`.
- Presentation currently uses `SDL_UpdateWindowSurface()`.
- `System` now receives the live `SDL_Window*` and only converts it to platform-native handles when needed.
- `Surface::SetAlpha()` is expressed in SDL2 terms and translates older call sites onto SDL2 blend/alpha-mod behavior.

## Font Rendering

- UI text rendering uses the original FreeType-based `Font` + `PsyMP3::Core::TrueType` path.
- `Font` now lives under `PsyMP3::Core` alongside `Display`, `Surface`, and `TrueType`.
- `src/core/truetype.cpp` owns the global FreeType library lifecycle through a process-wide static manager.
- `Font` loads `FT_Face` instances directly and rasterizes glyph bitmaps into PsyMP3-owned `Surface` instances.
- This preserves the older glyph metrics and rasterization behavior without relying on an SDL text-rendering wrapper.

## Audio Output

- Audio playback remains callback-driven.
- `Audio` owns the SDL audio device, decode thread, PCM queue, and FFT feed.
- SDL2 device-specific pause, lock, unlock, and close calls replaced the old global SDL 1.x audio functions.
- `Audio` keeps decoded stream format separate from SDL's obtained device format so timing, seek math, and stream-reuse decisions stay tied to source PCM.
- Same-format track changes no longer swap to an empty PCM queue.
- The loader thread primes roughly half a second of decoded PCM from the next stream before posting it back.
- Both `Audio::setStream()` and the `Audio` constructor install that primed lead-in directly into the live queue.
- The decoder thread keeps the active stream alive across swaps, revalidates ownership after decode work, and discards stale output after swaps.

## Integrations

- `src/mpris/` provides D-Bus / MPRIS integration on supported systems.
- `src/lastfm/` provides Last.fm scrobbling and offline caching.

## Infrastructure

- Build system: GNU Autotools
- Primary test execution: `make check`
- Extra diagnostics: `--enable-asan`, `--enable-ubsan`, `--enable-tsan`
