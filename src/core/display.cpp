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
    m_window = SDL_CreateWindow("PsyMP3 " PSYMP3_VERSION,
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                LOGICAL_WIDTH,
                                LOGICAL_HEIGHT,
                                SDL_WINDOW_SHOWN);
    if (!m_window) {
        throw SDLException("Could not create SDL window");
    }

    refreshWindowSurface();
    allocateLogicalSurface();
    Debug::log("display", "Display::Display() logical=", LOGICAL_WIDTH, "x", LOGICAL_HEIGHT,
               " handle=0x", std::hex, getHandle());
}

Display::~Display()
{
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Display::SetCaption(TagLib::String title, TagLib::String icon_title)
{
    (void)icon_title;
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.toCString(true));
    }
}

void Display::Flip()
{
    if (!m_window || !m_handle || !m_window_surface) {
        return;
    }

    if (m_logical_scale == 1) {
        SDL_BlitSurface(m_handle.get(), nullptr, m_window_surface, nullptr);
    } else {
        // SDL_SoftStretch is documented as nearest-neighbor — exactly what we
        // want for an integer pixel-double. SDL_BlitScaled may pick a linear
        // path on some software fast paths, so we use SoftStretch explicitly.
        SDL_Rect dst{0, 0, LOGICAL_WIDTH * m_logical_scale, LOGICAL_HEIGHT * m_logical_scale};
        SDL_SoftStretch(m_handle.get(), nullptr, m_window_surface, &dst);
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
        refreshWindowSurface();
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
    if (m_window_surface) {
        // Match the window's pixel format so the SoftStretch is zero-conversion.
        surf = SDL_CreateRGBSurfaceWithFormat(0, LOGICAL_WIDTH, LOGICAL_HEIGHT,
                                              m_window_surface->format->BitsPerPixel,
                                              m_window_surface->format->format);
    }
    if (!surf) {
        surf = SDL_CreateRGBSurface(0, LOGICAL_WIDTH, LOGICAL_HEIGHT, 32, 0, 0, 0, 0);
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

    switch (event.event) {
    case SDL_WINDOWEVENT_EXPOSED:
    case SDL_WINDOWEVENT_RESIZED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
    case SDL_WINDOWEVENT_SHOWN:
        refreshWindowSurface();
        return true;
    default:
        return false;
    }
}

} // namespace PsyMP3::Core
