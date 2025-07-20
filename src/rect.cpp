/*
 * rect.cpp - Rect class "implementation" (does nothing much)
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

Rect::Rect()
{
    //ctor
}

Rect::Rect(uint16_t width, uint16_t height) : m_x(0), m_y(0), m_width(width), m_height(height)
{
}

Rect::Rect(int16_t x, int16_t y, uint16_t w, uint16_t h) : m_x(x), m_y(y), m_width(w), m_height(h)
{
}

Rect::~Rect()
{
    //dtor
}

// Accessor methods

/**
 * @brief Get the x coordinate of the rectangle
 * @return The x coordinate
 */
int16_t Rect::x() const
{
    return m_x;
}

/**
 * @brief Get the y coordinate of the rectangle
 * @return The y coordinate
 */
int16_t Rect::y() const
{
    return m_y;
}

/**
 * @brief Get the width of the rectangle
 * @return The width
 */
uint16_t Rect::width() const
{
    return m_width;
}

/**
 * @brief Get the height of the rectangle
 * @return The height
 */
uint16_t Rect::height() const
{
    return m_height;
}

// Mutator methods

/**
 * @brief Set the x coordinate of the rectangle
 * @param val The new x coordinate
 */
void Rect::x(int16_t val)
{
    m_x = val;
}

/**
 * @brief Set the y coordinate of the rectangle
 * @param val The new y coordinate
 */
void Rect::y(int16_t val)
{
    m_y = val;
}

/**
 * @brief Set the width of the rectangle
 * @param a The new width
 */
void Rect::width(uint16_t a)
{
    m_width = a;
}

/**
 * @brief Set the height of the rectangle
 * @param a The new height
 */
void Rect::height(uint16_t a)
{
    m_height = a;
}

// Edge coordinate methods

/**
 * @brief Get the left edge coordinate (alias to x())
 * @return The left edge coordinate
 */
int16_t Rect::left() const
{
    return m_x;
}

/**
 * @brief Get the top edge coordinate (alias to y())
 * @return The top edge coordinate
 */
int16_t Rect::top() const
{
    return m_y;
}

/**
 * @brief Get the right edge coordinate
 * @return The right edge coordinate (x + width)
 */
int16_t Rect::right() const
{
    return m_x + m_width;
}

/**
 * @brief Get the bottom edge coordinate
 * @return The bottom edge coordinate (y + height)
 */
int16_t Rect::bottom() const
{
    return m_y + m_height;
}

// Center point calculation methods

/**
 * @brief Calculate the center X coordinate of the rectangle
 * @return The center X coordinate (x + width / 2)
 */
int16_t Rect::centerX() const
{
    return m_x + m_width / 2;
}

/**
 * @brief Calculate the center Y coordinate of the rectangle
 * @return The center Y coordinate (y + height / 2)
 */
int16_t Rect::centerY() const
{
    return m_y + m_height / 2;
}

/**
 * @brief Calculate the center point of the rectangle
 * @return A pair containing the center coordinates (centerX, centerY)
 */
std::pair<int16_t, int16_t> Rect::center() const
{
    return std::make_pair(centerX(), centerY());
}

// Area and validation methods

/**
 * @brief Calculate the area of the rectangle
 * @return The area (width * height)
 */
uint32_t Rect::area() const
{
    return static_cast<uint32_t>(m_width) * static_cast<uint32_t>(m_height);
}

/**
 * @brief Check if the rectangle is empty (zero width or height)
 * @return True if width or height is zero, false otherwise
 */
bool Rect::isEmpty() const
{
    return m_width == 0 || m_height == 0;
}

/**
 * @brief Check if the rectangle is valid
 * @return True if the rectangle has positive width and height, false otherwise
 */
bool Rect::isValid() const
{
    return m_width > 0 && m_height > 0;
}
