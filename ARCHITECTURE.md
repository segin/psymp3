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
- MP3 is handled by bundled `minimp3` via `MiniMP3Codec` and `MP3NullDemuxer`; no external MP3 runtime dependency remains.
- `src/io/`: Provide the file/HTTP abstraction and the large-file-safe offset contract used across the pipeline.
- `src/audio.cpp`: Own the SDL audio device, decode thread, PCM queue, and FFT feed.
- `src/widget/`: Render the software UI, manage event routing, z-order, floating windows, and overlays.
- `src/core/display.cpp` and `src/core/surface.cpp`: Present the software-rendered UI through SDL surfaces.
- `src/core/font.cpp`: Render all UI text through the FreeType-based font layer into PsyMP3-owned surfaces.
- `src/player.cpp`: Coordinate playback state, loading, preloading, GUI updates, and integration points.
  The startup playlist populator runs on a background thread and expands command-line `.m3u` / `.m3u8` files inline so mixed file-and-playlist argument lists preserve user order.
- `src/core/FileDialog.cpp`: Native "open file" chooser (`PsyMP3::Core::FileDialog`) backed by a build-time-selected toolkit (Qt 6/5/4/3, GTK+ 4/3/2, or Win32 comdlg32). Isolated in its own `libpsymp3-filedialog.a` convenience library so the GUI-toolkit headers/flags reach no other translation unit; compiled out entirely (and the I/L keys left unbound) when `configure` finds no toolkit.

## Key Runtime Rules

- Public/private lock pattern: public methods acquire locks and delegate to `_unlocked` helpers.
- The audio callback never blocks on decode or file I/O.
- Same-format track transitions reuse the live `Audio` object and device.
- `Audio` keeps decoded stream format separate from SDL's obtained device format so playback timing, seeks, and reuse decisions stay tied to source PCM instead of backend conversion details.
- Loader-thread prebuffering primes PCM before track handoff so both reused and recreated audio paths do not start from an empty queue.
- The decoder thread discards stale decode results after a stream swap.
- Playlist population stays asynchronous, and inline playlist arguments are flattened in place before later command-line media paths are appended.
- Text stays UTF-8 internally once it enters the process: command-line file arguments are normalized before playlist population, Unix file I/O opens `TagLib::String` paths through UTF-8 byte strings, SDL window titles receive UTF-8, and the FreeType font layer measures/renders decoded Unicode codepoints instead of raw UTF-8 bytes.
- Widget input is hierarchical, clip-aware, and capture-aware.
- The `I` key inserts files chosen from a native dialog at the current playlist index and jumps playback to the first inserted track; the `L` key plays a chosen file in place of the current track without touching the playlist, so the next track change resumes normal flow and forgets the override. Both keys exist only when a file-dialog backend is compiled in.

## SDL2 Status

- `Display` owns the SDL window and presents through the wrapped window surface.
- Text input uses `SDL_TEXTINPUT`.
- Audio uses SDL2 device APIs.
- Font rendering uses the original FreeType path, with the shared FreeType bootstrap living under `src/core/`.
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
- Last updated: 2026-04-06
