# SDL2 Port Plan

## Plan A: SDL2 window-surface port

- [x] Switch dependency detection from SDL 1.2 to SDL2 in [configure.ac](/home/segin/psymp3/configure.ac).
- [x] Update umbrella/header include handling in [include/psymp3.h](/home/segin/psymp3/include/psymp3.h).
- [x] Remove SDL 1.2-specific `sdl-config` assumptions from the build.
- [x] Create a seam so [Display](/home/segin/psymp3/include/display.h) can attach to a non-owned presentation surface instead of inheriting identity directly from `SDL_SetVideoMode`.
- [x] Make [Display](/home/segin/psymp3/include/display.h) own an SDL2 window object.
- [x] Wrap the active SDL2 window surface as the `Surface` identity used by the software renderer.
- [x] Add a refresh path so the wrapped window surface can be replaced after resize or expose events.
- [x] Replace `SDL_Flip()` usage with display-specific present logic.
- [x] Use `SDL_UpdateWindowSurface()` for the initial SDL2 present path.
- [x] Convert [Player](/home/segin/psymp3/include/player.h) event handling from SDL 1.2 assumptions to SDL2 event flow.
- [x] Preserve current widget dispatch semantics while updating key, mouse, timer, and quit handling.
- [x] Replace `SDL_EnableUNICODE(1)` and `SDL_keysym.unicode` assumptions with SDL2 text-input events.
- [x] Update [TextInputWidget](/home/segin/psymp3/include/widget/ui/TextInputWidget.h) and player dispatch for SDL2 text input.
- [x] Replace legacy WM info access in [src/system.cpp](/home/segin/psymp3/src/system.cpp) with SDL2 window-manager info retrieval.
- [x] Thread the actual display window through the Windows taskbar integration path.
- [x] Move [src/audio.cpp](/home/segin/psymp3/src/audio.cpp) from SDL 1.x global audio calls to SDL2 device-based audio APIs.
- [x] Preserve the existing callback-driven decode/buffer model during the audio migration.
- [x] Replace SDL 1.2 alpha calls with SDL2 surface alpha/blend equivalents.
- [x] Audit clipping, locking, and blit behavior for SDL2 surface semantics.
- [x] Rebuild the app and widget tests with SDL2.
- [ ] Run smoke tests for window creation, playback start, text input, resize cursors, and overlay rendering.

## Plan B: SDL_Renderer as presentation backend

- [ ] Extend [Display](/home/segin/psymp3/include/display.h) to own `SDL_Renderer` and a presentation texture on top of the completed Plan A window path.
- [ ] Keep software composition from Plan A unchanged while adding renderer-backed presentation.
- [ ] Upload the fully composed frame into a texture each frame.
- [ ] Replace window-surface present with `SDL_RenderCopy` and `SDL_RenderPresent`.
- [ ] Standardize the frame surface format used by [Surface](/home/segin/psymp3/include/surface.h) for efficient texture upload.
- [ ] Avoid per-frame conversion where possible.
- [ ] Measure renderer present cost versus window-surface present.
- [ ] Validate alpha-heavy widgets, lyrics, toast, and spectrum rendering visually on the renderer path.
- [ ] Add targeted caching for stable UI surfaces if upload cost becomes a bottleneck.
- [ ] Keep widget rendering software-based for this phase; do not rewrite widget drawing around renderer primitives yet.

## Plan C: renderer-native UI and drawing

- [ ] Redesign widget drawing APIs so widgets emit renderer-native draw operations instead of mutating software surfaces.
- [ ] Retire or narrow [Surface](/home/segin/psymp3/include/surface.h) to offscreen/image use only.
- [ ] Move presentation-oriented drawing to renderer-friendly primitives.
- [ ] Replace surface-returning font rendering with texture or cached-glyph rendering.
- [ ] Introduce renderer-native text layout and caching.
- [ ] Replace software pixel, box, rounded-rect, and polygon operations with renderer-native equivalents.
- [ ] Revalidate clipping and alpha behavior across the widget stack under renderer-native drawing.
- [ ] Convert overlays, spectrum effects, fades, and window chrome from software blits to renderer composition.
- [ ] Remove the full-frame software composition dependency carried through Plans A and B.
- [ ] Remove remaining compatibility paths that only exist to preserve the old software pipeline.
- [ ] Update architecture and test coverage for the renderer-native model.
