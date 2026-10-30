/*
 * display.h - SDL_Surface wrapper header for display window.
 * This also wraps SDL_gfx for primitives.
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

namespace PsyMP3::Core {

Display::Display() : Surface()
{
    // SDL3: SDL_CreateWindow takes only (title, w, h, flags) — position is set
    // separately and windows are shown by default (no SDL_WINDOW_SHOWN).
    m_window = SDL_CreateWindow("PsyMP3 " PSYMP3_VERSION,
                                LOGICAL_WIDTH,
                                LOGICAL_HEIGHT,
                                0);
    if (!m_window) {
        throw SDLException("Could not create SDL window");
    }
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    /*
     * Present through an SDL_Renderer, which hands the copy to the screen
     * and the 2x zoom to the GPU. SDL falls back through its renderers down
     * to a software one, so this rarely fails; when it does, the frame goes
     * to the window surface as before. A window can have one or the other,
     * never both.
     */
    if (!createRenderer()) {
        refreshWindowSurface();
    }
    allocateLogicalSurface();
    Debug::log("display", "Display::Display() logical=", LOGICAL_WIDTH, "x", LOGICAL_HEIGHT,
               " handle=0x", std::hex, getHandle());
}

Display::~Display()
{
    // Player::Cleanup()'s SDL_Quit() destroys every window before this
    // destructor runs; destroying it again dereferences freed SDL memory
    // (same shutdown hazard as ~Audio's stream and ~WindowFrameWidget's
    // cursors). Only destroy while the video subsystem is still up.
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        if (m_underlay) {
            SDL_DestroyTexture(m_underlay);
        }
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
        }
        if (m_renderer) {
            SDL_DestroyRenderer(m_renderer);
        }
        if (m_window) {
            SDL_DestroyWindow(m_window);
        }
    }
    m_underlay = nullptr;
    m_texture = nullptr;
    m_renderer = nullptr;
    m_window = nullptr;
}

/**
 * @brief The blend mode that fades the spectrum's trail on the GPU.
 *
 * Drawn with colour (0.5/255, 0.5/255, 0.5/255, a), it leaves
 * dst * (1 - a) - 0.5/255. A GPU rounds that to the nearest level, which
 * is rounding dst * (1 - a) down: what SDL's own blit of a black surface
 * at alpha a does, so a trail fades all the way to black on either path.
 * A plain black blend rounds to nearest and leaves the last levels of a
 * trail glowing forever.
 * @return The custom blend mode.
 */
SDL_BlendMode Display::fadeBlendMode()
{
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_REV_SUBTRACT,
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
}

namespace {

/**
 * @brief Whether a renderer can draw with Display::fadeBlendMode().
 * @param renderer The renderer to ask.
 * @return true if it accepts the blend mode.
 */
bool canFade(SDL_Renderer* renderer)
{
    const bool ok = SDL_SetRenderDrawBlendMode(renderer, Display::fadeBlendMode());
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    return ok;
}

} // namespace

/**
 * @brief Creates the window's renderer and the texture each frame is uploaded to.
 *
 * SDL's own choice is kept unless it cannot do the spectrum's fade blend
 * (SDL's desktop OpenGL renderer cannot), when OpenGL ES 2, Vulkan and
 * SDL's GPU renderer are tried in turn. SDL_RENDER_DRIVER, if set, is
 * obeyed as it is. The software renderer needs no custom blend: its plain
 * blend rounds as SDL's blits do.
 * @return true if both exist; false leaves neither, for the window-surface path.
 */
bool Display::createRenderer()
{
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        Debug::log("display", "Display: no renderer (", SDL_GetError(), "), using the window surface");
        return false;
    }
    const std::string first = SDL_GetRendererName(m_renderer);
    if (!SDL_GetHint(SDL_HINT_RENDER_DRIVER) && first != SDL_SOFTWARE_RENDERER && !canFade(m_renderer)) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
        for (const char* name : {"opengles2", "vulkan", "gpu"}) {
            if (first == name) {
                continue;
            }
            SDL_Renderer* candidate = SDL_CreateRenderer(m_window, name);
            if (!candidate) {
                continue;
            }
            if (canFade(candidate)) {
                m_renderer = candidate;
                break;
            }
            SDL_DestroyRenderer(candidate);
        }
        if (!m_renderer) {
            m_renderer = SDL_CreateRenderer(m_window, first.c_str());
        }
        if (!m_renderer) {
            Debug::log("display", "Display: no renderer (", SDL_GetError(), "), using the window surface");
            return false;
        }
    }
    if (!createTexture()) {
        Debug::log("display", "Display: no frame texture (", SDL_GetError(), "), using the window surface");
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
        return false;
    }
    Debug::log("display", "Display: presenting with the ", SDL_GetRendererName(m_renderer), " renderer");
    return true;
}

/**
 * @brief (Re)creates the streaming texture that holds the logical frame.
 * @return true on success.
 */
bool Display::createTexture()
{
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
    }
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, LOGICAL_WIDTH, LOGICAL_HEIGHT);
    // A new texture holds nothing: upload the whole of the next frame.
    m_uploaded.clear();
    if (!m_texture) {
        return false;
    }
    // Nearest-neighbour keeps the 2x zoom an exact pixel double.
    SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
    /*
     * The frame is drawn over the underlay. Blending a surface onto a
     * transparent pixel leaves its colour already multiplied by its alpha,
     * so the frame is premultiplied, and an opaque pixel covers what is
     * beneath as before.
     */
    SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    return true;
}

/**
 * @brief Uploads the rows of the frame that differ from the last upload.
 *
 * Most frames change a few rows (the position, the progress bar, a toast),
 * and comparing rows on the CPU costs far less than sending all of them.
 */
void Display::uploadChangedRows()
{
    const int row_bytes = LOGICAL_WIDTH * 4;
    const size_t size = static_cast<size_t>(row_bytes) * LOGICAL_HEIGHT;
    const bool all = m_uploaded.size() != size;
    if (all) {
        m_uploaded.assign(size, 0);
    }
    const auto* pixels = static_cast<const uint8_t*>(m_handle->pixels);
    int first = -1;
    int last = -1;
    for (int y = 0; y < LOGICAL_HEIGHT; ++y) {
        const uint8_t* row = pixels + static_cast<size_t>(y) * m_handle->pitch;
        uint8_t* copy = m_uploaded.data() + static_cast<size_t>(y) * row_bytes;
        if (all || std::memcmp(row, copy, row_bytes) != 0) {
            std::memcpy(copy, row, row_bytes);
            if (first < 0) {
                first = y;
            }
            last = y;
        }
    }
    if (first >= 0) {
        const SDL_Rect rows{0, first, LOGICAL_WIDTH, last - first + 1};
        SDL_UpdateTexture(m_texture, &rows, pixels + static_cast<size_t>(first) * m_handle->pitch,
                          m_handle->pitch);
    }
}

/**
 * @brief Starts drawing into the underlay, making it if needed.
 * @param width Underlay width in logical pixels.
 * @param height Underlay height in logical pixels.
 * @param fresh Set true when the underlay is new or its contents were lost,
 *        so the caller must draw all of it.
 * @return The renderer, targeting the underlay; null without a renderer.
 */
SDL_Renderer* Display::beginUnderlay(int width, int height, bool& fresh)
{
    fresh = false;
    if (!m_renderer) {
        return nullptr;
    }
    float w = 0.0f;
    float h = 0.0f;
    if (m_underlay) {
        SDL_GetTextureSize(m_underlay, &w, &h);
    }
    if (!m_underlay || m_underlay_lost || static_cast<int>(w) != width || static_cast<int>(h) != height) {
        if (m_underlay) {
            SDL_DestroyTexture(m_underlay);
        }
        m_underlay = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_XRGB8888,
                                       SDL_TEXTUREACCESS_TARGET, width, height);
        m_underlay_lost = false;
        if (!m_underlay) {
            Debug::log("display", "Display: no underlay texture: ", SDL_GetError());
            return nullptr;
        }
        SDL_SetTextureScaleMode(m_underlay, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(m_underlay, SDL_BLENDMODE_NONE);
        fresh = true;
    }
    SDL_SetRenderTarget(m_renderer, m_underlay);
    return m_renderer;
}

/**
 * @brief Ends drawing into the underlay; the renderer targets the window again.
 */
void Display::endUnderlay()
{
    if (m_renderer) {
        SDL_SetRenderTarget(m_renderer, nullptr);
    }
}

/**
 * @brief Draws the underlay beneath this frame, at the given place.
 *
 * Called on every frame by the widget that owns the underlay while it is
 * visible; a frame without the call shows black beneath the frame.
 * @param at Where, in logical pixels.
 */
void Display::showUnderlay(const Rect& at)
{
    m_underlay_at = at;
    m_underlay_shown = true;
}

/**
 * @brief Notes that render targets lost their contents (SDL_EVENT_RENDER_TARGETS_RESET).
 */
void Display::handleRenderTargetsReset()
{
    m_underlay_lost = true;
}

/**
 * @brief Recreates the frame texture after SDL_EVENT_RENDER_DEVICE_RESET.
 * @return true if a renderer is in use (the next frame should be drawn).
 */
bool Display::handleRenderDeviceReset()
{
    // A lost device (Direct3D 9 can lose one) takes its textures with it.
    if (!m_renderer) {
        return false;
    }
    if (!createTexture()) {
        Debug::log("display", "Display: could not recreate the frame texture: ", SDL_GetError());
    }
    m_underlay_lost = true;
    return true;
}

void Display::setWindowIcon(const uint8_t* rgba, int width, int height)
{
    if (!m_window || !rgba || width <= 0 || height <= 0) {
        return;
    }
    // SDL_PIXELFORMAT_RGBA32 gives R,G,B,A byte order regardless of endianness,
    // matching the raw blob. SDL_SetWindowIcon copies the pixels, so the
    // temporary surface (and the caller's buffer) can be released afterwards.
    // SDL3: SDL_CreateSurfaceFrom(width, height, format, pixels, pitch).
    SDL_Surface* icon = SDL_CreateSurfaceFrom(
        width, height, SDL_PIXELFORMAT_RGBA32,
        const_cast<uint8_t*>(rgba), width * 4);
    if (icon) {
        SDL_SetWindowIcon(m_window, icon);
        SDL_DestroySurface(icon);
        Debug::log("display", "Display::setWindowIcon: set ", width, "x", height, " window icon");
    }
}

void Display::SetCaption(TagLib::String title, TagLib::String icon_title)
{
    (void)icon_title;
    if (!m_window) {
        return;
    }
    /*
     * The player calls this on every frame. Setting the title makes the
     * window system update the window's properties and redraw its title bar,
     * so only do it when the text has changed.
     */
    std::string caption = title.to8Bit(true);
    if (caption == m_caption) {
        return;
    }
    SDL_SetWindowTitle(m_window, caption.c_str());
    m_caption = std::move(caption);
}

void Display::Flip()
{
    if (!m_window || !m_handle) {
        return;
    }

    if (m_renderer) {
        if (!m_texture && !createTexture()) {
            return;
        }
        uploadChangedRows();
        // Black where the window is bigger than the frame, as when a tiling
        // window manager sizes it, and beneath the frame without an underlay.
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
        const float scale = static_cast<float>(m_logical_scale);
        if (m_underlay && m_underlay_shown) {
            const SDL_FRect under{m_underlay_at.x() * scale, m_underlay_at.y() * scale,
                                  m_underlay_at.width() * scale, m_underlay_at.height() * scale};
            SDL_RenderTexture(m_renderer, m_underlay, nullptr, &under);
        }
        m_underlay_shown = false;
        // Top left at the zoom factor, as the window-surface path draws it,
        // so mouse coordinates divided by the zoom stay right.
        const SDL_FRect dst{0.0f, 0.0f, LOGICAL_WIDTH * scale, LOGICAL_HEIGHT * scale};
        SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst);
        SDL_RenderPresent(m_renderer);
        return;
    }

    if (!m_window_surface) {
        return;
    }

    if (m_logical_scale == 1) {
        SDL_BlitSurface(m_handle.get(), nullptr, m_window_surface, nullptr);
    } else {
        // SDL3: SDL_SoftStretch became SDL_StretchSurface with an explicit
        // scale mode. SDL_SCALEMODE_NEAREST keeps the exact nearest-neighbor
        // integer pixel-double we want (SDL_BlitSurfaceScaled may pick a linear
        // path on some software fast paths).
        SDL_Rect dst{0, 0, LOGICAL_WIDTH * m_logical_scale, LOGICAL_HEIGHT * m_logical_scale};
        SDL_StretchSurface(m_handle.get(), nullptr, m_window_surface, &dst, SDL_SCALEMODE_NEAREST);
    }

    SDL_UpdateWindowSurface(m_window);
}

void Display::setLogicalScale(int scale)
{
    if (scale < 1) {
        scale = 1;
    }
    if (scale == m_logical_scale) {
        return;
    }

    m_logical_scale = scale;
    if (m_window) {
        SDL_SetWindowSize(m_window, LOGICAL_WIDTH * scale, LOGICAL_HEIGHT * scale);
        if (!m_renderer) {
            refreshWindowSurface();
        }
    }
}

void Display::reapplyWindowSize()
{
    if (m_window) {
        // SDL's Windows backend accounts for a present menu when it adjusts the
        // window rect, so re-setting the client size after SetMenu grows the
        // window to keep the drawable area at the logical size.
        SDL_SetWindowSize(m_window, LOGICAL_WIDTH * m_logical_scale, LOGICAL_HEIGHT * m_logical_scale);
        if (!m_renderer) {
            refreshWindowSurface();
        }
    }
}

void Display::refreshWindowSurface()
{
    if (!m_window) {
        throw SDLException("Cannot refresh window surface without an SDL window");
    }

    SDL_Surface* window_surface = SDL_GetWindowSurface(m_window);
    if (!window_surface) {
        throw SDLException("Could not get SDL window surface");
    }

    m_window_surface = window_surface;
}

void Display::allocateLogicalSurface()
{
    SDL_Surface* surf = nullptr;
    if (m_renderer) {
        /*
         * Drawn over the underlay, so it needs an alpha channel: zeroed, it
         * starts transparent, which shows the black beneath as a black
         * XRGB8888 surface did.
         */
        surf = SDL_CreateSurface(LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_PIXELFORMAT_ARGB8888);
        if (surf) {
            SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
        }
    } else if (m_window_surface) {
        // Match the window's pixel format so the stretch is zero-conversion.
        // SDL3: surface->format is the SDL_PixelFormat enum directly.
        surf = SDL_CreateSurface(LOGICAL_WIDTH, LOGICAL_HEIGHT, m_window_surface->format);
    }
    if (!surf) {
        surf = SDL_CreateSurface(LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_PIXELFORMAT_XRGB8888);
    }
    if (!surf) {
        throw SDLException("Could not create logical display surface");
    }
    m_handle.reset(surf);
}

SDL_Window* Display::getWindowHandle() const
{
    return m_window;
}

bool Display::handleWindowEvent(const SDL_WindowEvent& event)
{
    if (!m_window) {
        return false;
    }

    if (event.windowID != SDL_GetWindowID(m_window)) {
        return false;
    }

    // SDL3: window events are distinct top-level event types carried in
    // SDL_WindowEvent.type (there is no more .event sub-field).
    switch (event.type) {
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_SHOWN:
        // A renderer follows the window by itself; the window surface does not.
        if (!m_renderer) {
            refreshWindowSurface();
        }
        return true;
    default:
        return false;
    }
}

} // namespace PsyMP3::Core
