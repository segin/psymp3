/*
 * ProgressBarWidget.h - Progress bar widget with rainbow fill and seek functionality
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

#ifndef PLAYERPROGRESSBARWIDGET_H
#define PLAYERPROGRESSBARWIDGET_H

#include "Widget.h"
#include <functional>

/**
 * @brief A progress bar widget with rainbow fill and seek functionality.
 * 
 * This widget displays a progress bar with a rainbow color gradient fill
 * and supports click-and-drag seeking functionality.
 */
class PlayerProgressBarWidget : public Widget {
public:
    /**
     * @brief Constructor for PlayerProgressBarWidget.
     * @param width Width of the progress bar
     * @param height Height of the progress bar
     */
    PlayerProgressBarWidget(int width = 220, int height = 15);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~PlayerProgressBarWidget() = default;
    
    /**
     * @brief Handles mouse button down events for seeking.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Handles mouse motion events for drag seeking.
     * @param event The mouse motion event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Handles mouse button up events to finalize seeking.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Sets the current progress (0.0 to 1.0).
     * @param progress Progress value between 0.0 and 1.0
     */
    void setProgress(double progress);
    
    /**
     * @brief Gets the current progress.
     * @return Progress value between 0.0 and 1.0
     */
    double getProgress() const { return m_progress; }
    
    /**
     * @brief Sets the drag progress (0.0 to 1.0) for visual feedback.
     * @param progress Progress value between 0.0 and 1.0
     */
    void setDragProgress(double progress);
    
    /**
     * @brief Gets whether the progress bar is currently being dragged.
     * @return true if dragging is in progress
     */
    bool isDragging() const { return m_is_dragging; }
    
    /**
     * @brief Sets the seek start callback.
     * @param callback Function to call when seeking starts
     */
    void setOnSeekStart(std::function<void(double progress)> callback) { m_on_seek_start = callback; }
    
    /**
     * @brief Sets the seek update callback.
     * @param callback Function to call during seeking
     */
    void setOnSeekUpdate(std::function<void(double progress)> callback) { m_on_seek_update = callback; }
    
    /**
     * @brief Sets the seek end callback.
     * @param callback Function to call when seeking ends
     */
    void setOnSeekEnd(std::function<void(double progress)> callback) { m_on_seek_end = callback; }

private:
    int m_width;
    int m_height;
    double m_progress;          // Actual progress (0.0 to 1.0)
    double m_drag_progress;     // Visual progress during drag (0.0 to 1.0)
    bool m_is_dragging;
    
    // Seek callbacks
    std::function<void(double progress)> m_on_seek_start;
    std::function<void(double progress)> m_on_seek_update;
    std::function<void(double progress)> m_on_seek_end;
    
    /**
     * @brief Rebuilds the progress bar surface.
     */
    void rebuildSurface();
    
    /**
     * @brief Draws the progress bar frame (six lines).
     * @param surface Surface to draw on
     */
    void drawFrame(Surface& surface) const;
    
    /**
     * @brief Draws the rainbow progress fill.
     * @param surface Surface to draw on
     * @param progress Progress value (0.0 to 1.0)
     */
    void drawRainbowFill(Surface& surface, double progress) const;
    
    /**
     * @brief Converts screen coordinates to progress value.
     * @param x X coordinate relative to widget
     * @return Progress value (0.0 to 1.0)
     */
    double coordinateToProgress(int x) const;
};

#endif // PLAYERPROGRESSBARWIDGET_H