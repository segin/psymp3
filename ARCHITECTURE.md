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

**Metadata flow.** Demuxers that understand their container's tags populate `Demuxer::m_tag` with the in-house Tag framework (`include/tag/`): Ogg and FLAC store a `VorbisCommentTag`, and the MP3 demuxer parses the leading ID3v2 and trailing ID3v1 blocks into an `ID3v2Tag`/`ID3v1Tag` (merged as `MergedID3Tag` when both exist, ID3v2 taking precedence per field). `DemuxedStream`'s metadata getters (`getArtist`/`getTitle`/`getAlbum`/`getMusicBrainzID`/…) read `m_tag` first and fall back per-field to the base `Stream` implementation (TagLib `FileRef`) when a field is missing, so the titlebar, MPRIS, Last.fm, and Discord all see the same values. ID3v1 fields are decoded ISO-8859-1 → UTF-8 at parse time, matching ID3v2's encoding-0 handling.

## Primary Subsystems

- `src/codecs/`: Decode AAC, FLAC, MP3, Opus, Vorbis, PCM-family, G.711, and G.722 streams into PCM. Vorbis uses the vendored stb_vorbis (`third_party/stb`, compiled as its own C object) in pushdata mode: the Ogg demuxer delivers de-paged packets, and `VorbisCodec` wraps each one back into minimal Ogg page framing (real lacing/continuation and page CRC — stb validates CRCs during its post-seek resync) before feeding the decoder. No libvorbis at link time; libogg remains for container parsing.
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

- **DSP (`src/dsp/Equalizer.cpp`).** A cascade of seven RBJ peaking biquads (60/150/400/1k/2.4k/6k/15k Hz), one filter state per channel, applied in place to the output PCM inside the SDL audio callback — *after* the volume scaling, so that at sub-100% volume the attenuation leaves headroom and positive band gains are far less likely to clip loud material. (The FFT tap runs before both volume and EQ, so the spectrum stays volume- and EQ-independent.) It is real-time-safe like `Audio::m_volume`: the UI thread pushes band gains via atomics and bumps a dirty counter; the audio thread owns the coefficients and history and recomputes lazily. Filter history is zeroed on seek/track-change via an atomic reset flag. `Audio` owns one `Equalizer` and exposes thin `setEq*`/`getEq*` forwarders; the EQ is disabled by default (a no-op).
- **State ownership.** `Player` holds the canonical band gains + enabled flag and re-applies them to each new `Audio` (mirroring volume), so settings persist across track changes — and across restarts: volume and EQ state are loaded from / saved to `psymp3.conf` (key=value, in the config dir) at construction/shutdown.
- **UI (`src/widget/ui/EqualizerWindow.cpp`).** A draggable in-app window (a `WindowFrameWidget` hosted in `m_random_windows`) whose client is a `LayoutWidget` of one `SliderWidget` fader per band (live dB readout + frequency label), an `EqualizerCurveWidget` preview, an enable checkbox, and an embedded `MenuBarWidget` with **Presets** (built-ins) and **User Presets** (five `.psymp3eq` slots in the config dir; a *Save* submenu stores into a slot). Moving a fader routes through one change path that updates the curve, the readout, and the DSP.
- **Curve preview.** `EqualizerCurveWidget` (a `DrawableWidget`) plots the band gains as a smooth curve using `core/BezierCurve.h` — a Catmull-Rom spline through the control points expressed as cubic Bézier segments.
- **New toolkit pieces.** `SliderWidget` (vertical/horizontal fader), `EqualizerCurveWidget` (curve canvas), and `core/BezierCurve.h` (reusable Bézier/smoothing helpers) were added for this feature and are usable elsewhere.

## Widget Toolkit

The software-rendered UI (`src/widget/`) imitates Windows 3.1, and since 2.0-BETA2 it does so to the pixel, with a real keyboard focus model layered on top.

- **Win3.1 rendering fidelity.** `ButtonWidget` paints the reference push button: 1px black outline with notched (transparent) corners, 2px white top/left highlight, stepped 2px grey bottom/right shadow, 24px tall, label centered over the bevels. The window's *default* button (`setDefault`) carries the bold double border; a focused button shows label-bounded focus dots. `ScrollbarWidget` is 17px wide with 17×17 end pieces whose outlines merge into the control's frame, a fixed square thumb, stem-and-head arrow glyphs (3×3 stem, 7→5→3→1 head), a 50% white/grey dithered shaft, and an inverted-dither pressed-track indicator.
- **Press gestures.** All push buttons (including the titlebar minimize/maximize/restore buttons drawn by `WindowFrameWidget`) follow one gesture model: sink on mouse-down with capture, track hover while held (drag off = pop up, drag back = re-sink), fire only on release inside. Scrollbar arrows additionally sink their glyph 1px down-right and auto-repeat while held (300ms initial delay, 60ms cadence, pausing while the cursor is off the button); track paging uses the same repeat clock, ticked once per frame from `recursiveBlitTo`.
- **Keyboard focus.** Focus is per-class static state (`TextInputWidget`, `ListViewWidget`, `ButtonWidget` each track their focused instance) rather than a central focus manager. `Player::handleKeyPress` routes keys through a fixed chain: menu bar first, then the focused text input (which swallows everything except Alt chords and F-keys; Escape blurs; Ctrl+V pastes the system clipboard, filtered to one line), then the focused list (arrows move the cursor, Enter fires `setOnActivate`, Delete fires `setOnDelete`), then the focused button (Enter activates immediately; Space sinks and fires on release), then window-level keys (Ctrl+F4 closes the active frame), then global shortcuts. Tab/Shift+Tab walk a DFS-collected list of focusable widgets (`collectFocusables`); Enter with no focused button activates the window's default button. Clicking empty space clears all three focus classes.
- **Window frames.** `WindowFrameWidget` owns the titlebar, the L-shaped corner resize zones (outer edge + notch, with per-corner cursors), double-click-to-close, and the control menu — which is an ordinary `UI::ContextMenuWidget` child (entries carry enabled flags, separators, and an accelerator column), not bespoke drawing. While the menu is open the system-menu icon inverts; clicking anywhere else dismisses it. Menu entries: Restore/Move/Minimize/Maximize/Close (Ctrl+F4), disabled to match window state; Move enters a pointer-follow move mode.
- **Blit model.** Top-level windows blit via `BlitTo`; children composite via `recursiveBlitTo` under `ClipRectGuard`. Overlay-ish children (context menus) render as real children of their frame so z-order and hit-testing stay inside the ordinary widget tree.

## MPRIS / D-Bus Integration

`src/mpris/` implements MPRIS2 as five cooperating components coordinated by `MPRISManager`; incoming method calls are dispatched, not just properties broadcast.

- **Component split.** `DBusConnectionManager` owns the bus connection and name; `PropertyManager` is the thread-safe property store (metadata, playback status, position with wall-clock interpolation while Playing); `MethodHandler` is the dispatch table for incoming calls; `SignalEmitter` batches and emits `PropertiesChanged`/`Seeked` from a worker thread (PropertiesChanged coalesces within a 50ms window; `signals_sent` counts batches). `MPRISManager` wires them, registers the object path, and exposes the player-facing update API.
- **The pump.** Incoming traffic is dispatched by `MPRISManager::processEvents()` — a non-blocking `dbus_connection_read_write_dispatch` that must be called from the main thread (dispatched handlers call non-thread-safe `Player` methods). Nothing pumps autonomously: no pump, no replies.
- **Connection policy.** Connections are *private* (`dbus_bus_get_private`-style), never shared — libdbus caches shared connections and the session address globally, which made bus-restart recovery impossible. The session address is read from the environment on every connect. The well-known name `org.mpris.MediaPlayer2.psymp3` is requested with `DO_NOT_QUEUE`; if another instance owns it, the spec's `…psymp3.instance<pid>` fallback is registered instead (`getServiceName()` reports what was actually acquired, and cleanup releases that name).
- **Reconnection.** Automatic retries are budgeted and exponentially backed off; a successful reconnect resets the budget, and explicit caller-initiated `reconnect()` bypasses the backoff (`attemptReconnection(force)`). After reconnecting, the manager re-registers the object path on the new connection — a fresh connection carries none of the old registration.
- **Null-player mode.** Constructing `MPRISManager(nullptr)` is supported end-to-end (headless/testing): the `MethodHandler` is still created and answers commands with error replies while properties work normally, `isReady()` holds, and `initialize()` after `shutdown()` starts a fresh lifecycle.
- **Spec surface.** `org.freedesktop.DBus.Introspectable.Introspect` returns the complete MPRIS2 XML; the root interface includes `DesktopEntry`; `Properties.GetAll` on an interface without registered properties returns an empty dict per the D-Bus spec.

## Last.fm Scrobbling

`src/lastfm/` scrobbles via the Last.fm **Web Services API 2.0** (`ws.audioscrobbler.com/2.0/`), using PsyMP3's registered API key/shared secret (embedded in `LastFM.cpp`).

- **Auth.** `auth.getMobileSession` (username + plaintext password over HTTPS, request signed with the shared secret: MD5 over name-sorted `<param><value>` pairs + secret). The issued session key never expires; it is persisted as `session_key=` in `lastfm.conf`, after which the plaintext password is dropped from memory (and only ever needed again if the user revokes the app — error 9 clears the stored key so the next attempt re-authenticates). Hard auth failures (bad credentials, bad API key) disable scrobbling for the session instead of retrying.
- **Submission.** `track.scrobble` (one call per track, drained in batches by the background submission thread with exponential backoff) and `track.updateNowPlaying`. A `status="ok"` with an *ignored* verdict still counts as submitted — resubmitting can only be ignored again. API 2.0 has no removeNowPlaying call, so clearing (pause/stop, and unconditionally on destruction so exit never leaves a stale status) works by re-submitting the last-shown track with `duration=1`, replacing the live status with one that expires immediately.
- **Offline cache.** Failed/pending scrobbles persist across runs in `scrobble_cache.xml` (0600, like `lastfm.conf`).
- **MusicBrainz IDs.** `track` carries the recording MBID from the file's tags — TagLib's normalized `PropertyMap` key `MUSICBRAINZ_TRACKID` on the primary path, and the custom Tag framework (`ID3v2Tag` reads the Picard `UFID:http://musicbrainz.org` frame and matches `TXXX` frames by description; Vorbis comments are generic already) on the decoder-fallback path. Scrobbles and now-playing updates pass it as `mbid` only when it is a well-formed UUID (`LastFM::isValidMBID`), and it round-trips through the offline cache.
- **Multi-artist credits.** A multi-valued artist tag (several Vorbis `ARTIST` fields, ID3v2.4 multi-value TPE1) is kept value-separate all the way through the demuxers and Tag framework; display strings join every artist (", "), while Last.fm submission uses the **first artist only** (the API has no multi-artist model — a joined credit lands on a nonexistent artist page). `Stream::getPrimaryArtist()` / `getMusicBrainzID()` are the submission-side accessors; the Player's scrobble/now-playing sites build their metadata-only `track` from the *stream* (not `track::loadTags`), so that is where submission metadata must be wired.
- **Credentials UI.** Settings → "Last.fm Credentials..." (a `WindowFrameWidget` dialog in `player.cpp`) edits username/password with a masked password field (`TextInputWidget::setPasswordMode`), and a Test button that runs a stateless `LastFM::testCredentials` handshake on a detached worker thread. Saving rewrites `lastfm.conf` (dropping a stale `session_key` when the account or password changes) and recreates the `LastFM` instance; the old instance is destroyed *first* because its worker also writes the conf on auth success.

## Discord Rich Presence

`src/discord/DiscordPresence.cpp` shows "Listening to PsyMP3" via the Discord desktop client's local IPC (framed-JSON over `discord-ipc-N` — unix socket on POSIX incl. Flatpak/snap paths, named pipe on Windows; no external library).

- **Worker model.** Same shape as `LastFM`: public setters record a last-write-wins pending activity and notify; one worker thread owns all socket I/O, retries connecting every 15s while Discord is absent, and re-sends the current presence after a reconnect. Updates are sent only on track change/pause/seek/stop — the progress bar is client-rendered from start/end timestamps.
- **Activity shape.** Type 2 (Listening): `details`=title (filename stem for untagged files), `state`=artist (+" (paused)"), timestamps only while playing with known length, and Cover Art Archive artwork (`front-250`) keyed by the track's MusicBrainz *release* ID (`Stream::getMusicBrainzReleaseID()`, validated as a UUID first). Strings are clamped to Discord's 2–128-byte limits on UTF-8 boundaries; absent fields are omitted, never faked.
- **Wiring.** `Player::updateDiscordPresence()` mirrors player state; called from `submitNowPlaying` (start/resume), pause, stop, and seek. Settings → "Discord Presence" toggles it (`discord_presence` in psymp3.conf). The application ID is a compiled-in constant (`PSYMP3_DISCORD_CLIENT_ID` env overrides for testing); empty ID leaves the worker dormant.

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
- Dropping files onto the window (SDL `DROPBEGIN`/`DROPFILE`/`DROPCOMPLETE`) acts like "Open": the whole drop batch — directories recursed for supported media files, `.m3u`/`.m3u8` playlists expanded, unsupported files silently ignored — replaces the playlist in one step and plays from its first track. Available even in builds without a file-dialog backend.

## SDL3 Status

- The whole tree builds against SDL3 (Linux and all three Windows cross-arches); there is no SDL2 fallback.
- `Display` owns the SDL window and presents through the wrapped window surface; teardown order is guarded with `SDL_WasInit` so late destructors never touch a quit subsystem.
- Text input uses `SDL_EVENT_TEXT_INPUT`; SDL3's bool-returning APIs (`SDL_Init`, `SDL_LockSurface`, …) are used with their SDL3 semantics.
- Audio uses the SDL3 stream/device APIs; headless environments run under the `dummy` audio driver.
- Font rendering uses the original FreeType path, with the shared FreeType bootstrap living under `src/core/`.

## Supporting Docs

- Build and usage: [README.md](/home/segin/psymp3/README.md)
- Testing policy and commands: [TESTING.md](/home/segin/psymp3/TESTING.md)
- Extended architecture notes: [docs/ARCHITECTURE_DETAILS.md](/home/segin/psymp3/docs/ARCHITECTURE_DETAILS.md)

## Project Identification

- Project: PsyMP3
- Repository: `https://github.com/segin/psymp3`
- Maintainer: Kirn Gill II `<segin2005@gmail.com>`
- Last updated: 2026-08-21
