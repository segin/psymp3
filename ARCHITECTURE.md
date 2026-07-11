# Architecture Overview

PsyMP3 is a cross-platform audio player with a software-rendered UI, asynchronous media loading, on-demand decoding, FFT visualization, and optional desktop integrations such as MPRIS and Last.fm.

This file is the compact architectural map. Detailed subsystem notes, policy-level implementation details, and porting-specific commentary live in [docs/ARCHITECTURE_DETAILS.md](/home/segin/psymp3/docs/ARCHITECTURE_DETAILS.md).

## Project Layout

```text
[Project Root]/
├── src/
│   ├── codecs/      # Audio decoders
│   ├── demuxer/     # Container parsers / stream assembly
│   ├── dsp/         # Real-time DSP (equalizer)
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
- `src/dsp/Equalizer.cpp`: Real-time 7-band biquad equalizer applied to the output PCM (see below).
- `src/widget/`: Render the software UI, manage event routing, z-order, floating windows, and overlays.
- `src/core/display.cpp` and `src/core/surface.cpp`: Present the software-rendered UI through SDL surfaces.
- `src/core/font.cpp`: Render all UI text through the FreeType-based font layer into PsyMP3-owned surfaces.
- `src/player.cpp`: Coordinate playback state, loading, preloading, GUI updates, and integration points.
  The startup playlist populator runs on a background thread and expands command-line `.m3u` / `.m3u8` files inline so mixed file-and-playlist argument lists preserve user order.
- `src/core/FileDialog.cpp`: Native "open file" chooser (`PsyMP3::Core::FileDialog`) backed by a build-time-selected toolkit (Qt 6/5/4/3, GTK+ 4/3/2, or Win32 comdlg32). Isolated in its own `libpsymp3-filedialog.a` convenience library so the GUI-toolkit headers/flags reach no other translation unit; compiled out entirely (and the I/L keys left unbound) when `configure` finds no toolkit.
- `src/widget/ui/MenuBarWidget.cpp`: In-app, software-drawn menu bar (see below).

## In-App Menu Bar

The menu bar is a single widget, `MenuBarWidget` (namespace `PsyMP3::Widget::UI`), not a tree of per-menu / per-item widgets. Menus and items are plain data structs it paints and hit-tests itself.

- **One overlay widget.** `MenuBarWidget` spans the whole logical surface (640×400) and is added as a top-most window at `ZOrder::MAX` — above all UI including toasts (`ApplicationWidget::getWindowZOrder` special-cases it). Its `Surface` is transparent everywhere except the top bar and any open dropdown/submenu, so clicks and hover pass through to the widgets beneath when nothing is open.
- **Data, not widgets.** A private `Menu` struct (name + cached bar x/width) holds a `std::vector<Item>`. The public `Item` struct is a tagged-by-convention row — a *leaf* (`action`, optional `checked` predicate for a radio dot, optional right-aligned `shortcut` hint), a *separator*, or a *submenu* (self-nesting `std::vector<Item>`). Built via the `leaf()` / `sep()` / `sub()` factories.
- **Immediate-mode paint.** `rebuild()` repaints the entire overlay from current state on every change: transparent fill, opaque bar, then the open dropdown and any expanded submenu. Text is drawn with the FreeType LCD/ClearType path (`Font::RenderLCD`); `&` in a label marks a mnemonic drawn underlined, and `shortcut` renders right-aligned in its own reserved column.
- **Self-contained hit-testing.** Because there are no child widgets, `handleMouseDown` / `handleMouseMotion` do the geometry directly via shared pure helpers (`barHitTest`, `dropdownRect`, `submenuRect`, `itemAt`, `popupWidth/Height`) so draw and hit-test never disagree. A leaf click closes the menu and invokes its `action`; returning `false` when closed lets input fall through.
- **Keeps the UI live.** Since the bar is painted and dispatched by the normal widget/event loop rather than a native OS menu, the visualizer keeps animating while a menu is open (no modal message pump). Menu actions call the same player methods as the corresponding keys; `checked` predicates reflect live state.
- **Player wiring.** `src/player.cpp` assembles the `Item` trees (File; Playback → Pause / Prev / Next / Volume / Equalizer; Settings → FFT Mode / Delay / Intensity / 2x Zoom) and calls `addMenu()`. Each key-mirroring leaf calls a named `Player` action method (`volumeUp`, `setIntensity`, `toggleZoom`, …) that the keyboard switch calls too — menus invoke the real action directly rather than injecting synthetic key events.
- **Keyboard driver.** `MenuBarWidget::handleKey(SDL_keysym)` is the single entry point, called first from `Player::handleKeyPress`. When no menu is open it claims only **Alt+&lt;mnemonic&gt;** (from the `&` in each menu name — Alt+F/P/S) and opens that menu; otherwise it returns `false` so global shortcuts run. While a menu is open it is *modal for the keyboard*: Up/Down move the selection (skipping separators, wrapping via `stepSelectable`), Right enters a submenu or advances to the next top-level menu, Left backs out of a submenu or moves to the previous menu, Enter/Space activate (opening a submenu or running the leaf `action`), Esc backs out one level then closes, and a bare mnemonic letter jumps to/activates the matching item. Every key is consumed while open so none leak to the global shortcut table. Submenu-vs-dropdown focus is derived from the existing `m_open_sub`/`m_hover_sub` state — no new focus fields.

## Equalizer

A 7-band graphic equalizer, opened from **Playback → Equalizer…**.

- **DSP (`src/dsp/Equalizer.cpp`).** A cascade of seven RBJ peaking biquads (60/150/400/1k/2.4k/6k/15k Hz), one filter state per channel, applied in place to the output PCM inside the SDL audio callback — after the silence-fill, before the FFT tap, so the spectrum reflects the EQ. It is real-time-safe like `Audio::m_volume`: the UI thread pushes band gains via atomics and bumps a dirty counter; the audio thread owns the coefficients and history and recomputes lazily. Filter history is zeroed on seek/track-change via an atomic reset flag. `Audio` owns one `Equalizer` and exposes thin `setEq*`/`getEq*` forwarders; the EQ is disabled by default (a no-op).
- **State ownership.** `Player` holds the canonical band gains + enabled flag and re-applies them to each new `Audio` (mirroring volume), so settings persist across track changes.
- **UI (`src/widget/ui/EqualizerWindow.cpp`).** A draggable in-app window (a `WindowFrameWidget` hosted in `m_random_windows`) whose client is a `LayoutWidget` of one `SliderWidget` fader per band (live dB readout + frequency label), an `EqualizerCurveWidget` preview, an enable checkbox, and an embedded `MenuBarWidget` with **Presets** (built-ins) and **User Presets** (five `.psymp3eq` slots in the config dir; a *Save* submenu stores into a slot). Moving a fader routes through one change path that updates the curve, the readout, and the DSP.
- **Curve preview.** `EqualizerCurveWidget` (a `DrawableWidget`) plots the band gains as a smooth curve using `core/BezierCurve.h` — a Catmull-Rom spline through the control points expressed as cubic Bézier segments.
- **New toolkit pieces.** `SliderWidget` (vertical/horizontal fader), `EqualizerCurveWidget` (curve canvas), and `core/BezierCurve.h` (reusable Bézier/smoothing helpers) were added for this feature and are usable elsewhere.

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
- Last updated: 2026-07-11
