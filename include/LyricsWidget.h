/*
 * LyricsWidget.h - Widget for displaying synchronized lyrics
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

#ifndef LYRICSWIDGET_H
#define LYRICSWIDGET_H

#include <memory>
#include <vector>
#include <string>

// Forward declarations to avoid circular includes
class Widget;
class Font;
class LyricsFile;
class Surface;

/**
 * @brief A widget that displays synchronized lyrics at the top of the screen.
 * 
 * The widget shows the current lyric line prominently, with upcoming lines
 * displayed with reduced opacity for context. It automatically updates based
 * on playback position and hides when no lyrics are available.
 */
class LyricsWidget : public Widget
{
public:
    /**
     * @brief Constructs a LyricsWidget.
     * @param font Pointer to the font to use for rendering
     * @param width Width of the widget area
     */
    LyricsWidget(Font* font, int width);
    
    /**
     * @brief Sets the lyrics to display.
     * @param lyrics Shared pointer to the lyrics file
     */
    void setLyrics(std::shared_ptr<LyricsFile> lyrics);
    
    /**
     * @brief Updates the widget for the current playback position.
     * @param current_time_ms Current playback time in milliseconds
     */
    void updatePosition(unsigned int current_time_ms);
    
    /**
     * @brief Checks if the widget has lyrics to display.
     * @return true if lyrics are available and should be shown
     */
    bool hasLyrics() const;
    
    /**
     * @brief Clears current lyrics and hides the widget.
     */
    void clearLyrics();
    
    /**
     * @brief Custom blit method that handles transparency and positioning.
     * @param target Surface to blit onto
     */
    void BlitTo(Surface& target) override;

private:
    Font* m_font;                               ///< Non-owning pointer to font
    std::shared_ptr<LyricsFile> m_lyrics;       ///< Current lyrics file
    unsigned int m_last_update_time;            ///< Last update time to avoid unnecessary redraws
    int m_widget_width;                         ///< Width of the widget area
    
    // Display state
    std::string m_current_line_text;            ///< Currently displayed line text
    std::vector<std::string> m_preview_lines;  ///< Upcoming lines for context
    bool m_needs_redraw;                        ///< Whether the surface needs to be redrawn
    
    /**
     * @brief Rebuilds the widget surface with current lyrics.
     */
    void rebuildSurface();
    
    /**
     * @brief Creates a surface with the current and upcoming lyrics.
     * @return Unique pointer to the created surface
     */
    std::unique_ptr<Surface> createLyricsSurface();
    
    /**
     * @brief Calculates the height needed for the current lyrics display.
     * @return Required height in pixels
     */
    int calculateRequiredHeight();
};

#endif // LYRICSWIDGET_H