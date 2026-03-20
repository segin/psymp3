/*
 * surface.h - class implementation for SDL_Surface wrapper
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

/**
 * @brief Default constructor — creates a Surface with a null handle.
 */
Surface::Surface() : m_handle(nullptr, SDL_FreeSurface)
{
    // Default constructor creates a null handle.
}

/**
 * @brief Wraps an existing, non-owned SDL_Surface pointer.
 *
 * A no-op deleter is used so that `SDL_FreeSurface` is never called on the
 * wrapped pointer; the caller retains ownership.
 *
 * @param non_owned_sfc Pointer to an existing SDL_Surface that must outlive this object.
 */
Surface::Surface(SDL_Surface *non_owned_sfc) : m_handle(non_owned_sfc, [](SDL_Surface*){ /* do nothing */ })
{
    // This constructor wraps a pointer but does not take ownership.
    // The custom empty deleter ensures SDL_FreeSurface is not called on it.
}

/**
 * @brief Creates a new 32-bit RGBA software surface of the given dimensions.
 *
 * Throws `SDLException` if SDL cannot allocate the surface.
 *
 * @param width  Width in pixels.
 * @param height Height in pixels.
 */
Surface::Surface(int width, int height)
    : m_handle(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, width, height, 32, 0, 0, 0, 0), SDL_FreeSurface)
{
    if (!m_handle) {
        throw SDLException("Could not create RGB surface");
    }
}

/**
 * @brief Creates a new 32-bit RGBA software surface optimised for text rendering.
 *
 * When `for_text` is `true`, endian-correct RGBA channel masks are used so
 * that FreeType alpha blending produces correct results on all platforms.
 *
 * @param width    Width in pixels.
 * @param height   Height in pixels.
 * @param for_text If `true`, use endian-aware RGBA masks; otherwise use SDL defaults.
 */
Surface::Surface(int width, int height, bool for_text)
    : m_handle(nullptr, SDL_FreeSurface)
{
    if (for_text) {
        // Use proper RGBA masks based on endianness for text surfaces
        Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000;
        gmask = 0x00ff0000;
        bmask = 0x0000ff00;
        amask = 0x000000ff;
#else
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
#endif
        
        m_handle.reset(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, width, height, 32, 
                                            rmask, gmask, bmask, amask));
    } else {
        m_handle.reset(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, width, height, 32, 0, 0, 0, 0));
    }
    
    if (!m_handle) {
        throw SDLException("Could not create RGB surface");
    }
}

/**
 * @brief Loads a BMP image from a `std::string` path into a new Surface.
 * @param a_file Path to the BMP file.
 * @return A owning `unique_ptr` to the new Surface.
 */
std::unique_ptr<Surface> Surface::FromBMP(std::string a_file)
{
    return FromBMP(a_file.c_str());
}

/**
 * @brief Loads a BMP image from a C-string path into a new Surface.
 *
 * Throws `SDLException` if SDL cannot load the file.
 *
 * @param a_file Null-terminated path to the BMP file.
 * @return An owning `unique_ptr` to the new Surface.
 */
std::unique_ptr<Surface> Surface::FromBMP(const char *a_file)
{
    auto sfc_handle = SDL_LoadBMP(a_file);
    if (!sfc_handle) {
        throw SDLException("Could not load BMP");
    }
    return std::make_unique<Surface>(sfc_handle);
}

/**
 * @brief Sets the per-surface alpha (transparency) value.
 * @param flags SDL alpha flags (e.g., `SDL_SRCALPHA`).
 * @param alpha 0 = fully transparent, 255 = fully opaque.
 */
void Surface::SetAlpha(uint32_t flags, uint8_t alpha)
{
    if (!m_handle) return;
    SDL_SetAlpha(m_handle.get(), flags, alpha);
}

/**
 * @brief Blits `src` onto this surface at the position given by `rect`.
 * @param src  Source Surface to blit from.
 * @param rect Destination position and (ignored) size  on this surface.
 */
void Surface::Blit(Surface& src, const Rect& rect) // Changed to const Rect&
{
    if (!m_handle) return;
    SDL_Rect r = { rect.x(), rect.y(), rect.width(), rect.height() };
    SDL_BlitSurface(src.getHandle(), nullptr, m_handle.get(), &r);
}

/**
 * @brief Checks whether the underlying SDL surface handle is non-null.
 * @return `true` if the surface is valid, `false` if the handle is null.
 */
bool Surface::isValid()
{
    if (m_handle) return true; else return false;
}

/**
 * @brief Maps RGB component values to a packed pixel colour for this surface's format.
 * @param r Red component (0–255).
 * @param g Green component (0–255).
 * @param b Blue component (0–255).
 * @return Packed pixel colour, or `(uint32_t)-1` if the handle is null.
 */
uint32_t Surface::MapRGB(uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_handle) return -1;
    return SDL_MapRGB(m_handle->format, r, g, b);
}

/**
 * @brief Maps RGBA component values to a packed pixel colour for this surface's format.
 * @param r Red component (0–255).
 * @param g Green component (0–255).
 * @param b Blue component (0–255).
 * @param a Alpha component (0–255, 0 = transparent).
 * @return Packed pixel colour, or `(uint32_t)-1` if the handle is null.
 */
uint32_t Surface::MapRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return -1;
    return SDL_MapRGBA(m_handle->format, r, g, b, a);
}

/**
 * @brief Fills the entire surface with a packed pixel colour.
 * @param color The packed pixel colour produced by `MapRGB` or `MapRGBA`.
 */
void Surface::FillRect(uint32_t color)
{
    if (!m_handle) return;
    SDL_FillRect(m_handle.get(), 0, color);
}

/**
 * @brief Flips the display surface to the screen (double-buffer swap).
 *
 * Calls `SDL_Flip`. Only meaningful for the primary display surface.
 */
void Surface::Flip()
{
    if (!m_handle) return;
    SDL_Flip(m_handle.get());
}

/**
 * @brief Writes a single pixel at (x, y) without locking the surface.
 *
 * The surface @e must already be locked by the caller. No bounds checking.
 *
 * @param x     Pixel x-coordinate.
 * @param y     Pixel y-coordinate.
 * @param color Packed pixel colour appropriate for the surface's format.
 */
void Surface::put_pixel_unlocked(int16_t x, int16_t y, uint32_t color)
{
    // No bounds checking here, expecting caller to handle it.
    int bpp = m_handle->format->BytesPerPixel;
    uint8_t *p = (uint8_t *)m_handle->pixels + y * m_handle->pitch + x * bpp;

    switch (bpp) {
    case 1:
        *p = color;
        break;

    case 2:
        *(uint16_t *)p = color;
        break;

    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (color >> 16) & 0xff;
            p[1] = (color >> 8) & 0xff;
            p[2] = color & 0xff;
        } else {
            p[0] = color & 0xff;
            p[1] = (color >> 8) & 0xff;
            p[2] = (color >> 16) & 0xff;
        }
        break;

    case 4:
        *(uint32_t *)p = color;
        break;
    }
}

/**
 * @brief Draws a single pixel at (x, y) with automatic surface locking.
 *
 * Silently does nothing if the handle is null or coordinates are out of bounds.
 *
 * @param x     Pixel x-coordinate.
 * @param y     Pixel y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::pixel(int16_t x, int16_t y, uint32_t color)
{
    if (!m_handle || x < 0 || x >= m_handle->w || y < 0 || y >= m_handle->h) {
        return;
    }
    SDLLockGuard lock_guard(m_handle.get());
    put_pixel_unlocked(x, y, color);
}

/**
 * @brief Draws a single RGBA-coloured pixel at (x, y) with automatic surface locking.
 * @param x Pixel x-coordinate.
 * @param y Pixel y-coordinate.
 * @param r Red component (0–255).
 * @param g Green component (0–255).
 * @param b Blue component (0–255).
 * @param a Alpha component (0–255).
 */
void Surface::pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    // This overload maps the RGBA components to a single color value
    // and then calls the primary pixel function.
    pixel(x, y, MapRGBA(r, g, b, a));
}

/**
 * @brief Draws a horizontal line without locking the surface.
 *
 * Surface @e must already be locked by the caller. Clips to surface bounds.
 *
 * @param x1    Left x-coordinate (inclusive).
 * @param x2    Right x-coordinate (inclusive).
 * @param y     Vertical position.
 * @param color Packed pixel colour.
 */
void Surface::hline_unlocked(int16_t x1, int16_t x2, int16_t y, uint32_t color)
{
    // No surface lock/unlock, expecting caller to handle it.
    if (y < 0 || y >= m_handle->h) return;
    if (x1 > x2) std::swap(x1, x2);
    if (x1 >= m_handle->w || x2 < 0) return;

    x1 = std::max<int16_t>(x1, 0);
    x2 = std::min<int16_t>(x2, m_handle->w - 1);

    int bpp = m_handle->format->BytesPerPixel;
    uint8_t *row_start = (uint8_t *)m_handle->pixels + y * m_handle->pitch + x1 * bpp;

    // Optimize for common cases
    if (bpp == 2) {
        auto *p = (uint16_t*)row_start;
        for (int16_t x = x1; x <= x2; ++x) {
            *p++ = color;
        }
    } else if (bpp == 4) {
        auto *p = (uint32_t*)row_start;
        for (int16_t x = x1; x <= x2; ++x) {
            *p++ = color;
        }
    } else { // Generic fallback for 1 and 3 bpp
        for (int16_t x = x1; x <= x2; ++x) {
            put_pixel_unlocked(x, y, color);
        }
    }
}

/**
 * @brief Draws a vertical line without locking the surface.
 *
 * Surface @e must already be locked by the caller. Clips to surface bounds.
 *
 * @param x     Horizontal position.
 * @param y1    Top y-coordinate (inclusive).
 * @param y2    Bottom y-coordinate (inclusive).
 * @param color Packed pixel colour.
 */
void Surface::vline_unlocked(int16_t x, int16_t y1, int16_t y2, uint32_t color)
{
    // No surface lock/unlock, expecting caller to handle it.
    if (x < 0 || x >= m_handle->w) return;
    if (y1 > y2) std::swap(y1, y2);
    if (y1 >= m_handle->h || y2 < 0) return;

    y1 = std::max<int16_t>(y1, 0);
    y2 = std::min<int16_t>(y2, m_handle->h - 1);

    int bpp = m_handle->format->BytesPerPixel;
    uint8_t *p = (uint8_t *)m_handle->pixels + y1 * m_handle->pitch + x * bpp;

    for (int16_t y = y1; y <= y2; ++y) {
        // This is a bit slow due to the switch, but correct for all bpp.
        // An outer switch would be faster but more verbose.
        if (bpp == 2) *(uint16_t*)p = color;
        else if (bpp == 4) *(uint32_t*)p = color;
        else put_pixel_unlocked(x, y, color); // Fallback for other bpp
        p += m_handle->pitch;
    }
}

/**
 * @brief Draws a horizontal line with automatic surface locking.
 * @param x1    Left x-coordinate.
 * @param x2    Right x-coordinate.
 * @param y     Vertical position.
 * @param color Packed pixel colour.
 */
void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint32_t color)
{
    if (!m_handle) return;
    SDLLockGuard lock_guard(m_handle.get());
    hline_unlocked(x1, x2, y, color);
}

/**
 * @brief Draws an RGBA-coloured horizontal line with automatic surface locking.
 * @param x1 Left x-coordinate.
 * @param x2 Right x-coordinate.
 * @param y  Vertical position.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    hline(x1, x2, y, MapRGBA(r, g, b, a));
}

/**
 * @brief Draws a rectangle outline without locking the surface.
 *
 * Surface @e must already be locked by the caller.
 *
 * @param x1    Left x-coordinate.
 * @param y1    Top y-coordinate.
 * @param x2    Right x-coordinate.
 * @param y2    Bottom y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::rectangle_unlocked(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    // Draw rectangle outline using unlocked line methods
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    
    // Draw the four sides of the rectangle
    hline_unlocked(x1, x2, y1, color);     // Top
    hline_unlocked(x1, x2, y2, color);     // Bottom
    vline_unlocked(x1, y1, y2, color);     // Left
    vline_unlocked(x2, y1, y2, color);     // Right
}

/**
 * @brief Draws a rectangle outline with automatic surface locking.
 * @param x1 Left x-coordinate.
 * @param y1 Top y-coordinate.
 * @param x2 Right x-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    SDLLockGuard lock_guard(m_handle.get());
    rectangle_unlocked(x1, y1, x2, y2, color);
}

/**
 * @brief Draws an RGBA-coloured rectangle outline with automatic surface locking.
 * @param x1 Left x-coordinate.
 * @param y1 Top y-coordinate.
 * @param x2 Right x-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    rectangle(x1, y1, x2, y2, MapRGBA(r, g, b, a));
}

/**
 * @brief Fills an axis-aligned rectangle without locking the surface.
 *
 * Surface @e must already be locked by the caller.
 *
 * @param x1    Left x-coordinate.
 * @param y1    Top y-coordinate.
 * @param x2    Right x-coordinate.
 * @param y2    Bottom y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::box_unlocked(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    // Fill rectangle using horizontal lines - assumes surface is already locked
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    
    for (int16_t y = y1; y <= y2; y++) {
        hline_unlocked(x1, x2, y, color);
    }
}

/**
 * @brief Fills an axis-aligned rectangle with automatic surface locking.
 *
 * Uses `SDL_FillRect` for surfaces that do not require locking, falling back
 * to the scanline method for surfaces that do.
 *
 * @param x1    Left x-coordinate.
 * @param y1    Top y-coordinate.
 * @param x2    Right x-coordinate.
 * @param y2    Bottom y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    // Use SDL_FillRect for efficiency when we can avoid locking overhead
    if (SDL_MUSTLOCK(m_handle.get())) {
        SDL_LockSurface(m_handle.get());
        box_unlocked(x1, y1, x2, y2, color);
        SDL_UnlockSurface(m_handle.get());
    } else {
        // For surfaces that don't need locking, use SDL_FillRect directly
        if (x1 > x2) std::swap(x1, x2);
        if (y1 > y2) std::swap(y1, y2);
        SDL_Rect rect = { x1, y1, static_cast<Uint16>(x2 - x1 + 1), static_cast<Uint16>(y2 - y1 + 1) };
        SDL_FillRect(m_handle.get(), &rect, color);
    }
}

/**
 * @brief Fills an RGBA-coloured rectangle with automatic surface locking.
 * @param x1 Left x-coordinate.
 * @param y1 Top y-coordinate.
 * @param x2 Right x-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    box(x1, y1, x2, y2, MapRGBA(r, g, b, a));
}

/**
 * @brief Draws a vertical line with automatic surface locking.
 * @param x  Horizontal position.
 * @param y1 Top y-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    SDLLockGuard lock_guard(m_handle.get());
    vline_unlocked(x, y1, y2, color);
}

/**
 * @brief Draws an RGBA-coloured vertical line with automatic surface locking.
 * @param x  Horizontal position.
 * @param y1 Top y-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    vline(x, y1, y2, MapRGBA(r, g, b, a));
}

/**
 * @brief Draws a line using Bresenham's algorithm without locking the surface.
 *
 * Surface @e must already be locked by the caller.
 *
 * @param x1 Start x-coordinate.
 * @param y1 Start y-coordinate.
 * @param x2 End x-coordinate.
 * @param y2 End y-coordinate.
 * @param color Packed pixel colour.
 */
void Surface::line_unlocked(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, uint32_t color)
{
    // Bresenham's line algorithm - assumes surface is already locked
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < m_handle->w && y1 >= 0 && y1 < m_handle->h) {
            put_pixel_unlocked(x1, y1, color);
        }

        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief Draws an RGBA-coloured line with automatic surface locking.
 * @param x1 Start x-coordinate.
 * @param y1 Start y-coordinate.
 * @param x2 End x-coordinate.
 * @param y2 End y-coordinate.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::line(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle) return;
    uint32_t color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    line_unlocked(x1, y1, x2, y2, color);
}

/**
 * @brief Draws a filled convex polygon using a scanline algorithm without locking the surface.
 *
 * Surface @e must already be locked by the caller.
 *
 * @param vx    Array of x-coordinates for each vertex.
 * @param vy    Array of y-coordinates for each vertex.
 * @param n     Number of vertices (must be ≥ 3).
 * @param color Packed pixel colour.
 */
void Surface::filledPolygon_unlocked(const Sint16* vx, const Sint16* vy, int n, uint32_t color)
{
    if (n < 3) return;
    
    // Find bounding box
    Sint16 min_y = vy[0], max_y = vy[0];
    for (int i = 1; i < n; i++) {
        if (vy[i] < min_y) min_y = vy[i];
        if (vy[i] > max_y) max_y = vy[i];
    }
    
    // Scanline fill algorithm - assumes surface is already locked
    for (Sint16 y = min_y; y <= max_y; y++) {
        std::vector<Sint16> intersections;
        
        // Find intersections with all edges
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            
            if (vy[i] == vy[j]) continue; // Skip horizontal edges
            
            // Check if scanline intersects this edge
            if ((vy[i] <= y && y < vy[j]) || (vy[j] <= y && y < vy[i])) {
                // Calculate intersection x-coordinate
                Sint16 x = vx[i] + (vx[j] - vx[i]) * (y - vy[i]) / (vy[j] - vy[i]);
                intersections.push_back(x);
            }
        }
        
        // Sort intersections
        std::sort(intersections.begin(), intersections.end());
        
        // Fill between pairs of intersections
        for (size_t i = 0; i < intersections.size(); i += 2) {
            if (i + 1 < intersections.size()) {
                hline_unlocked(intersections[i], intersections[i + 1], y, color);
            }
        }
    }
}

/**
 * @brief Draws a filled convex polygon with automatic surface locking.
 * @param vx Array of x-coordinates for each vertex.
 * @param vy Array of y-coordinates for each vertex.
 * @param n  Number of vertices (must be ≥ 3).
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::filledPolygon(const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle || n < 3) return;
    
    uint32_t color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    filledPolygon_unlocked(vx, vy, n, color);
}

/**
 * @brief Draws a filled triangle without locking the surface.
 *
 * Delegates to `filledPolygon_unlocked`. Surface @e must already be locked.
 *
 * @param x1 x-coordinate of vertex 1.
 * @param y1 y-coordinate of vertex 1.
 * @param x2 x-coordinate of vertex 2.
 * @param y2 y-coordinate of vertex 2.
 * @param x3 x-coordinate of vertex 3.
 * @param y3 y-coordinate of vertex 3.
 * @param color Packed pixel colour.
 */
void Surface::filledTriangle_unlocked(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, uint32_t color)
{
    // Use the general polygon algorithm for triangles - assumes surface is already locked
    Sint16 vx[3] = {x1, x2, x3};
    Sint16 vy[3] = {y1, y2, y3};
    filledPolygon_unlocked(vx, vy, 3, color);
}

/**
 * @brief Draws a filled triangle with automatic surface locking.
 * @param x1 x-coordinate of vertex 1.
 * @param y1 y-coordinate of vertex 1.
 * @param x2 x-coordinate of vertex 2.
 * @param y2 y-coordinate of vertex 2.
 * @param x3 x-coordinate of vertex 3.
 * @param y3 y-coordinate of vertex 3.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::filledTriangle(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle) return;
    uint32_t color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    filledTriangle_unlocked(x1, y1, x2, y2, x3, y3, color);
}

/**
 * @brief Draws a filled circle without locking the surface.
 *
 * Uses integer scanline arithmetic. Surface @e must already be locked.
 *
 * @param x     Centre x-coordinate.
 * @param y     Centre y-coordinate.
 * @param rad   Radius in pixels.
 * @param color Packed pixel colour.
 */
void Surface::filledCircleRGBA_unlocked(Sint16 x, Sint16 y, Sint16 rad, uint32_t color)
{
    if (rad <= 0) return;

    // Assumes surface is already locked
    for (Sint16 dy = -rad; dy <= rad; dy++) {
        // Using integer arithmetic for the circle equation to avoid floats
        Sint16 dx = static_cast<Sint16>(sqrt(rad * rad - dy * dy));
        hline_unlocked(x - dx, x + dx, y + dy, color);
    }
}

/**
 * @brief Draws a filled RGBA-coloured circle with automatic surface locking.
 * @param x Centre x-coordinate.
 * @param y Centre y-coordinate.
 * @param rad Radius in pixels.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::filledCircleRGBA(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle) return;
    if (rad <= 0) return;

    uint32_t color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    filledCircleRGBA_unlocked(x, y, rad, color);
}

/**
 * @brief Draws a filled rounded rectangle without locking the surface.
 *
 * Composed of several box_unlocked and filledCircleRGBA_unlocked calls.
 * Surface @e must already be locked.
 *
 * @param x1  Left x-coordinate.
 * @param y1  Top y-coordinate.
 * @param x2  Right x-coordinate.
 * @param y2  Bottom y-coordinate.
 * @param rad Corner radius in pixels.
 * @param color Packed pixel colour.
 */
void Surface::roundedBoxRGBA_unlocked(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, uint32_t color)
{
    if (rad < 0) return;
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Make sure radius is not too large for the box dimensions
    if (rad > (x2 - x1) / 2) rad = (x2 - x1) / 2;
    if (rad > (y2 - y1) / 2) rad = (y2 - y1) / 2;

    if (rad == 0) {
        box_unlocked(x1, y1, x2, y2, color);
        return;
    }

    // Draw non-overlapping rectangles that form the main body - assumes surface is already locked
    // 1. Central horizontal rectangle
    box_unlocked(x1 + rad, y1 + rad, x2 - rad, y2 - rad, color);
    
    // 2. Top rectangle (excluding corners)
    box_unlocked(x1 + rad, y1, x2 - rad, y1 + rad - 1, color);
    
    // 3. Bottom rectangle (excluding corners)
    box_unlocked(x1 + rad, y2 - rad + 1, x2 - rad, y2, color);
    
    // 4. Left rectangle (excluding corners)
    box_unlocked(x1, y1 + rad, x1 + rad - 1, y2 - rad, color);
    
    // 5. Right rectangle (excluding corners)
    box_unlocked(x2 - rad + 1, y1 + rad, x2, y2 - rad, color);

    // Draw the four corner circles
    filledCircleRGBA_unlocked(x1 + rad, y1 + rad, rad, color);
    filledCircleRGBA_unlocked(x2 - rad, y1 + rad, rad, color);
    filledCircleRGBA_unlocked(x1 + rad, y2 - rad, rad, color);
    filledCircleRGBA_unlocked(x2 - rad, y2 - rad, rad, color);
}

/**
 * @brief Draws a filled RGBA-coloured rounded rectangle with automatic surface locking.
 * @param x1 Left x-coordinate.
 * @param y1 Top y-coordinate.
 * @param x2 Right x-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param rad Corner radius.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::roundedBoxRGBA(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle || rad < 0) return;
    uint32_t color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    roundedBoxRGBA_unlocked(x1, y1, x2, y2, rad, color);
}

/**
 * @brief Draws a filled rounded rectangle using a packed colour with automatic surface locking.
 * @param x1 Left x-coordinate.
 * @param y1 Top y-coordinate.
 * @param x2 Right x-coordinate.
 * @param y2 Bottom y-coordinate.
 * @param rad Corner radius.
 * @param color Packed pixel colour.
 */
void Surface::roundedBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, uint32_t color)
{
    if (!m_handle || rad < 0) return;
    SDLLockGuard lock_guard(m_handle.get());
    roundedBoxRGBA_unlocked(x1, y1, x2, y2, rad, color);
}
    
/**
 * @brief Returns the surface height in pixels.
 * @return Height in pixels, or 0 if the handle is null.
 */
int16_t Surface::height()
{
    if (!m_handle) return 0;
    return m_handle->h;
}

/**
 * @brief Returns the surface width in pixels.
 * @return Width in pixels, or 0 if the handle is null.
 */
int16_t Surface::width()
{
    if (!m_handle) return 0;
    return m_handle->w;
}

/**
 * @brief Returns the raw `SDL_Surface*` handle.
 * @return Pointer to the underlying SDL surface, or `nullptr` if not valid.
 */
SDL_Surface * Surface::getHandle()
{
    return m_handle.get();
}

/**
 * @brief Reads a pixel colour at (x, y) without locking the surface.
 *
 * Surface @e must already be locked by the caller.
 *
 * @param x Pixel x-coordinate.
 * @param y Pixel y-coordinate.
 * @return Packed pixel colour, or 0 if out of bounds or handle is null.
 */
uint32_t Surface::get_pixel_unlocked(int16_t x, int16_t y)
{
    if (!m_handle || x < 0 || x >= m_handle->w || y < 0 || y >= m_handle->h) {
        return 0;
    }
    
    int bpp = m_handle->format->BytesPerPixel;
    uint8_t *p = (uint8_t *)m_handle->pixels + y * m_handle->pitch + x * bpp;

    switch (bpp) {
    case 1:
        return *p;
    case 2:
        return *(uint16_t *)p;
    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            return (p[0] << 16) | (p[1] << 8) | p[2];
        } else {
            return p[0] | (p[1] << 8) | (p[2] << 16);
        }
    case 4:
        return *(uint32_t *)p;
    }
    return 0;
}

/**
 * @brief Performs a stack-based flood fill without locking the surface.
 *
 * Replaces all connected pixels of `original_color` reachable from (x, y)
 * with `new_color` using 4-connectivity. Surface @e must already be locked.
 *
 * @param x              Seed x-coordinate.
 * @param y              Seed y-coordinate.
 * @param new_color      Replacement packed pixel colour.
 * @param original_color The colour to replace (pixels not matching this value stop the fill).
 */
void Surface::floodFill_unlocked(Sint16 x, Sint16 y, uint32_t new_color, uint32_t original_color)
{
    // Stack-based flood fill to avoid recursion stack overflow - assumes surface is already locked
    std::vector<std::pair<Sint16, Sint16>> stack;
    stack.push_back({x, y});
    
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        
        if (cx < 0 || cx >= m_handle->w || cy < 0 || cy >= m_handle->h) {
            continue;
        }
        
        if (get_pixel_unlocked(cx, cy) != original_color) {
            continue;
        }
        
        put_pixel_unlocked(cx, cy, new_color);
        
        // Add neighboring pixels to stack
        stack.push_back({cx + 1, cy});
        stack.push_back({cx - 1, cy});
        stack.push_back({cx, cy + 1});
        stack.push_back({cx, cy - 1});
    }
}

/**
 * @brief Performs a flood fill at (x, y) using an RGBA colour with automatic surface locking.
 *
 * Does nothing if the fill would be a no-op (target colour equals seed colour).
 *
 * @param x Seed x-coordinate.
 * @param y Seed y-coordinate.
 * @param r Red component of the fill colour.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 */
void Surface::floodFill(Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle || x < 0 || x >= m_handle->w || y < 0 || y >= m_handle->h) {
        return;
    }
    
    uint32_t new_color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    
    uint32_t original_color = get_pixel_unlocked(x, y);
    
    // Don't fill if the color is already the target color
    if (original_color == new_color) {
        return;
    }
    
    floodFill_unlocked(x, y, new_color, original_color);
}

/**
 * @brief Renders a Bézier curve without locking the surface.
 *
 * Uses De Casteljau's algorithm. Surface @e must already be locked.
 *
 * @param points Control points as (x, y) pairs.
 * @param color  Packed pixel colour.
 * @param step   Parameter increment per iteration (smaller = smoother, default 0.01).
 */
void Surface::bezierCurve_unlocked(const std::vector<std::pair<double, double>>& points, uint32_t color, double step)
{
    if (points.size() < 2) return;
    
    int order = points.size() - 1;
    
    // Bezier curve computation using De Casteljau's algorithm - assumes surface is already locked
    for (double t = 0.0; t <= 1.0; t += step) {
        // Create working copy of points for this t value
        std::vector<std::vector<std::pair<double, double>>> levels(order + 1);
        levels[0] = points;
        
        // Compute intermediate points at each level
        for (int level = 1; level <= order; level++) {
            levels[level].resize(order + 1 - level);
            for (int i = 0; i < order + 1 - level; i++) {
                double x = levels[level - 1][i].first * (1.0 - t) + levels[level - 1][i + 1].first * t;
                double y = levels[level - 1][i].second * (1.0 - t) + levels[level - 1][i + 1].second * t;
                levels[level][i] = {x, y};
            }
        }
        
        // The final point is our curve point
        int16_t px = static_cast<int16_t>(levels[order][0].first);
        int16_t py = static_cast<int16_t>(levels[order][0].second);
        
        if (px >= 0 && px < m_handle->w && py >= 0 && py < m_handle->h) {
            put_pixel_unlocked(px, py, color);
        }
    }
}

/**
 * @brief Renders an RGBA-coloured Bézier curve with automatic surface locking.
 * @param points Control points as (x, y) pairs.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 * @param step Parameter increment per iteration.
 */
void Surface::bezierCurve(const std::vector<std::pair<double, double>>& points, Uint8 r, Uint8 g, Uint8 b, Uint8 a, double step)
{
    if (!m_handle || points.size() < 2) return;
    
    uint32_t color = MapRGBA(r, g, b, a);
    SDLLockGuard lock_guard(m_handle.get());
    bezierCurve_unlocked(points, color, step);
}
