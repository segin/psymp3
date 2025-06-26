/*
 * surface.h - class implementation for SDL_Surface wrapper
 * This also wraps SDL_gfx for primitives.
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

Surface::Surface()
{
    //ctor - wat do?
    m_handle = nullptr; // XXX: Figure out what to do for a default constructor, if we're to ever use it.
}

Surface::Surface(SDL_Surface *sfc)
{
    //std::cout << "Surface::Surface(SDL_Surface*): called, 0x" << std::hex << (unsigned int) sfc << std::endl;
    m_handle = sfc;
}

Surface::Surface(const Surface *rhs)
{
    m_handle = rhs->m_handle;
}

Surface::Surface(int width, int height)
{
    m_handle = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0, 0, 0, 0);
}

Surface& Surface::operator= (const Surface &rhs)
{
    if (m_handle) SDL_FreeSurface(m_handle);
    m_handle = rhs.m_handle;
    return *this;
}

Surface& Surface::operator= (const Surface *rhs)
{
    if (m_handle) SDL_FreeSurface(m_handle);
    m_handle = rhs->m_handle;
    return *this;
}

Surface::~Surface()
{
    //dtor
    if (m_handle) SDL_FreeSurface(m_handle);
}

Surface& Surface::FromBMP(std::string a_file)
{
    return *(new Surface(SDL_LoadBMP(a_file.c_str())));
}

Surface& Surface::FromBMP(const char *a_file)
{
    return *(new Surface(SDL_LoadBMP(a_file)));
}

void Surface::Blit(Surface& src, Rect& rect)
{
    if (!m_handle) return;
    SDL_Rect r = { rect.width(), rect.height() };
    SDL_BlitSurface(src.m_handle, 0, m_handle, &r);
}

bool Surface::isValid()
{
    if (m_handle) return true; else return false;
}

uint32_t Surface::MapRGB(uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_handle) return -1;
    return SDL_MapRGB(m_handle->format, r, g, b);
}

uint32_t Surface::MapRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return -1;
    return SDL_MapRGBA(m_handle->format, r, g, b, a);
}

void Surface::FillRect(uint32_t color)
{
    if (!m_handle) return;
    SDL_FillRect(m_handle, 0, color);
}

void Surface::Flip()
{
    if (!m_handle) return;
    SDL_Flip(m_handle);
}

void Surface::put_pixel_unlocked(int16_t x, int16_t y, uint32_t color)
{
    // No bounds checking here, expecting caller to handle it.
    // Assumes 32bpp surface, which is what the graph uses.
    uint32_t *target_pixel = (uint32_t *)((uint8_t *)m_handle->pixels + y * m_handle->pitch + x * sizeof(uint32_t));
    *target_pixel = color;
}

void Surface::hline_unlocked(int16_t x1, int16_t x2, int16_t y, uint32_t color)
{
    // No surface lock/unlock, expecting caller to handle it.
    if (y < 0 || y >= m_handle->h) return;
    if (x1 > x2) std::swap(x1, x2);
    if (x1 >= m_handle->w || x2 < 0) return;

    x1 = std::max<int16_t>(x1, 0);
    x2 = std::min<int16_t>(x2, m_handle->w - 1);

    uint32_t *row = (uint32_t *)((uint8_t *)m_handle->pixels + y * m_handle->pitch);
    for (int16_t x = x1; x <= x2; ++x) {
        row[x] = color;
    }
}

void Surface::vline_unlocked(int16_t x, int16_t y1, int16_t y2, uint32_t color)
{
    // No surface lock/unlock, expecting caller to handle it.
    if (x < 0 || x >= m_handle->w) return;
    if (y1 > y2) std::swap(y1, y2);
    if (y1 >= m_handle->h || y2 < 0) return;

    y1 = std::max<int16_t>(y1, 0);
    y2 = std::min<int16_t>(y2, m_handle->h - 1);

    uint8_t *pixel_addr = (uint8_t *)m_handle->pixels + y1 * m_handle->pitch + x * sizeof(uint32_t);
    for (int16_t y = y1; y <= y2; ++y) {
        *(uint32_t *)pixel_addr = color;
        pixel_addr += m_handle->pitch;
    }
}

void Surface::pixel(int16_t x, int16_t y, uint32_t color)
{
    if (!m_handle) return;
    if (x < 0 || x >= m_handle->w || y < 0 || y >= m_handle->h) return;
    if (SDL_MUSTLOCK(m_handle)) SDL_LockSurface(m_handle);
    put_pixel_unlocked(x, y, color);
    if (SDL_MUSTLOCK(m_handle)) SDL_UnlockSurface(m_handle);
}

void Surface::pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    pixel(x, y, MapRGBA(r, g, b, a));
}

void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    // This is implemented as a filled rectangle (box) to match its usage in the spectrum analyzer.
    if (SDL_MUSTLOCK(m_handle)) SDL_LockSurface(m_handle);
    if (y1 > y2) std::swap(y1, y2);
    for (int16_t y = y1; y <= y2; ++y) {
        hline_unlocked(x1, x2, y, color);
    }
    if (SDL_MUSTLOCK(m_handle)) SDL_UnlockSurface(m_handle);
}

void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    rectangle(x1, y1, x2, y2, MapRGBA(r, g, b, a));
}

void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    // A box is a filled rectangle.
    rectangle(x1, y1, x2, y2, color);
}

void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    box(x1, y1, x2, y2, MapRGBA(r, g, b, a));
}

void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint32_t color)
{
    if (!m_handle) return;
    if (SDL_MUSTLOCK(m_handle)) SDL_LockSurface(m_handle);
    hline_unlocked(x1, x2, y, color);
    if (SDL_MUSTLOCK(m_handle)) SDL_UnlockSurface(m_handle);
}

void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    hline(x1, x2, y, MapRGBA(r, g, b, a));
}

void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    if (SDL_MUSTLOCK(m_handle)) SDL_LockSurface(m_handle);
    vline_unlocked(x, y1, y2, color);
    if (SDL_MUSTLOCK(m_handle)) SDL_UnlockSurface(m_handle);
}

void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    vline(x, y1, y2, MapRGBA(r, g, b, a));
}

int16_t Surface::height()
{
    if (!m_handle) return 0;
    return m_handle->h;
}

int16_t Surface::width()
{
    if (!m_handle) return 0;
    return m_handle->w;
}

SDL_Surface * Surface::getHandle()
{
    return m_handle;
}
