/*
 * display.h - class implementation for SDL display wrapper
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

#ifndef SURFACE_H
#define SURFACE_H

class Display;

// Forward declaration for Widget (now in namespace)
namespace PsyMP3 {
namespace Widget {
namespace Foundation {
    class Widget;
}}}
using PsyMP3::Widget::Foundation::Widget;

class Surface
{
    public:
        // Default constructor for an empty surface
        Surface(); 
        // Constructor for creating a new, owned surface
        Surface(int width, int height);
        // Constructor for creating a text surface with explicit RGBA format
        Surface(int width, int height, bool for_text);
        // Constructor for wrapping a non-owned surface (e.g., the main screen)
        explicit Surface(SDL_Surface *non_owned_sfc);

        // Rule of Five: Disable copy/move to prevent ownership issues
        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;
        Surface(Surface&&) = default;
        Surface& operator=(Surface&&) = default;
        virtual ~Surface() = default;

        static std::unique_ptr<Surface> FromBMP(const char *a_file);
        static std::unique_ptr<Surface> FromBMP(std::string a_file);
        bool isValid();
        uint32_t MapRGB(uint8_t r, uint8_t g, uint8_t b);
        void SetAlpha(uint32_t flags, uint8_t alpha); // SDL_SRCALPHA, SDL_ALPHA_OPAQUE, etc.
        uint32_t MapRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void Blit(Surface& src, const Rect& rect); // Changed to const Rect&
        void FillRect(uint32_t color);
        void Flip();
        void pixel(int16_t x, int16_t y, uint32_t color);
        void pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
        void rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
        void box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void hline(int16_t x1, int16_t x2, int16_t y, uint32_t color);
        void hline(int16_t x1, int16_t x2, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void vline(int16_t x, int16_t y1, int16_t y2, uint32_t color);
        void vline(int16_t x, int16_t y1, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void line(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void filledPolygon(const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void filledTriangle(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void filledCircleRGBA(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void roundedBoxRGBA(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void roundedBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, uint32_t color);
        void floodFill(Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void bezierCurve(const std::vector<std::pair<double, double>>& points, Uint8 r, Uint8 g, Uint8 b, Uint8 a, double step = 0.01);
        int16_t height();
        int16_t width();
        SDL_Surface * getHandle();
        friend class Display;
        friend class Widget;
        friend class Font;
    protected:
        std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> m_handle;
    private:
        // RAII helper for SDL surface locking
        class SDLLockGuard {
        public:
            explicit SDLLockGuard(SDL_Surface* surface) : m_surface(surface), m_locked(false) {
                if (m_surface && SDL_MUSTLOCK(m_surface)) {
                    SDL_LockSurface(m_surface);
                    m_locked = true;
                }
            }
            
            ~SDLLockGuard() {
                if (m_locked) {
                    SDL_UnlockSurface(m_surface);
                }
            }
            
            // Non-copyable, non-movable
            SDLLockGuard(const SDLLockGuard&) = delete;
            SDLLockGuard& operator=(const SDLLockGuard&) = delete;
            SDLLockGuard(SDLLockGuard&&) = delete;
            SDLLockGuard& operator=(SDLLockGuard&&) = delete;
            
        private:
            SDL_Surface* m_surface;
            bool m_locked;
        };
        
        void put_pixel_unlocked(int16_t x, int16_t y, uint32_t color);
        uint32_t get_pixel_unlocked(int16_t x, int16_t y);
        void hline_unlocked(int16_t x1, int16_t x2, int16_t y, uint32_t color);
        void vline_unlocked(int16_t x, int16_t y1, int16_t y2, uint32_t color);
        void line_unlocked(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, uint32_t color);
        void rectangle_unlocked(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
        void box_unlocked(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
        void filledPolygon_unlocked(const Sint16* vx, const Sint16* vy, int n, uint32_t color);
        void filledTriangle_unlocked(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, uint32_t color);
        void filledCircleRGBA_unlocked(Sint16 x, Sint16 y, Sint16 rad, uint32_t color);
        void roundedBoxRGBA_unlocked(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, uint32_t color);
        void floodFill_unlocked(Sint16 x, Sint16 y, uint32_t new_color, uint32_t original_color);
        void bezierCurve_unlocked(const std::vector<std::pair<double, double>>& points, uint32_t color, double step);
};

/* This is experimental */


#endif // SURFACE_H
