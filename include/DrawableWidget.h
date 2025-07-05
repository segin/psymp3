/*
 * DrawableWidget.h - Base class for widgets that perform custom rendering
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

#ifndef DRAWABLEWIDGET_H
#define DRAWABLEWIDGET_H

#include "Widget.h"

/**
 * @brief Base class for widgets that need to perform custom drawing operations.
 * 
 * This class provides a framework for widgets that need to draw custom content
 * rather than just compositing child widgets. Examples include:
 * - Spectrum analyzer visualizations
 * - Progress bars with custom gradients  
 * - Custom controls with specific rendering needs
 * - Background patterns or textures
 */
class DrawableWidget : public Widget {
public:
    /**
     * @brief Constructor for DrawableWidget.
     * @param width Widget width
     * @param height Widget height
     */
    DrawableWidget(int width, int height);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~DrawableWidget() = default;
    
    /**
     * @brief Marks the widget as needing to be redrawn.
     * Call this when the widget's visual state has changed.
     */
    void invalidate();
    
    /**
     * @brief Forces an immediate redraw of the widget.
     * This calls the draw() method and updates the surface.
     */
    void redraw();
    
    /**
     * @brief Overrides Widget::BlitTo to ensure surface is updated when needed.
     * @param target The target surface to blit to
     */
    virtual void BlitTo(Surface& target) override;

protected:
    /**
     * @brief Pure virtual method that subclasses must implement to draw their content.
     * @param surface The surface to draw on (guaranteed to be the correct size)
     * 
     * This method is called whenever the widget needs to be redrawn. Subclasses
     * should implement all their custom drawing logic here.
     */
    virtual void draw(Surface& surface) = 0;
    
    /**
     * @brief Called when the widget size changes. Default implementation calls invalidate().
     * @param new_width New widget width
     * @param new_height New widget height
     * 
     * Subclasses can override this to handle size changes specially (e.g., recalculating
     * layout parameters) but should call the base implementation to trigger a redraw.
     */
    virtual void onResize(int new_width, int new_height);

private:
    bool m_needs_redraw;
    
    /**
     * @brief Internal method to create and update the widget surface.
     */
    void updateSurface();
};

#endif // DRAWABLEWIDGET_H