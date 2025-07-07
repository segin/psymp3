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

Surface::Surface() : m_handle(nullptr, SDL_FreeSurface)
{
    // Default constructor creates a null handle.
}

Surface::Surface(SDL_Surface *non_owned_sfc) : m_handle(non_owned_sfc, [](SDL_Surface*){ /* do nothing */ })
{
    // This constructor wraps a pointer but does not take ownership.
    // The custom empty deleter ensures SDL_FreeSurface is not called on it.
}

Surface::Surface(int width, int height)
    : m_handle(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, width, height, 32, 0, 0, 0, 0), SDL_FreeSurface)
{
    if (!m_handle) {
        throw SDLException("Could not create RGB surface");
    }
}

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

std::unique_ptr<Surface> Surface::FromBMP(std::string a_file)
{
    return FromBMP(a_file.c_str());
}

std::unique_ptr<Surface> Surface::FromBMP(const char *a_file)
{
    auto sfc_handle = SDL_LoadBMP(a_file);
    if (!sfc_handle) {
        throw SDLException("Could not load BMP");
    }
    return std::make_unique<Surface>(sfc_handle);
}

void Surface::SetAlpha(uint32_t flags, uint8_t alpha)
{
    if (!m_handle) return;
    SDL_SetAlpha(m_handle.get(), flags, alpha);
}

void Surface::Blit(Surface& src, const Rect& rect) // Changed to const Rect&
{
    if (!m_handle) return;
    SDL_Rect r = { rect.x(), rect.y(), rect.width(), rect.height() };
    SDL_BlitSurface(src.getHandle(), nullptr, m_handle.get(), &r);
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
    SDL_FillRect(m_handle.get(), 0, color);
}

void Surface::Flip()
{
    if (!m_handle) return;
    SDL_Flip(m_handle.get());
}

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

void Surface::pixel(int16_t x, int16_t y, uint32_t color)
{
    if (!m_handle || x < 0 || x >= m_handle->w || y < 0 || y >= m_handle->h) {
        return;
    }
    if (SDL_MUSTLOCK(m_handle.get())) {
        SDL_LockSurface(m_handle.get());
    }
    put_pixel_unlocked(x, y, color);
    if (SDL_MUSTLOCK(m_handle.get())) {
        SDL_UnlockSurface(m_handle.get());
    }
}

void Surface::pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    // This overload maps the RGBA components to a single color value
    // and then calls the primary pixel function.
    pixel(x, y, MapRGBA(r, g, b, a));
}

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

void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint32_t color)
{
    if (!m_handle) return;
    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());
    hline_unlocked(x1, x2, y, color);
    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    hline(x1, x2, y, MapRGBA(r, g, b, a));
}

void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    // This is implemented as a filled rectangle (box) to match its usage in the spectrum analyzer.
    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());
    if (y1 > y2) std::swap(y1, y2);
    for (int16_t y = y1; y <= y2; ++y) {
        hline_unlocked(x1, x2, y, color);
    }
    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    rectangle(x1, y1, x2, y2, MapRGBA(r, g, b, a));
}

void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    // This is now the canonical implementation for drawing a filled rectangle.
    // It uses SDL_FillRect for efficiency, breaking the recursive loop with rectangle().
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    SDL_Rect rect = { x1, y1, static_cast<Uint16>(x2 - x1 + 1), static_cast<Uint16>(y2 - y1 + 1) };
    SDL_FillRect(m_handle.get(), &rect, color);
}

void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    box(x1, y1, x2, y2, MapRGBA(r, g, b, a));
}

void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());
    vline_unlocked(x, y1, y2, color);
    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    vline(x, y1, y2, MapRGBA(r, g, b, a));
}

void Surface::line(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle) return;
    uint32_t color = MapRGBA(r, g, b, a);

    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());

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

    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::filledPolygon(const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle || n < 3) return;
    
    uint32_t color = MapRGBA(r, g, b, a);
    
    // Find bounding box
    Sint16 min_y = vy[0], max_y = vy[0];
    for (int i = 1; i < n; i++) {
        if (vy[i] < min_y) min_y = vy[i];
        if (vy[i] > max_y) max_y = vy[i];
    }
    
    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());
    
    // Scanline fill algorithm
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
    
    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::filledTriangle(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    // Use the general polygon algorithm for triangles
    Sint16 vx[3] = {x1, x2, x3};
    Sint16 vy[3] = {y1, y2, y3};
    filledPolygon(vx, vy, 3, r, g, b, a);
}

void Surface::filledCircleRGBA(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle) return;
    if (rad <= 0) return;

    uint32_t color = MapRGBA(r, g, b, a);

    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());

    for (Sint16 dy = -rad; dy <= rad; dy++) {
        // Using integer arithmetic for the circle equation to avoid floats
        Sint16 dx = static_cast<Sint16>(sqrt(rad * rad - dy * dy));
        hline_unlocked(x - dx, x + dx, y + dy, color);
    }

    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::roundedBoxRGBA(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle || rad < 0) return;
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Make sure radius is not too large for the box dimensions
    if (rad > (x2 - x1) / 2) rad = (x2 - x1) / 2;
    if (rad > (y2 - y1) / 2) rad = (y2 - y1) / 2;

    if (rad == 0) {
        box(x1, y1, x2, y2, r, g, b, a);
        return;
    }

    // Draw three non-overlapping rectangles that form the main body
    // 1. Central horizontal rectangle
    box(x1 + rad, y1 + rad, x2 - rad, y2 - rad, r, g, b, a);
    
    // 2. Top rectangle (excluding corners)
    box(x1 + rad, y1, x2 - rad, y1 + rad - 1, r, g, b, a);
    
    // 3. Bottom rectangle (excluding corners)
    box(x1 + rad, y2 - rad + 1, x2 - rad, y2, r, g, b, a);
    
    // 4. Left rectangle (excluding corners)
    box(x1, y1 + rad, x1 + rad - 1, y2 - rad, r, g, b, a);
    
    // 5. Right rectangle (excluding corners)
    box(x2 - rad + 1, y1 + rad, x2, y2 - rad, r, g, b, a);

    // Draw the four corner circles
    filledCircleRGBA(x1 + rad, y1 + rad, rad, r, g, b, a);
    filledCircleRGBA(x2 - rad, y1 + rad, rad, r, g, b, a);
    filledCircleRGBA(x1 + rad, y2 - rad, rad, r, g, b, a);
    filledCircleRGBA(x2 - rad, y2 - rad, rad, r, g, b, a);
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
    return m_handle.get();
}

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

void Surface::floodFill(Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!m_handle || x < 0 || x >= m_handle->w || y < 0 || y >= m_handle->h) {
        return;
    }
    
    uint32_t new_color = MapRGBA(r, g, b, a);
    
    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());
    
    uint32_t original_color = get_pixel_unlocked(x, y);
    
    // Don't fill if the color is already the target color
    if (original_color == new_color) {
        if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
        return;
    }
    
    // Stack-based flood fill to avoid recursion stack overflow
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
    
    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}

void Surface::bezierCurve(const std::vector<std::pair<double, double>>& points, Uint8 r, Uint8 g, Uint8 b, Uint8 a, double step)
{
    if (!m_handle || points.size() < 2) return;
    
    uint32_t color = MapRGBA(r, g, b, a);
    int order = points.size() - 1;
    
    if (SDL_MUSTLOCK(m_handle.get())) SDL_LockSurface(m_handle.get());
    
    // Bezier curve computation using De Casteljau's algorithm
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
    
    if (SDL_MUSTLOCK(m_handle.get())) SDL_UnlockSurface(m_handle.get());
}
