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

/**
 * @brief Constructs the main Display object.
 * 
 * This constructor initializes the SDL video mode with a fixed size of 640x400
 * and 16-bit color depth, using a hardware surface with double buffering.
 * It then sets the initial window caption. The resulting SDL_Surface for the
 * screen is passed to the base `Surface` class constructor.
 */
Display::Display() : Surface()
{
    m_window = SDL_CreateWindow("PsyMP3 " PSYMP3_VERSION,
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                640,
                                400,
                                SDL_WINDOW_SHOWN);
    if (!m_window) {
        throw SDLException("Could not create SDL window");
    }

    refreshWindowSurface();
    Debug::log("display", "Display::Display() got 0x", std::hex, getHandle());
}

Display::~Display()
{
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

/**
 * @brief Sets the main window's title bar text.
 * @param title The main text to display in the window's title bar.
 * @param icon_title The text to use for the window when it is iconified (minimized).
 */
void Display::SetCaption(TagLib::String title, TagLib::String icon_title)
{
    (void)icon_title;
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.toCString(false));
    }
}

void Display::Flip()
{
    if (!m_window) {
        return;
    }

    SDL_UpdateWindowSurface(m_window);
}

void Display::attachWindowSurface(SDL_Surface* window_surface)
{
    wrapNonOwnedSurface(window_surface);
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

    attachWindowSurface(window_surface);
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
