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
#include <algorithm> // For std::max and std::min

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

// Geometric operation methods

/**
 * @brief Check if a point is contained within the rectangle
 * @param x The x coordinate of the point
 * @param y The y coordinate of the point
 * @return True if the point is inside the rectangle (inclusive of edges), false otherwise
 */
bool Rect::contains(int16_t x, int16_t y) const
{
    // Empty rectangles contain no points
    if (isEmpty()) {
        return false;
    }
    
    // Check if point is within bounds (inclusive)
    return x >= m_x && x < m_x + m_width &&
           y >= m_y && y < m_y + m_height;
}

/**
 * @brief Check if another rectangle is completely contained within this rectangle
 * @param other The rectangle to check for containment
 * @return True if the other rectangle is completely inside this rectangle, false otherwise
 */
bool Rect::contains(const Rect& other) const
{
    // Empty rectangles cannot contain anything, and nothing can contain empty rectangles
    if (isEmpty() || other.isEmpty()) {
        return false;
    }
    
    // Check if all corners of the other rectangle are within this rectangle
    return other.m_x >= m_x &&
           other.m_y >= m_y &&
           other.m_x + other.m_width <= m_x + m_width &&
           other.m_y + other.m_height <= m_y + m_height;
}

/**
 * @brief Check if this rectangle intersects with another rectangle
 * @param other The rectangle to check for intersection
 * @return True if the rectangles overlap, false otherwise
 */
bool Rect::intersects(const Rect& other) const
{
    // Empty rectangles don't intersect with anything
    if (isEmpty() || other.isEmpty()) {
        return false;
    }
    
    // Check if rectangles are separated on any axis
    // If separated on x-axis: this.right <= other.left OR other.right <= this.left
    // If separated on y-axis: this.bottom <= other.top OR other.bottom <= this.top
    return !(m_x + m_width <= other.m_x ||           // this is left of other
             other.m_x + other.m_width <= m_x ||     // other is left of this
             m_y + m_height <= other.m_y ||          // this is above other
             other.m_y + other.m_height <= m_y);     // other is above this
}

/**
 * @brief Calculate the intersection rectangle of this rectangle with another
 * @param other The rectangle to intersect with
 * @return The intersection rectangle, or Rect(0, 0, 0, 0) if no intersection
 */
Rect Rect::intersection(const Rect& other) const
{
    // If rectangles don't intersect, return empty rectangle
    if (!intersects(other)) {
        return Rect(0, 0, 0, 0);
    }
    
    // Calculate intersection bounds
    int16_t left = std::max(m_x, other.m_x);
    int16_t top = std::max(m_y, other.m_y);
    int16_t right = std::min(m_x + m_width, other.m_x + other.m_width);
    int16_t bottom = std::min(m_y + m_height, other.m_y + other.m_height);
    
    // Calculate width and height
    uint16_t width = static_cast<uint16_t>(right - left);
    uint16_t height = static_cast<uint16_t>(bottom - top);
    
    return Rect(left, top, width, height);
}

/**
 * @brief Calculate the union (bounding box) of this rectangle with another
 * @param other The rectangle to unite with
 * @return The smallest rectangle that contains both rectangles
 */
Rect Rect::united(const Rect& other) const
{
    // Handle empty rectangles - union with empty rectangle returns the non-empty one
    if (isEmpty() && other.isEmpty()) {
        return Rect(0, 0, 0, 0);  // Both empty, return empty
    }
    if (isEmpty()) {
        return other;  // This is empty, return other
    }
    if (other.isEmpty()) {
        return *this;  // Other is empty, return this
    }
    
    // Calculate bounding box using int32_t to avoid overflow
    int16_t left = std::min(m_x, other.m_x);
    int16_t top = std::min(m_y, other.m_y);
    int32_t right = std::max(static_cast<int32_t>(m_x) + static_cast<int32_t>(m_width), 
                            static_cast<int32_t>(other.m_x) + static_cast<int32_t>(other.m_width));
    int32_t bottom = std::max(static_cast<int32_t>(m_y) + static_cast<int32_t>(m_height), 
                             static_cast<int32_t>(other.m_y) + static_cast<int32_t>(other.m_height));
    
    // Handle potential coordinate overflow
    // Check if the calculated dimensions would exceed uint16_t limits
    int32_t width_calc = right - left;
    int32_t height_calc = bottom - top;
    
    // Clamp to maximum uint16_t values if overflow would occur
    uint16_t width = (width_calc > 65535) ? 65535 : static_cast<uint16_t>(width_calc);
    uint16_t height = (height_calc > 65535) ? 65535 : static_cast<uint16_t>(height_calc);
    
    return Rect(left, top, width, height);
}
// Expansion methods

/**
 * @brief Expand the rectangle uniformly by a margin in all directions
 * @param margin The margin to expand by (positive values expand, negative values shrink)
 */
void Rect::expand(int16_t margin)
{
    expand(margin, margin);
}

/**
 * @brief Expand the rectangle by specified amounts in x and y directions
 * @param dx The amount to expand horizontally (added to both left and right)
 * @param dy The amount to expand vertically (added to both top and bottom)
 */
void Rect::expand(int16_t dx, int16_t dy)
{
    // Calculate new position (move left/up by the expansion amount)
    int32_t new_x = static_cast<int32_t>(m_x) - static_cast<int32_t>(dx);
    int32_t new_y = static_cast<int32_t>(m_y) - static_cast<int32_t>(dy);
    
    // Calculate new dimensions (expand by 2x the amount since we expand in both directions)
    int32_t new_width = static_cast<int32_t>(m_width) + 2 * static_cast<int32_t>(dx);
    int32_t new_height = static_cast<int32_t>(m_height) + 2 * static_cast<int32_t>(dy);
    
    // Handle coordinate overflow/underflow for position
    if (new_x < -32768) new_x = -32768;
    if (new_x > 32767) new_x = 32767;
    if (new_y < -32768) new_y = -32768;
    if (new_y > 32767) new_y = 32767;
    
    // Handle dimension underflow/overflow
    if (new_width < 0) new_width = 0;
    if (new_width > 65535) new_width = 65535;
    if (new_height < 0) new_height = 0;
    if (new_height > 65535) new_height = 65535;
    
    // Update the rectangle
    m_x = static_cast<int16_t>(new_x);
    m_y = static_cast<int16_t>(new_y);
    m_width = static_cast<uint16_t>(new_width);
    m_height = static_cast<uint16_t>(new_height);
}

/**
 * @brief Return a new rectangle expanded uniformly by a margin in all directions
 * @param margin The margin to expand by (positive values expand, negative values shrink)
 * @return A new expanded rectangle
 */
Rect Rect::expanded(int16_t margin) const
{
    return expanded(margin, margin);
}

/**
 * @brief Return a new rectangle expanded by specified amounts in x and y directions
 * @param dx The amount to expand horizontally (added to both left and right)
 * @param dy The amount to expand vertically (added to both top and bottom)
 * @return A new expanded rectangle
 */
Rect Rect::expanded(int16_t dx, int16_t dy) const
{
    Rect result(*this);
    result.expand(dx, dy);
    return result;
}

// Shrinking methods

/**
 * @brief Shrink the rectangle uniformly by a margin in all directions
 * @param margin The margin to shrink by (positive values shrink, negative values expand)
 */
void Rect::shrink(int16_t margin)
{
    shrink(margin, margin);
}

/**
 * @brief Shrink the rectangle by specified amounts in x and y directions
 * @param dx The amount to shrink horizontally (removed from both left and right)
 * @param dy The amount to shrink vertically (removed from both top and bottom)
 */
void Rect::shrink(int16_t dx, int16_t dy)
{
    // Calculate new position (move right/down by the shrinking amount)
    int32_t new_x = static_cast<int32_t>(m_x) + static_cast<int32_t>(dx);
    int32_t new_y = static_cast<int32_t>(m_y) + static_cast<int32_t>(dy);
    
    // Calculate new dimensions (shrink by 2x the amount since we shrink from both sides)
    int32_t new_width = static_cast<int32_t>(m_width) - 2 * static_cast<int32_t>(dx);
    int32_t new_height = static_cast<int32_t>(m_height) - 2 * static_cast<int32_t>(dy);
    
    // Handle coordinate overflow/underflow for position
    if (new_x < -32768) new_x = -32768;
    if (new_x > 32767) new_x = 32767;
    if (new_y < -32768) new_y = -32768;
    if (new_y > 32767) new_y = 32767;
    
    // Handle dimension underflow/overflow - ensure dimensions don't become negative
    if (new_width < 0) new_width = 0;
    if (new_width > 65535) new_width = 65535;
    if (new_height < 0) new_height = 0;
    if (new_height > 65535) new_height = 65535;
    
    // Update the rectangle
    m_x = static_cast<int16_t>(new_x);
    m_y = static_cast<int16_t>(new_y);
    m_width = static_cast<uint16_t>(new_width);
    m_height = static_cast<uint16_t>(new_height);
}

/**
 * @brief Return a new rectangle shrunk uniformly by a margin in all directions
 * @param margin The margin to shrink by (positive values shrink, negative values expand)
 * @return A new shrunk rectangle
 */
Rect Rect::shrunk(int16_t margin) const
{
    return shrunk(margin, margin);
}

/**
 * @brief Return a new rectangle shrunk by specified amounts in x and y directions
 * @param dx The amount to shrink horizontally (removed from both left and right)
 * @param dy The amount to shrink vertically (removed from both top and bottom)
 * @return A new shrunk rectangle
 */
Rect Rect::shrunk(int16_t dx, int16_t dy) const
{
    Rect result(*this);
    result.shrink(dx, dy);
    return result;
}

// Translation methods

/**
 * @brief Move the rectangle by the specified offset
 * @param dx The horizontal offset to move by
 * @param dy The vertical offset to move by
 */
void Rect::translate(int16_t dx, int16_t dy)
{
    // Calculate new position using int32_t to detect overflow
    int32_t new_x = static_cast<int32_t>(m_x) + static_cast<int32_t>(dx);
    int32_t new_y = static_cast<int32_t>(m_y) + static_cast<int32_t>(dy);
    
    // Handle coordinate overflow/underflow
    if (new_x < -32768) new_x = -32768;
    if (new_x > 32767) new_x = 32767;
    if (new_y < -32768) new_y = -32768;
    if (new_y > 32767) new_y = 32767;
    
    // Update position
    m_x = static_cast<int16_t>(new_x);
    m_y = static_cast<int16_t>(new_y);
}

/**
 * @brief Return a new rectangle moved by the specified offset
 * @param dx The horizontal offset to move by
 * @param dy The vertical offset to move by
 * @return A new rectangle at the translated position
 */
Rect Rect::translated(int16_t dx, int16_t dy) const
{
    Rect result(*this);
    result.translate(dx, dy);
    return result;
}

/**
 * @brief Move the rectangle to the specified absolute position
 * @param x The new x coordinate
 * @param y The new y coordinate
 */
void Rect::moveTo(int16_t x, int16_t y)
{
    m_x = x;
    m_y = y;
}

/**
 * @brief Return a new rectangle at the specified absolute position
 * @param x The new x coordinate
 * @param y The new y coordinate
 * @return A new rectangle at the specified position
 */
Rect Rect::movedTo(int16_t x, int16_t y) const
{
    return Rect(x, y, m_width, m_height);
}

// Resizing methods

/**
 * @brief Resize the rectangle to the specified dimensions
 * @param width The new width (must be non-negative)
 * @param height The new height (must be non-negative)
 */
void Rect::resize(uint16_t width, uint16_t height)
{
    m_width = width;
    m_height = height;
}

/**
 * @brief Return a new rectangle with the specified dimensions
 * @param width The new width (must be non-negative)
 * @param height The new height (must be non-negative)
 * @return A new rectangle with the same position but new dimensions
 */
Rect Rect::resized(uint16_t width, uint16_t height) const
{
    return Rect(m_x, m_y, width, height);
}

// Combined adjustment methods

/**
 * @brief Adjust the rectangle's position and size by the specified deltas
 * @param dx The change in x coordinate
 * @param dy The change in y coordinate
 * @param dw The change in width (can be negative)
 * @param dh The change in height (can be negative)
 */
void Rect::adjust(int16_t dx, int16_t dy, int16_t dw, int16_t dh)
{
    // Calculate new position using int32_t to detect overflow
    int32_t new_x = static_cast<int32_t>(m_x) + static_cast<int32_t>(dx);
    int32_t new_y = static_cast<int32_t>(m_y) + static_cast<int32_t>(dy);
    
    // Calculate new dimensions using int32_t to detect overflow/underflow
    int32_t new_width = static_cast<int32_t>(m_width) + static_cast<int32_t>(dw);
    int32_t new_height = static_cast<int32_t>(m_height) + static_cast<int32_t>(dh);
    
    // Handle coordinate overflow/underflow for position
    if (new_x < -32768) new_x = -32768;
    if (new_x > 32767) new_x = 32767;
    if (new_y < -32768) new_y = -32768;
    if (new_y > 32767) new_y = 32767;
    
    // Handle dimension underflow/overflow - ensure dimensions don't become negative
    if (new_width < 0) new_width = 0;
    if (new_width > 65535) new_width = 65535;
    if (new_height < 0) new_height = 0;
    if (new_height > 65535) new_height = 65535;
    
    // Update the rectangle
    m_x = static_cast<int16_t>(new_x);
    m_y = static_cast<int16_t>(new_y);
    m_width = static_cast<uint16_t>(new_width);
    m_height = static_cast<uint16_t>(new_height);
}

/**
 * @brief Return a new rectangle with position and size adjusted by the specified deltas
 * @param dx The change in x coordinate
 * @param dy The change in y coordinate
 * @param dw The change in width (can be negative)
 * @param dh The change in height (can be negative)
 * @return A new rectangle with adjusted position and dimensions
 */
Rect Rect::adjusted(int16_t dx, int16_t dy, int16_t dw, int16_t dh) const
{
    Rect result(*this);
    result.adjust(dx, dy, dw, dh);
    return result;
}