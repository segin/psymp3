# SDL2 → SDL3 Migration Plan — PsyMP3

> **Status (2026-08-17):** IMPLEMENTED on branch `sdl3-migration`. Phases 0–5 are done; the Linux unity build and the Windows x86_64 llvm-mingw cross-build are both green under `-Werror`, linking real SDL3 (3.4.14). Remaining: full 3-arch Windows confirmation and the Playlist Manager live-drop-bar *feature* (the `SDL_EVENT_DROP_POSITION` case + plumbing is landed; the UI is follow-up work). This document is retained as the record of the migration.
> **Target:** SDL 3.4.14 (`/usr/include/SDL3`).
> **Scope:** Derived from seven subsystem inventories, each verified against the installed SDL3 headers.

---

## 1. Executive Summary

The migration touches **~110 distinct SDL symbols, struct members, and macros** across roughly a dozen source files. The overwhelming majority (~75%) are mechanical renames or ignored-return-value changes that compile with minimal edits. The real work concentrates in five files: `src/player.cpp` (event pump, timers, Win32), `src/audio.cpp` (backend rewrite), `src/core/surface.cpp`, `src/core/display.cpp`, and `src/widget/ui/Label.cpp` (pixel-format plumbing).

**Overall effort:** Large. Estimate ~2 person-weeks. Two subsystems (audio backend, Win32 SYSWM) are genuine re-architectures, not ports; the rest is 1–3 days of careful mechanical work plus a build-green pass.

### Biggest risks

| # | Risk | Why it hurts |
|---|------|--------------|
| **R1** | **Audio backend rewrite** (`audio.cpp`) | The SDL2 pull-callback hands you a buffer to fill in place; SDL3's `SDL_AudioStreamCallback` hands you **no buffer** — you assemble PCM in an app-owned scratch buffer and push it with `SDL_PutAudioStreamData`. The FFT/volume/EQ tap runs in real-time (RT-safe) code and must be restructured around this. Highest-risk change in the whole migration. |
| **R2** | **Win32 SYSWM removal** (`player.cpp`, `system.cpp`) | `SDL_syswm.h`, `SDL_SysWMinfo`, `SDL_GetWindowWMInfo`, `SDL_SYSWMEVENT`, `event.syswm`, and `SDL_VERSION(&struct)` are all **deleted**. HWND now comes from window *properties*; native `WM_COMMAND`/`THBN_CLICKED`/`TaskbarButtonCreated` interception must move to `SDL_SetWindowsMessageHook`, which fires on SDL's **internal message thread** → needs thread marshalling. A design change, not a rename. |
| **R3** | **`SDL_Init`-family return-value inversion** | `SDL_Init`, `SDL_InitSubSystem`, `SDL_SetThreadPriority`(→`SDL_SetCurrentThreadPriority`), `SDL_StartTextInput` all now return **bool (true=success)**. Every existing `< 0` failure test compiles clean, never fires, and **silently swallows real init failures**. Invisible to the compiler — must be audited by hand. |
| **R4** | **Event enum/struct flattening** | `SDL_WINDOWEVENT` collapses into ~25 top-level `SDL_EVENT_WINDOW_*` types (`.window.event` subtype field gone); `SDL_keysym` type **removed** (`event.key.keysym.sym`→`event.key.key`); coordinates float. Touches the pump, `Screen::handleWindowEvent`, `Display::handleWindowEvent`, and three widget key handlers. |
| **R5** | **Surface `format` is now an enum, not a struct pointer** | Every `surf->format->BytesPerPixel/BitsPerPixel` breaks; `SDL_MapRGB/MapRGBA/GetRGBA` gained `const SDL_PixelFormatDetails*` + `SDL_Palette*` params. `SDL_CreateRGBSurface*`→`SDL_CreateSurface`/`SDL_CreateSurfaceFrom` (masks dropped, args reordered). Hot pixel loops in `surface.cpp`/`Label.cpp` must cache `SDL_GetPixelFormatDetails()` outside the loop. |
| **R6** | **`SDL_GetTicks` is now `Uint64`; mouse coords are `float`** | 21 tick call sites store into `Uint32` members/locals (narrowing → `-Wconversion`/`-Werror`, reintroduces ~49-day wrap). Mouse `x/y`/`wheel.y` are `float`; logical-scale integer divides change rounding and downstream `int` reads narrow. |

### The concrete win that motivated this migration

**SDL3 adds `SDL_EVENT_DROP_POSITION`**, which fires continuously while a drag hovers the window, carrying `event.drop.x` / `event.drop.y` / `windowID`. SDL2 had no equivalent — there was no cross-platform way to know *where* in the window a drag was hovering before the drop landed. This unblocks the **Playlist Manager live blue insertion bar**: as the user drags files over the list, we can compute the insertion index from the hover Y and render the indicator, cross-platform. See Phase 5.

---

## 2. Phased Migration Plan

Phases are dependency-ordered. Each is independently buildable except where noted. Prefer atomic, per-module commits (`Audio: …`, `Core: …`, `Player: …`) per the project's commit policy.

### Phase 0 — Build system *(in progress, parallel)*

**Changes:** `configure.ac` pkg-config `sdl2`→`sdl3`; umbrella include `<SDL.h>`→`<SDL3/SDL.h>` (`include/psymp3.h:267`); **delete** `#include <SDL_syswm.h>` (`psymp3.h:235`) and the unused `#include <SDL_mutex.h>` (`psymp3.h:269`); decide `SDL_MAIN_HANDLED` vs SDL3-provided `WinMain` (see Decisions).
**Files:** `configure.ac`, `Makefile.am`, `include/psymp3.h`, `src/main.cpp`.
**Risk:** LOW-MED. **Effort:** S.
**Verify:** `./autogen.sh && ./configure` resolves `sdl3` via pkg-config; the tree will not yet compile — that is expected until Phase 1–5 land.

> Because a broken build blocks all verification, land Phases 1–5 on a branch and only merge once Phase 6 is green. The `sdl2-compat` shim is an alternative interim (see Decisions D6).

### Phase 1 — Mechanical renames (event enums, keymods, surface free/blit/fill, GetTicks, clip rects)

The high-volume, low-risk bulk. Do this first so later phases work against SDL3-named symbols.

**Changes:**
- Event enum renames: `SDL_KEYDOWN`→`SDL_EVENT_KEY_DOWN`, `SDL_KEYUP`, `SDL_TEXTINPUT`, `SDL_MOUSEBUTTON{DOWN,UP}`, `SDL_MOUSEMOTION`, `SDL_MOUSEWHEEL`, `SDL_QUIT`, `SDL_USEREVENT`→`SDL_EVENT_USER`, `SDL_DROP{BEGIN,FILE,COMPLETE}`→`SDL_EVENT_DROP_*`. Both as case labels and synthesized `event.type` assignments.
- Keymod renames: `KMOD_*`→`SDL_KMOD_*` (`SHIFT/LCTRL/RCTRL/LALT/RALT`); values unchanged.
- Surface renames: `SDL_FreeSurface`→`SDL_DestroySurface`, `SDL_FillRect`→`SDL_FillSurfaceRect`, `SDL_GetClipRect`→`SDL_GetSurfaceClipRect`, `SDL_SetClipRect`→`SDL_SetSurfaceClipRect`, `SDL_FreeCursor`→`SDL_DestroyCursor`.
- `SDL_GetTicks` → `Uint64`: widen all tick storage (`m_drag_start_time`, `m_track_start_time`, `s_last_gui_iteration_tick`, `m_state_change_time`, `m_next_repeat_ms`, and local vars) to `Uint64`/`uint64_t` per the project-wide decision (D5).
- Audio format enum: `AUDIO_S16`→`SDL_AUDIO_S16` (never hard-code `0x8010`).
- `SDLK_*` keycodes: **no edits** — names/values unchanged (`SDL_Keycode` is now a `Uint32` typedef, still legal as switch labels). `SDLKey`/`SDL_keysym` project aliases are handled in Phase 5.

**Files:** `src/player.cpp`, `src/core/surface.cpp`, `src/core/display.cpp`, `src/audio.cpp`, `src/widget/**` (WindowFrameWidget, FadingWidget, ScrollbarWidget, ListViewWidget, SpectrumAnalyzerWidget, WindowWidget, Label, ToastWidget, LyricsWidget), `include/player.h`, `include/widget/**`.
**Risk:** LOW (bulk), MED (GetTicks width decision is cross-cutting).
**Effort:** M.
**Verify:** These do not compile in isolation (window/surface/audio/key symbols still SDL2-shaped), so verify via the Phase 6 build. Grep confirms zero remaining `KMOD_`, `SDL_MOUSEBUTTONDOWN`, `SDL_FreeSurface`, `AUDIO_S16` tokens.

### Phase 2 — Window & surface creation

**Changes:**
- `SDL_CreateWindow`: drop x/y args and `SDL_WINDOW_SHOWN` (`display.cpp:31`). Restore centering via `SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, …)` after create, or `SDL_CreateWindowWithProperties` (decision D2).
- `SDL_CreateRGBSurface*`→`SDL_CreateSurface(w,h,fmt)` (`surface.cpp:66,101,104`) and `SDL_CreateSurfaceFrom(w,h,fmt,pixels,pitch)` (`display.cpp:63` — **arg order changed**, silent-bug risk). Pick `XRGB8888` vs `RGBA32` default (D1); delete the `#if SDL_BYTEORDER` mask block for text surfaces (use `SDL_PIXELFORMAT_RGBA32`).
- `SDL_CreateRGBSurfaceWithFormat`→`SDL_CreateSurface(W,H,m_window_surface->format)` (`display.cpp:146`; `format` is already the enum).
- `SDL_SoftStretch`→`SDL_StretchSurface(..., SDL_SCALEMODE_NEAREST)` (`display.cpp:94` — **must** pass NEAREST to preserve the integer pixel-double; do **not** use `SDL_BlitSurfaceScaled`).
- **Surface `->format` member rework** (R5): replace every `->format->BytesPerPixel` with `SDL_BYTESPERPIXEL(surf->format)` or a cached `SDL_GetPixelFormatDetails(surf->format)->bytes_per_pixel` (`surface.cpp:253,338,379,918`, `display.cpp:148`, `Label.cpp:442,455`).
- `SDL_MapRGB`/`SDL_MapRGBA`/`SDL_GetRGBA`: use `SDL_MapSurfaceRGB/RGBA` convenience wrappers for one-shots; for the per-pixel `applyRelativeOpacity` loop (`surface.cpp:222-233`) and `Label::applyEdgeFade` (`Label.cpp:405-462`), **hoist** `SDL_GetPixelFormatDetails` + `SDL_GetSurfacePalette` out of the loop to avoid a perf regression.
- Fix `SDL_LockSurface` inverted logic at `Label.cpp:421`: `!= 0` → `!SDL_LockSurface(handle)` (bool now returns true on success — this is a latent bug, not cosmetic).
- Bool-return-ignored calls (`SDL_BlitSurface`, `SDL_UpdateWindowSurface`, `SDL_SetSurface{BlendMode,AlphaMod}`, `SDL_SetWindow{Title,Icon,Size}`, `SDL_SetCursor`): compile as-is; optionally add `SDL_GetError` logging (D7).

**Files:** `src/core/display.cpp`, `src/core/surface.cpp`, `include/surface.h`, `src/widget/ui/Label.cpp`.
**Risk:** HIGH (format enum, StretchSurface, CreateSurfaceFrom arg order, Label lock bug). **Effort:** M.
**Verify:** Confirm no repo-wide caller of `Surface::getHandle()` inspects `->format` as a struct pointer (D-check). Visually verify text rendering, edge-fade labels, window icon, and scaled (>1) present path are pixel-correct.

### Phase 3 — Audio backend rewrite

**The re-architecture (R1).** Convert from SDL2 pull-callback to `SDL_OpenAudioDeviceStream` + `SDL_AudioStreamCallback`.

**Changes:**
- `SDL_AudioSpec desired{}` now carries only `{format, channels, freq}`; delete `samples`/`callback`/`userdata` field assignments (`audio.cpp:158-160`).
- `SDL_OpenAudioDevice(...)`→`m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, &Audio::callback, this)`. Keep the NULL-return throw. **Device opens paused** — the initial `m_playing=false` is naturally honored.
- Member change: `SDL_AudioDeviceID m_device_id`→`SDL_AudioStream* m_stream` (`audio.h:101`), rippling through open/pause/close and the `!= 0` guards.
- Callback rewrite (`audio.cpp:492-591`, `audio.h:65`): signature → `void cb(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)`. Assemble PCM into a **pre-sized app-owned scratch buffer** (no allocs/locks on the audio thread), run the existing FFT tap / volume / EQ on the scratch, then `SDL_PutAudioStreamData(stream, scratch, nbytes)`. Loop in fixed **512-frame chunks** to preserve the FFT cadence formerly given by `desired.samples=512` (guard `additional_amount <= 0`). `SDL_memset` moves to zero-filling the scratch shortfall.
- Pause/resume: `SDL_PauseAudioDevice(id, go?0:1)` → `if (go) SDL_ResumeAudioStreamDevice(m_stream); else SDL_PauseAudioStreamDevice(m_stream);` (`audio.cpp:205-215`).
- Teardown: `SDL_CloseAudioDevice`→`SDL_DestroyAudioStream(m_stream)` (`audio.cpp:115-118`); guard `if (m_stream)`.
- `SDL_InitSubSystem` bool fix (R3): `if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)` → `if (!SDL_InitSubSystem(SDL_INIT_AUDIO))` (`audio.cpp:40`). `SDL_WasInit` `& SDL_INIT_AUDIO` idiom unchanged.
- Drop the `obtained`-spec comparison block (`audio.cpp:170-194`); if `getDeviceRate()/getDeviceChannels()` must reflect true hardware, derive via `SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(m_stream), …)` (D3/D4).

**Files:** `src/audio.cpp`, `include/audio.h`.
**Risk:** HIGH. **Effort:** L.
**Verify:** Audio plays without underruns/glitches; FFT visualizer cadence unchanged; volume + EQ apply correctly; pause/resume works; init failure is now *detected* on a no-audio-backend host. Run under `--enable-tsan` to confirm the scratch buffer and FFT tap stay race-free.

### Phase 4 — Win32 SYSWM → window properties + message hook *(Windows-only)*

**The re-architecture (R2).**

**Changes:**
- **HWND via properties:** replace `SDL_GetWindowWMInfo`/`SDL_SysWMinfo`/`SDL_VERSION(&wmi.version)` in `System::getHwnd()` (`system.cpp:572-584`) and `installWin32Menu()` (`player.cpp:1636-1645`) with:
  ```c
  SDL_PropertiesID p = SDL_GetWindowProperties(win);
  HWND hwnd = (HWND)SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
  ```
  The `subsystem != SDL_SYSWM_WINDOWS` guard collapses to a NULL check.
- **Native messages via hook:** delete the `SDL_SYSWMEVENT` case (`player.cpp:3276-3299`) and the two `SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE)` calls (`player.cpp:1689,2905`). Install `SDL_SetWindowsMessageHook(cb, this)` once at startup; move `WM_COMMAND` (native menu), `THBN_CLICKED` (taskbar thumb buttons), and the `TaskbarButtonCreated` shell message dispatch into `bool cb(void*, MSG*)`.
- **Threading:** the hook runs on SDL's internal message-pump thread. Do **not** call `prevTrack()`/`playPause()`/`nextTrack()`/`handleWin32MenuCommand()` inline. Marshal to the GUI thread via `synthesizeUserEvent` (the pattern the Winamp IPC path at `system.cpp:271/283` already uses) and dispatch in the main loop (D-thread). Return `true` from the hook to let SDL continue processing.

**Files:** `src/system.cpp`, `include/system.h`, `src/player.cpp`.
**Risk:** HIGH (Windows-only, threading, message-ordering). **Effort:** M.
**Verify:** Build + run on Windows (installation-05 cross → the build-host matrix). Confirm native menu items, taskbar thumb-buttons, and taskbar-button creation all work and mutate `Player` state only from the main thread (TSan/manual review). Check `TaskbarButtonCreated` timing vs hook-install order (D-Win32).

### Phase 5 — Events & input (keysym flattening, float coords, timers, text input) + DROP_POSITION payoff

**Changes:**
- **`SDL_Init` bool fix (R3):** `if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)` → `if (!SDL_Init(SDL_INIT_VIDEO))`. **Drop `SDL_INIT_TIMER`** (removed; timers need no subsystem). `SDL_INIT_VIDEO` implies `SDL_INIT_EVENTS`.
- **`SDL_SetThreadPriority` (R3):** `system.cpp:897` → `if (!SDL_SetCurrentThreadPriority(sdl_priority))`. Enum values unchanged.
- **`SDL_WINDOWEVENT` collapse (R4):** the single `case SDL_WINDOWEVENT` (`player.cpp:3248`) becomes a range test `type >= SDL_EVENT_WINDOW_FIRST && type <= SDL_EVENT_WINDOW_LAST` (or grouped cases). `Screen::handleWindowEvent` / `Display::handleWindowEvent` (`display.cpp:164`) switch on `event.type`, not `event.window.event`. Map `SDL_WINDOWEVENT_SIZE_CHANGED`→`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`, `EXPOSED`/`RESIZED`/`SHOWN`→`SDL_EVENT_WINDOW_*`.
- **`SDL_keysym` removal (R4):** delete `using SDL_keysym = SDL_Keysym;` (`psymp3.h:271`). Re-signature `handleKeyPress`/`handleKeyUp` (`player.cpp:2502,2768`) and the widget handlers `MenuBarWidget::handleKey`, `TextInputWidget::handleFocusedKeyPress`, `EqualizerWindow::handleMenuKey` to take `const SDL_KeyboardEvent&` (or a thin app-local key struct — D-key). Inside: `keysym.sym`→`.key`, `keysym.mod`→`.mod`. `synthesizeKeyEvent` (`player.cpp:596`): `event.key.keysym.sym = kpress`→`event.key.key = kpress`. `SDLKey` alias stays valid (`SDL_Keycode` still exists).
- **Float coordinates (R6):** `event.button.x/y`, `event.motion.x/y`, `event.wheel.y` are `float`. Absorb at the pump boundary via explicit `static_cast<int>` (keeps handlers/widgets int-based — D-coord) since widgets consume precomputed int `relative_x/y`. For wheel notches use the new `event.wheel.integer_y` instead of truncating `wheel.y`.
- **Timers:** re-signature the three `SDL_TimerCallback`s (`AppLoopTimer`, `AutomatedTestTimer`, `AutomatedQuitTimer` — decl `player.h:84-86`, defn `player.cpp:630,3548,3572`) to `Uint32 cb(void* userdata, SDL_TimerID id, Uint32 interval)`; `param` moves to first arg. `SDL_AddTimer`/`SDL_RemoveTimer`/`SDL_TimerID` call sites otherwise unchanged.
- **Text input:** `SDL_StartTextInput()`→`SDL_StartTextInput(screen->getWindowHandle())` (`player.cpp:2864`, after window creation); `SDL_StopTextInput()`→`SDL_StopTextInput(window)` (`player.cpp:3470`, guard shutdown).
- **Drop-string ownership (R4/critical):** `event.drop.file`→`event.drop.data`; **delete** `SDL_free(event.drop.file)` at `player.cpp:3268` — the string is SDL-owned in SDL3, so the existing free is an invalid free/crash.
- **Bool-return traps:** `SDL_PushEvent(&event) == 1` (`player.cpp:619`) — `true` converts to `1` so it still works, but fix the stale "negative on error" comment. `SDL_WaitEvent`, `SDL_RemoveTimer`, `SDL_RaiseWindow` return-ignored → compile unchanged.

- **🎯 DROP_POSITION payoff:** add a `case SDL_EVENT_DROP_POSITION` near `player.cpp:3262`. It carries `event.drop.x` / `event.drop.y` / `windowID` while a drag hovers. Feed the hover Y to the Playlist Manager to compute the insertion index and render the **live blue insertion bar**. Requires new UI state, so it may be split into the Playlist-Manager feature work rather than the mechanical migration (D-drop) — but the enabling `case` and payload plumbing land here.

**Files:** `src/player.cpp`, `include/player.h`, `include/psymp3.h`, `src/system.cpp`, `src/core/display.cpp`, `include/display.h`, `src/widget/**` (MenuBarWidget, TextInputWidget, EqualizerWindow), and their headers.
**Risk:** HIGH (keysym re-signature, window-event dispatch, drop free). **Effort:** L.
**Verify:** Keyboard shortcuts, Alt-menu access keys, text entry/IME, mouse hit-testing at fractional logical scale, wheel scroll per-notch, drag-and-drop (no crash on drop), and window resize/expose refresh all work. `SDL_Init`/priority failures now surface in logs.

### Phase 6 — Build-green & verification

**Changes:** resolve residual compile errors, silence `-Wconversion` narrowing from the tick/coord changes, update `ARCHITECTURE.md` for the audio push-model and Win32 message-hook changes.
**Files:** tree-wide; `ARCHITECTURE.md`.
**Risk:** LOW. **Effort:** S–M.
**Verify:**
- Unity build green: `./configure --enable-final && make -j$(nproc)`.
- Sanitizers: `--enable-asan` (drop-event free, scratch buffer), `--enable-tsan` (audio callback, Win32 hook marshalling), `--enable-ubsan`.
- Tests: `./configure --enable-rapidcheck && make check`.
- Cross-platform: local amd64 (arcadia), Windows via build-win.sh (installation-05), plus the FreeBSD i386 / NetBSD / RPi ARM hosts from the build matrix.

---

## 3. Consolidated SDL2 → SDL3 Symbol Mapping

Deduplicated across all seven inventories. **Status legend:** SAME = no change · RENAMED = name changes, semantics same · SIG = signature/return changed · SEM = struct/semantics changed · REMOVED = deleted/re-architected · NEW = new SDL3 symbol we adopt.

### Init / teardown / error

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_Init` | SIG | `bool SDL_Init(SDL_InitFlags)` | `< 0` check now dead — use `!SDL_Init(...)`. **R3** |
| `SDL_InitSubSystem` | SIG | `bool SDL_InitSubSystem(...)` | Same bool inversion. `audio.cpp:40`. **R3** |
| `SDL_WasInit` | SAME | `SDL_InitFlags SDL_WasInit(...)` | `& SDL_INIT_AUDIO` idiom unchanged. |
| `SDL_Quit` / `SDL_QuitSubSystem` | SAME | same | No change. |
| `SDL_INIT_VIDEO` / `SDL_INIT_AUDIO` | SAME | same | `VIDEO`/`AUDIO` now imply `SDL_INIT_EVENTS`. |
| `SDL_INIT_TIMER` | REMOVED | *(none)* | Drop from init mask; timers need no subsystem. |
| `SDL_GetError` | SAME | same | Check the bool return, not GetError, for success. |
| `SDL_main` / entry | SEM | `SDL_MAIN_HANDLED` or SDL3 `WinMain` | Keep classic `main()`; do **not** adopt `SDL_MAIN_USE_CALLBACKS`. **D8** |

### Event enums & pump

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_KEYDOWN`/`KEYUP`/`TEXTINPUT`/`MOUSEBUTTONDOWN`/`MOUSEBUTTONUP`/`MOUSEMOTION`/`MOUSEWHEEL`/`QUIT`/`USEREVENT`/`DROPBEGIN`/`DROPFILE`/`DROPCOMPLETE` | RENAMED | `SDL_EVENT_*` (`KEY_DOWN`, `KEY_UP`, `TEXT_INPUT`, `MOUSE_BUTTON_DOWN`, …, `USER`, `DROP_BEGIN`, `DROP_FILE`, `DROP_COMPLETE`) | Values unchanged; bulk rename (Phase 1). |
| `SDL_WINDOWEVENT` | REMOVED | `SDL_EVENT_WINDOW_*` (`_FIRST`..`_LAST`) | Umbrella type gone; range-test on `event.type`. **R4** |
| `event.window.event` | REMOVED | `event.type` itself | Subtype field removed from `SDL_WindowEvent`. `SIZE_CHANGED`→`PIXEL_SIZE_CHANGED`. |
| `SDL_EVENT_DROP_POSITION` | NEW | same | Hover x/y/windowID → live drop bar. **The payoff.** |
| `SDL_WaitEvent` / `SDL_PushEvent` | SIG | bool return | `WaitEvent` loop fine; fix stale `PushEvent == 1` comment. |
| `SDL_EventState` + `SDL_ENABLE` | REMOVED | `SDL_SetEventEnabled(type, bool)` | Both call sites enabled `SDL_SYSWMEVENT` → **deleted**, not ported. |
| `SDL_Event` (union) | SEM | same type | `event.type` still discriminant; sub-members moved (below). |
| `SDL_MOUSEWHEEL_FLIPPED` / `event.wheel.direction` | SAME | same | Flip logic unchanged. |
| `SDL_BUTTON_LEFT` / `SDL_BUTTON_RIGHT` | SAME | same (1, 3) | No change. |
| `SDL_GetMouseState` / `SDL_PollEvent` / `SDL_RegisterEvents` | — | *(not used in tree)* | No migration. |

### Event struct members

| SDL2 member | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `event.drop.file` | RENAMED+SEM | `event.drop.data` | **SDL-owned** — delete the `SDL_free`. Adds x/y/windowID/source. **Critical.** |
| `event.key.keysym.sym` | REMOVED | `event.key.key` | Nested keysym dissolved. `.scancode`/`.mod` now direct. **R4** |
| `SDL_keysym` / `SDL_Keysym` | REMOVED | fields flattened into `SDL_KeyboardEvent` | Re-signature 5 handlers; delete the project alias. **R4** |
| `keysym.mod` | RENAMED | `event.key.mod` (`SDL_Keymod`) | Direct member. |
| `event.button.x/y`, `event.motion.x/y`, `event.motion.xrel/yrel`, `event.wheel.y` | SEM | now `float` | Integer scale-divides change rounding; downstream int reads narrow. Use `wheel.integer_y` for notches. **R6** |
| `SDL_MouseButtonEvent` / `SDL_MouseMotionEvent` | SEM | float x/y; adds `bool down`, `clicks`; `which`=`SDL_MouseID` | Widgets read only `.button` → safe. |
| `SDL_TextInputEvent.text` | SAME | `const char*` UTF-8 | Payload access unchanged. |
| `SDL_USEREVENT` `event.user` | SAME | same (code/data1/data2) | Layout unchanged. |

### Keymods & keycodes

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `KMOD_SHIFT`/`LCTRL`/`RCTRL`/`LALT`/`RALT` | RENAMED | `SDL_KMOD_*` | Values identical. `SDL_KMOD_CTRL` mask available. |
| `SDLK_*` (all ~40 used) | SAME | same names/values | `SDL_Keycode` now `Uint32` typedef; switch labels still legal. **No edits.** |
| `SDLKey` (project alias) | RENAMED | `SDL_Keycode` | Alias remains valid. |

### Timers & threads

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_GetTicks` | SIG | `Uint64 SDL_GetTicks(void)` | 21 sites; widen `Uint32` storage. **R6** |
| `SDL_TimerCallback` | SIG | `Uint32(void* userdata, SDL_TimerID, Uint32 interval)` | Params reordered + `SDL_TimerID` added. 3 callbacks. |
| `SDL_AddTimer` | SAME | same 3-arg | Only knock-on: callback ptr must match new typedef. |
| `SDL_RemoveTimer` | SIG | `bool` return | Ignored — no change. |
| `SDL_TimerID` | SAME | `Uint32` typedef | No change. |
| `SDL_SetThreadPriority` | RENAMED+SIG | `bool SDL_SetCurrentThreadPriority(...)` | Rename **and** fix `< 0` check. `system.cpp:897`. **R3** |
| `SDL_ThreadPriority` + `SDL_THREAD_PRIORITY_*` | SAME | same enum | No change. |
| `#include <SDL_mutex.h>` | SEM | `SDL_Mutex`/`SDL_Condition` | Unused — delete the include. |

### Window & surface

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_Surface.format` (member) | SEM | `SDL_PixelFormat` **enum** (not `*`) | Use `SDL_GetPixelFormatDetails()` / `SDL_BYTESPERPIXEL`. **R5** |
| `SDL_MapRGB` / `SDL_MapRGBA` / `SDL_GetRGBA` | SIG | add `const SDL_PixelFormatDetails*` + `SDL_Palette*` | Or `SDL_MapSurfaceRGB/RGBA`. Hoist details/palette out of loops. **R5** |
| `SDL_CreateRGBSurface` / `WithFormat` | REMOVED | `SDL_CreateSurface(w,h,fmt)` | Masks/flags/bpp dropped. |
| `SDL_CreateRGBSurfaceWithFormatFrom` | REMOVED | `SDL_CreateSurfaceFrom(w,h,fmt,pixels,pitch)` | **Arg order changed** — silent-bug risk. |
| `SDL_FreeSurface` | RENAMED | `SDL_DestroySurface` | Deleter fits unchanged. |
| `SDL_FillRect` | RENAMED | `SDL_FillSurfaceRect` | bool return, ignored. |
| `SDL_SoftStretch` | REMOVED | `SDL_StretchSurface(..., SDL_SCALEMODE_NEAREST)` | Must pass NEAREST. |
| `SDL_BlitSurface` | SIG | bool return | Args identical; ignored. |
| `SDL_CreateWindow` | SIG | `(title, w, h, flags)` | Dropped x/y; use `SDL_SetWindowPosition` to center. |
| `SDL_WINDOW_SHOWN` | REMOVED | *(default shown)* | Drop flag; use `SDL_WINDOW_HIDDEN` to defer. |
| `SDL_WINDOWPOS_CENTERED` | SAME | same macro | No longer accepted by `CreateWindow`. |
| `SDL_GetWindowSurface` / `SDL_DestroyWindow` / `SDL_GetWindowID` | SAME | same | No change. |
| `SDL_UpdateWindowSurface` / `SDL_SetWindow{Title,Icon,Size}` / `SDL_RaiseWindow` | SIG | bool return | Ignored — recompile only. |
| `SDL_LockSurface` | SIG | `bool` (true=success) | **Inverted-logic bug** at `Label.cpp:421`. **R5** |
| `SDL_UnlockSurface` / `SDL_MUSTLOCK` | SAME/SEM | same | `MUSTLOCK` now tests `SDL_SURFACE_LOCK_NEEDED`. |
| `SDL_SetSurfaceBlendMode` / `SetSurfaceAlphaMod` | SIG | bool return | Ignored. Blend-mode constants unchanged. |
| `SDL_Get/SetClipRect` | RENAMED | `SDL_Get/SetSurfaceClipRect` | bool return, ignored. |
| `SDL_LoadBMP` | SAME | same | Returned surface's `->format` is now an enum. |
| `SDL_Color` / `SDL_Rect` / `SDL_PIXELFORMAT_RGBA32` / `SDL_BYTEORDER` | SAME | same | No change. |
| `SDL_StartTextInput` / `SDL_StopTextInput` | SIG | require `SDL_Window*`, bool return | Pass the window; gates IME delivery. |

### Win32 / SYSWM

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_GetWindowWMInfo` / `SDL_SysWMinfo` | REMOVED | `SDL_GetWindowProperties` + `SDL_GetPointerProperty(SDL_PROP_WINDOW_WIN32_HWND_POINTER)` | HWND via properties. **R2** |
| `SDL_SYSWMEVENT` / `event.syswm` / `SDL_SysWMEvent` | REMOVED | `SDL_SetWindowsMessageHook(cb, ud)` | Native messages no longer queued. Hook on non-GUI thread → marshal. **R2** |
| `SDL_SYSWM_WINDOWS` | REMOVED | *(none)* | Replaced by HWND NULL check. |
| `SDL_VERSION(&struct)` | SEM | int version macro | `SDL_VERSION(&wmi.version)` is now a **compile error** — delete. |
| `#include <SDL_syswm.h>` | REMOVED | `<SDL3/SDL_system.h>` + properties | Delete include. |

### Audio

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_AudioSpec` | SEM | `{format, channels, freq}` only | `samples`/`callback`/`userdata`/`silence`/`size` removed. |
| `AUDIO_S16` | RENAMED | `SDL_AUDIO_S16` | Value changed (`0x8010`) — never hard-code. |
| `SDL_OpenAudioDevice` | SIG | `SDL_OpenAudioDeviceStream(devid, spec, cb, ud)` → `SDL_AudioStream*` | No obtained spec. Opens paused. **R1** |
| `SDL_CloseAudioDevice` | SEM | `SDL_DestroyAudioStream(stream)` | Stops + closes implicit device. |
| `SDL_PauseAudioDevice(id, flag)` | SIG | `SDL_Pause/ResumeAudioStreamDevice(stream)` | Split into two calls. |
| `SDL_AudioCallback` | SEM | `SDL_AudioStreamCallback(ud, stream, additional, total)` | **No output buffer** — push via `SDL_PutAudioStreamData`. **R1** |
| `SDL_AudioDeviceID` (member) | SEM | `SDL_AudioStream*` | Ownership/type change. |
| `SDL_PutAudioStreamData` / `SDL_GetAudioStreamDevice` / `SDL_GetAudioDeviceFormat` | NEW | — | Push PCM; query real HW format. |
| `SDL_memset` | SAME | same | Now zero-fills app scratch buffer. |

### Cursors & misc

| SDL2 symbol | Status | SDL3 equivalent | Notes |
|---|---|---|---|
| `SDL_FreeCursor` | RENAMED | `SDL_DestroyCursor` | 4 sites in WindowFrameWidget. |
| `SDL_SetCursor` | SIG | bool return | Ignored. |
| `SDL_CreateCursor` / `SDL_GetCursor` / `SDL_Cursor` / `SDL_CreateSystemCursor` | SAME | same | No change (SystemCursor only in a comment). |
| `SDL_free` | SAME | same | Function unchanged; the **drop-string free must be removed** (see `event.drop.file`). |
| Not present in tree | — | — | `SDL_RWops`/`IOStream`, `SDL_LoadFile`, clipboard, `SDL_OpenURL`, `SDL_GetPlatform`, messagebox, `SDL_ShowCursor/HideCursor`, `SDL_SetHint`, `SDL_Delay`, `SDL_Create/Wait/Thread`, `SDL_mutex/cond/atomic` — **zero migration**. High grep counts were header filenames, not code. |

---

## 4. Decisions Needed

> **RESOLVED (2026-08-17):**
> - **D6 — no `sdl2-compat` shim.** Full, all-in SDL3 port against real SDL3.
> - **D3 — audio pull-port.** `SDL_OpenAudioDeviceStream` + an `SDL_AudioStreamCallback`, matching the existing pull/callback functional model.
> - **D8 — SDL-managed entry (Option A).** `#include <SDL3/SDL_main.h>` in `main.cpp` only (SDL_main is header-only in SDL3; there is no `libSDL3main`).
> - **D-key — project-local `Keysym` (Option 3).** `struct Keysym { SDL_Keycode sym; SDL_Keymod mod; }` with `using SDL_keysym = Keysym;`, filled from the SDL3 event at the pump; keeps the five key handlers' `.sym`/`.mod` bodies intact and the key API SDL-version-agnostic.


| ID | Decision | Options / recommendation |
|---|---|---|
| **D1** | Non-text default surface format | SDL2 `CreateRGBSurface(...,32,0,0,0,0)` gave an *opaque* 32-bit surface. Choose `SDL_PIXELFORMAT_XRGB8888` (preserve opaque/no-alpha blend semantics) vs `RGBA32` (uniform with text surfaces). Affects `MapRGB` alpha and blit blending. **Rec: XRGB8888 for the opaque default, RGBA32 for text.** |
| **D2** | Window centering | `SDL_SetWindowPosition(CENTERED)` after create, `SDL_CreateWindowWithProperties`, or accept SDL3 default placement. Interacts with the Windows menu-bar `reapplyWindowSize`. **Rec: SetWindowPosition after create.** |
| **D3** | Audio callback (pull) vs push model | Keep the 1:1 pull port (`SDL_OpenAudioDeviceStream` + callback) — **recommended for first pass, lower risk** — vs push model (decoder thread calls `SDL_PutAudioStreamData`, throttled by `SDL_GetAudioStreamQueued`), which retires the SDL callback thread but relocates the FFT/volume/EQ tap and `m_samples_played` advance. |
| **D4** | `getDeviceRate()`/`getDeviceChannels()` semantics | Report true hardware format via `SDL_GetAudioDeviceFormat` (mandatory if any consumer depends on negotiated HW values) vs mirror source/stream format (if only diagnostic). Confirm consumers. |
| **D5** | Tick-width policy | Widen all tick storage to `Uint64` (clean SDL3 match) vs cast `SDL_GetTicks()` at each of 21 call sites to keep `Uint32` (49.7-day-wrap) semantics. **Rec: widen to Uint64.** |
| **D6** | `sdl2-compat` shim as interim | The shim would let current code build against SDL3 binaries unchanged, de-risking `audio.cpp` entirely for an interim step. Confirm the project links real SDL3, not the shim, before committing to the source rewrite. |
| **D7** | void→bool return policy | Keep ignoring the new bool returns (minimal diff) vs add `SDL_GetError` logging on failure per the project's exception/error-context guideline. |
| **D8** | Windows entry point | Keep SDL3-provided `WinMain` (link SDL3main) vs `SDL_MAIN_HANDLED` + own `main()`. The llvm-mingw static self-contained build may prefer `SDL_MAIN_HANDLED`; align with `build-win.sh`. |
| **D-key** | Key-handler replacement type | Pass full `SDL_KeyboardEvent` (keeps scancode/repeat for a future rebinding UI) vs a minimal `(SDL_Keycode, SDL_Keymod)` pair vs a project-local `Keysym` struct (keeps the widget API SDL-version-agnostic). Decide **one canonical type project-wide** before touching the 5 handlers. |
| **D-coord** | Float-coord absorption | Cast float→int at the pump boundary (minimal churn, keeps handlers/widgets int-based) vs propagate float through the widget tree (higher-fidelity hit-testing, cross-subsystem). **Rec: cast at the boundary** since widgets already use precomputed int `relative_x/y`. |
| **D-thread** | Win32 message-hook marshalling | Execute menu/taskbar actions inline in the hook (races on `Player` state) vs marshal via `synthesizeUserEvent` to the main loop (matches existing Winamp IPC pattern). **Rec: marshal.** |
| **D-drop** | Wire `SDL_EVENT_DROP_POSITION` now or later | Land the `case` + payload plumbing during the mechanical migration; defer the actual Playlist-Manager blue-bar UI state to the feature work. |
| **D-Win32** | Single-window / hook ordering | Confirm the one-main-window assumption (hook `userdata` carries `Player*`) and that `TaskbarButtonCreated` timing vs hook-install order is correct on real Windows. |
| **D-verify** | Surface `getHandle()` consumers | Repo-wide check that no caller inspects `surf->format` expecting the old struct pointer; confirm the Surface migration exposes `format` as the enum so `Label.cpp` fixes line up. |

---

## 5. The Payoff — Live Drop Bar for the Playlist Manager

The single feature that justified this migration is not portability parity — it is **`SDL_EVENT_DROP_POSITION`**. In SDL2 the drag lifecycle exposed only `DROPBEGIN` → `DROPFILE`(s) → `DROPCOMPLETE`, with **no hover coordinates**: there was no cross-platform way to know where inside the window a drag was before it landed. The Playlist Manager therefore could not show *where* dropped files would be inserted until after the drop.

SDL3 adds `SDL_EVENT_DROP_POSITION`, dispatched continuously as a drag hovers, carrying `event.drop.x`, `event.drop.y`, and `windowID`. Wiring a single `case` into the pump (Phase 5) lets us map the hover Y to a playlist row and render the **live blue insertion bar** as the user drags — the exact interaction that motivated moving off SDL2, now available on Linux, Windows, and the BSDs from one code path.
