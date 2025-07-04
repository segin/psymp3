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
class Widget;

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
        void filledCircleRGBA(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        void roundedBoxRGBA(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
        int16_t height();
        int16_t width();
        SDL_Surface * getHandle();
        friend class Display;
        friend class Widget;
        friend class Font;
    protected:
        std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> m_handle;
    private:
        void put_pixel_unlocked(int16_t x, int16_t y, uint32_t color);
        void hline_unlocked(int16_t x1, int16_t x2, int16_t y, uint32_t color);
        void vline_unlocked(int16_t x, int16_t y1, int16_t y2, uint32_t color);
};

/* This is experimental */


#endif // SURFACE_H
