/*
 * surface.h - class implementation for SDL_Surface wrapper
 * This also wraps SDL_gfx for primitives.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "psymp3.h"

Surface::Surface()
{
    //ctor - wat do?
    m_handle = NULL; // XXX: Figure out what to do for a default constructor, if we're to ever use it.
}

Surface::Surface(SDL_Surface *sfc)
{
    m_handle = sfc;
}

Surface::~Surface()
{
    //dtor
    if (m_handle) SDL_FreeSurface(m_handle);
}

bool Surface::isInit()
{
    if (m_handle) return true; else return false;
}

void Surface::pixel(int16_t x, int16_t y, uint32_t color)
{
    if (!m_handle) return;
    pixelColor(m_handle, x, y, color);
}

void Surface::pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    pixelRGBA(m_handle, x, y, r, g, b, a);
}

void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    rectangleColor(m_handle, x1, y1, x2, y2, color);
}

void Surface::rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    rectangleRGBA(m_handle, x1, y1, x2, y2, r, g, b, a);
}

void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    boxColor(m_handle, x1, y1, x2, y2, color);
}

void Surface::box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    boxRGBA(m_handle, x1, y1, x2, y2, r, g, b, a);
}

void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint32_t color)
{
    if (!m_handle) return;
    hlineColor(m_handle, x1, x2, y, color);
}

void Surface::hline(int16_t x1, int16_t x2, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    hlineRGBA(m_handle, x1, x2, y, r, g, b, a);
}

void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint32_t color)
{
    if (!m_handle) return;
    vlineColor(m_handle, x, y1, y2, color);
}

void Surface::vline(int16_t x, int16_t y1, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!m_handle) return;
    vlineRGBA(m_handle, x, y1, y2, r, g, b, a);
}

SDL_Surface * Surface::getHandle()
{
    return m_handle;
}
