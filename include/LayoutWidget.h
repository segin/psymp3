/*
 * LayoutWidget.h - Generic container widget for layout and grouping
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef LAYOUTWIDGET_H
#define LAYOUTWIDGET_H

#include "Widget.h"

/**
 * @brief Generic container widget for grouping and laying out child widgets.
 * 
 * This widget provides a simple, reusable container that can hold multiple
 * child widgets without requiring subclassing. It's useful for:
 * - Grouping related widgets together
 * - Creating composite UI elements
 * - Building hierarchical UI structures
 * - Window content areas
 * - Panel and toolbar layouts
 */
class LayoutWidget : public Widget {
public:
    /**
     * @brief Constructor for LayoutWidget.
     * @param width Width of the layout container
     * @param height Height of the layout container
     * @param transparent Whether the background should be transparent (default: true)
     */
    LayoutWidget(int width = 0, int height = 0, bool transparent = true);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~LayoutWidget() = default;
    
    /**
     * @brief Sets the background color for the layout.
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param a Alpha component (0-255, default: 255)
     */
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    
    /**
     * @brief Makes the background transparent.
     */
    void setTransparent();
    
    /**
     * @brief Adds a child widget at a specific position within this layout.
     * @param child Unique pointer to the child widget
     * @param x X position within this layout
     * @param y Y position within this layout
     * @return Non-owning pointer to the added child widget
     */
    template<typename T>
    T* addChildAt(std::unique_ptr<T> child, int x, int y) {
        T* ptr = child.get();
        Rect current_pos = child->getPos();
        child->setPos(Rect(x, y, current_pos.width(), current_pos.height()));
        addChild(std::move(child));
        return ptr;
    }
    
    /**
     * @brief Adds a child widget at a specific position and size within this layout.
     * @param child Unique pointer to the child widget
     * @param x X position within this layout
     * @param y Y position within this layout
     * @param width Width of the child widget
     * @param height Height of the child widget
     * @return Non-owning pointer to the added child widget
     */
    template<typename T>
    T* addChildAt(std::unique_ptr<T> child, int x, int y, int width, int height) {
        T* ptr = child.get();
        child->setPos(Rect(x, y, width, height));
        addChild(std::move(child));
        return ptr;
    }
    
    /**
     * @brief Resizes the layout container.
     * @param new_width New width
     * @param new_height New height
     */
    void resize(int new_width, int new_height);

private:
    bool m_transparent;
    uint8_t m_bg_r, m_bg_g, m_bg_b, m_bg_a;
    
    /**
     * @brief Updates the layout's background surface.
     */
    void updateBackground();
};

#endif // LAYOUTWIDGET_H