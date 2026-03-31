# Architecture Overview

PsyMP3 is a cross-platform audio player with a software-rendered UI, asynchronous media loading, on-demand decoding, FFT visualization, and optional desktop integrations such as MPRIS and Last.fm.

This file is the compact architectural map. Detailed subsystem notes, policy-level implementation details, and porting-specific commentary live in [docs/ARCHITECTURE_DETAILS.md](/home/segin/psymp3/docs/ARCHITECTURE_DETAILS.md).

## Project Layout

```text
[Project Root]/
├── src/
│   ├── codecs/      # Audio decoders
│   ├── demuxer/     # Container parsers / stream assembly
│   ├── io/          # File and HTTP I/O
│   ├── widget/      # UI/widget system
│   ├── lastfm/      # Last.fm integration
│   └── mpris/       # D-Bus / MPRIS integration
├── include/         # Public/internal interfaces
├── tests/           # Custom harness + property / integration tests
├── docs/            # Specs, plans, and extended architectural notes
├── README.md
└── ARCHITECTURE.md
```

## Media Pipeline

```text
[Audio File / HTTP Stream]
           |
           v
       [IOHandler]
           |
           v
        [Demuxer]
           |
           v
      [MediaChunk]
           |
           v
       [AudioCodec]
           |
           v
       [PCM Frames]
           |
           +-----------------+
           |                 |
           v                 v
     [Audio Output]     [FFT / UI]
```

PsyMP3 is chunk-driven. Containers are parsed into compressed `MediaChunk`s, codecs decode on demand, and decoded PCM is buffered only far enough ahead to support smooth playback and responsive interaction.

## Primary Subsystems

- `src/codecs/`: Decode AAC, FLAC, MP3, Opus, Vorbis, PCM-family, G.711, and G.722 streams into PCM.
- `src/demuxer/`: Parse Ogg, ISO BMFF, RIFF/WAV, FLAC, and raw streams into codec-ready chunks and metadata.
- `src/io/`: Provide the file/HTTP abstraction and the large-file-safe offset contract used across the pipeline.
- `src/audio.cpp`: Own the SDL audio device, decode thread, PCM queue, and FFT feed.
- `src/widget/`: Render the software UI, manage event routing, z-order, floating windows, and overlays.
- `src/display.cpp` and `src/surface.cpp`: Present the software-rendered UI through SDL surfaces.
- `src/font.cpp`: Render all UI text through the FreeType-based font layer into PsyMP3-owned surfaces.
- `src/player.cpp`: Coordinate playback state, loading, preloading, GUI updates, and integration points.

## Key Runtime Rules

- Public/private lock pattern: public methods acquire locks and delegate to `_unlocked` helpers.
- The audio callback never blocks on decode or file I/O.
- Same-format track transitions reuse the live `Audio` object and device.
- `Audio` keeps decoded stream format separate from SDL's obtained device format so playback timing, seeks, and reuse decisions stay tied to source PCM instead of backend conversion details.
- Loader-thread prebuffering primes PCM before track handoff so both reused and recreated audio paths do not start from an empty queue.
- The decoder thread discards stale decode results after a stream swap.
- Widget input is hierarchical, clip-aware, and capture-aware.

## SDL2 Status

- `Display` owns the SDL window and presents through the wrapped window surface.
- Text input uses `SDL_TEXTINPUT`.
- Audio uses SDL2 device APIs.
- Font rendering uses the original FreeType path.
- Ongoing SDL2 work is tracked in [docs/SDL2_PORT_PLAN.md](/home/segin/psymp3/docs/SDL2_PORT_PLAN.md).

## Supporting Docs

- Build and usage: [README.md](/home/segin/psymp3/README.md)
- Testing policy and commands: [TESTING.md](/home/segin/psymp3/TESTING.md)
- Extended architecture notes: [docs/ARCHITECTURE_DETAILS.md](/home/segin/psymp3/docs/ARCHITECTURE_DETAILS.md)
- SDL2 migration plan: [docs/SDL2_PORT_PLAN.md](/home/segin/psymp3/docs/SDL2_PORT_PLAN.md)

## Project Identification

- Project: PsyMP3
- Repository: `https://github.com/segin/psymp3`
- Maintainer: Kirn Gill II `<segin2005@gmail.com>`
- Last updated: 2026-03-31
