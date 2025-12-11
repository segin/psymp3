/*
 * rect.cpp - Rect class "implementation" (does nothing much)
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace Core {

#include <algorithm> // For std::max and std::min
#include <sstream>   // For std::ostringstream
#include <limits>    // For std::numeric_limits

Rect::Rect() : m_x(0), m_y(0), m_width(0), m_height(0)
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
 * @brief Get the right edge coordinate
 * @return The right edge coordinate (x + width), clamped to int16_t range
 */
int16_t Rect::right() const
{
    int32_t result = static_cast<int32_t>(m_x) + static_cast<int32_t>(m_width);
    return clampToInt16(result);
}

/**
 * @brief Get the bottom edge coordinate
 * @return The bottom edge coordinate (y + height), clamped to int16_t range
 */
int16_t Rect::bottom() const
{
    int32_t result = static_cast<int32_t>(m_y) + static_cast<int32_t>(m_height);
    return clampToInt16(result);
}

// Center point calculation methods

/**
 * @brief Calculate the center X coordinate of the rectangle
 * @return The center X coordinate (x + width / 2), with overflow protection
 */
int16_t Rect::centerX() const
{
    int32_t result = static_cast<int32_t>(m_x) + static_cast<int32_t>(m_width) / 2;
    return clampToInt16(result);
}

/**
 * @brief Calculate the center Y coordinate of the rectangle
 * @return The center Y coordinate (y + height / 2), with overflow protection
 */
int16_t Rect::centerY() const
{
    int32_t result = static_cast<int32_t>(m_y) + static_cast<int32_t>(m_height) / 2;
    return clampToInt16(result);
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
 * 
 * Performance optimized: Uses early exit and avoids potential overflow
 */
bool Rect::contains(int16_t x, int16_t y) const
{
    // Early exit for empty rectangles - most common failure case
    if (m_width == 0 || m_height == 0) {
        return false;
    }
    
    // Optimized bounds checking with early exit on first failure
    // Check x bounds first (often fails earlier in UI hit testing)
    if (x < m_x || x >= m_x + static_cast<int32_t>(m_width)) {
        return false;
    }
    
    // Check y bounds only if x bounds passed
    return y >= m_y && y < m_y + static_cast<int32_t>(m_height);
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
 * 
 * Performance optimized: Early exit and optimized separation testing
 */
bool Rect::intersects(const Rect& other) const
{
    // Early exit for empty rectangles - most common failure case
    if (m_width == 0 || m_height == 0 || other.m_width == 0 || other.m_height == 0) {
        return false;
    }
    
    // Optimized separation test with early exit
    // Test x-axis separation first (often fails earlier in UI collision detection)
    if (m_x >= other.m_x + static_cast<int32_t>(other.m_width) ||  // this is right of other
        other.m_x >= m_x + static_cast<int32_t>(m_width)) {        // other is right of this
        return false;
    }
    
    // Test y-axis separation only if x-axis test passed
    return !(m_y >= other.m_y + static_cast<int32_t>(other.m_height) ||  // this is below other
             other.m_y >= m_y + static_cast<int32_t>(m_height));          // other is below this
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

// Centering operations

/**
 * @brief Center this rectangle within the specified container rectangle
 * @param container The container rectangle to center within
 * 
 * This method positions the rectangle so that its center point aligns with
 * the center point of the container. If the rectangle is larger than the
 * container in any dimension, it will be positioned so that it extends
 * equally beyond both sides of the container in that dimension.
 */
void Rect::centerIn(const Rect& container)
{
    // Calculate the center point of the container
    int16_t container_center_x = container.centerX();
    int16_t container_center_y = container.centerY();
    
    // Calculate the new position to center this rectangle
    // Position = container_center - (our_size / 2)
    int32_t new_x = static_cast<int32_t>(container_center_x) - static_cast<int32_t>(m_width) / 2;
    int32_t new_y = static_cast<int32_t>(container_center_y) - static_cast<int32_t>(m_height) / 2;
    
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
 * @brief Return a new rectangle centered within the specified container rectangle
 * @param container The container rectangle to center within
 * @return A new rectangle positioned at the center of the container
 * 
 * This method returns a new rectangle with the same dimensions as this one,
 * but positioned so that its center point aligns with the center point of
 * the container. If the rectangle is larger than the container in any
 * dimension, it will be positioned so that it extends equally beyond both
 * sides of the container in that dimension.
 */
Rect Rect::centeredIn(const Rect& container) const
{
    Rect result(*this);
    result.centerIn(container);
    return result;
}

// Comparison operators



// String representation and debugging support

/**
 * @brief Return a string representation of the rectangle
 * @return A formatted string in the format "Rect(x, y, width, height)" with validation status
 */
std::string Rect::toString() const
{
    std::ostringstream oss;
    oss << "Rect(" << m_x << ", " << m_y << ", " << m_width << ", " << m_height << ")";
    
    // Include validation status in debug output when rectangle is invalid
    if (!isValid()) {
        if (isEmpty()) {
            oss << " [EMPTY]";
        } else {
            oss << " [INVALID]";
        }
    }
    
    return oss.str();
}

// Coordinate system validation and normalization methods

/**
 * @brief Return a normalized version of this rectangle with positive dimensions
 * @return A new rectangle with positive width and height
 * 
 * This method handles rectangles that may have been created with negative
 * width or height values by adjusting the position and making dimensions positive.
 * For example, a rectangle at (10, 10) with width -5 and height -3 would become
 * a rectangle at (5, 7) with width 5 and height 3.
 */
Rect Rect::normalized() const
{
    Rect result(*this);
    result.normalize();
    return result;
}

/**
 * @brief Normalize this rectangle in-place to have positive dimensions
 * 
 * This method adjusts the rectangle's position and dimensions to ensure
 * width and height are positive. If width or height are negative, the
 * position is adjusted accordingly and the dimensions are made positive.
 */
void Rect::normalize()
{
    // Handle negative width
    if (static_cast<int32_t>(m_width) < 0) {
        // Convert to signed for calculation
        int32_t signed_width = static_cast<int32_t>(m_width);
        // Adjust position and make width positive
        m_x = safeAdd(m_x, static_cast<int16_t>(signed_width));
        m_width = static_cast<uint16_t>(-signed_width);
    }
    
    // Handle negative height
    if (static_cast<int32_t>(m_height) < 0) {
        // Convert to signed for calculation
        int32_t signed_height = static_cast<int32_t>(m_height);
        // Adjust position and make height positive
        m_y = safeAdd(m_y, static_cast<int16_t>(signed_height));
        m_height = static_cast<uint16_t>(-signed_height);
    }
}

// Safe arithmetic methods for internal use

/**
 * @brief Check if a 32-bit value would overflow when cast to int16_t
 * @param value The value to check
 * @param min_val The minimum allowed value (typically -32768)
 * @param max_val The maximum allowed value (typically 32767)
 * @return True if the value would overflow, false otherwise
 */
bool Rect::wouldOverflow(int32_t value, int16_t min_val, int16_t max_val)
{
    return value < static_cast<int32_t>(min_val) || value > static_cast<int32_t>(max_val);
}

/**
 * @brief Check if a 32-bit unsigned value would overflow when cast to uint16_t
 * @param value The value to check
 * @param max_val The maximum allowed value (typically 65535)
 * @return True if the value would overflow, false otherwise
 */
bool Rect::wouldOverflow(uint32_t value, uint16_t max_val)
{
    return value > static_cast<uint32_t>(max_val);
}

/**
 * @brief Safely add two int16_t values with overflow protection
 * @param a First value
 * @param b Second value
 * @return The sum, clamped to int16_t range if overflow would occur
 */
int16_t Rect::safeAdd(int16_t a, int16_t b)
{
    int32_t result = static_cast<int32_t>(a) + static_cast<int32_t>(b);
    return clampToInt16(result);
}

/**
 * @brief Safely subtract two int16_t values with overflow protection
 * @param a First value (minuend)
 * @param b Second value (subtrahend)
 * @return The difference, clamped to int16_t range if overflow would occur
 */
int16_t Rect::safeSubtract(int16_t a, int16_t b)
{
    int32_t result = static_cast<int32_t>(a) - static_cast<int32_t>(b);
    return clampToInt16(result);
}

/**
 * @brief Safely add two uint16_t values with overflow protection
 * @param a First value
 * @param b Second value
 * @return The sum, clamped to uint16_t range if overflow would occur
 */
uint16_t Rect::safeAdd(uint16_t a, uint16_t b)
{
    uint32_t result = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
    return clampToUInt16(result);
}

/**
 * @brief Safely subtract two uint16_t values with underflow protection
 * @param a First value (minuend)
 * @param b Second value (subtrahend)
 * @return The difference, clamped to 0 if underflow would occur
 */
uint16_t Rect::safeSubtract(uint16_t a, uint16_t b)
{
    if (b > a) {
        return 0;  // Prevent underflow
    }
    return a - b;
}

/**
 * @brief Clamp a 32-bit signed value to int16_t range
 * @param value The value to clamp
 * @return The value clamped to [-32768, 32767] range
 */
int16_t Rect::clampToInt16(int32_t value)
{
    if (value < std::numeric_limits<int16_t>::min()) {
        return std::numeric_limits<int16_t>::min();
    }
    if (value > std::numeric_limits<int16_t>::max()) {
        return std::numeric_limits<int16_t>::max();
    }
    return static_cast<int16_t>(value);
}

/**
 * @brief Clamp a 32-bit unsigned value to uint16_t range
 * @param value The value to clamp
 * @return The value clamped to [0, 65535] range
 */
uint16_t Rect::clampToUInt16(uint32_t value)
{
    if (value > std::numeric_limits<uint16_t>::max()) {
        return std::numeric_limits<uint16_t>::max();
    }
    return static_cast<uint16_t>(value);
}
} // namespace Core
} // namespace PsyMP3
