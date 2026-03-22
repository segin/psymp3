/*
 * LyricsWidget.cpp - Implementation for synchronized lyrics display
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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
namespace Widget {
namespace UI {

using Foundation::Widget;
using Foundation::DrawableWidget;

LyricsWidget::LyricsWidget(Font* font, int width)
    : TransparentWindowWidget(width, 100, 0.9f, true)  // width, height, opacity, mouse-transparent
    , m_font(font)
    , m_lyrics(nullptr)
    , m_last_update_time(0)
    , m_widget_width(width)
    , m_needs_redraw(false)
{
    // Set Z-order to UI level (just above desktop elements)
    setZOrder(ZOrder::UI);
    
    // Start with an empty widget
    clearLyrics();
}

void LyricsWidget::setLyrics(std::shared_ptr<LyricsFile> lyrics)
{
    m_lyrics = lyrics;
    m_last_update_time = 0; // Force update on next position update
    m_needs_redraw = true;
    
    if (!hasLyrics()) {
        clearLyrics();
        return;
    }

    updateDisplayedText(0);
    rebuildSurface();
    m_needs_redraw = false;
}

void LyricsWidget::updatePosition(unsigned int current_time_ms)
{
    if (!hasLyrics()) {
        return;
    }
    
    // Avoid unnecessary updates (update every 100ms at most)
    if (current_time_ms - m_last_update_time < 100) {
        return;
    }
    
    m_last_update_time = current_time_ms;

    if (updateDisplayedText(current_time_ms)) {
        m_needs_redraw = true;
    }
    
    if (m_needs_redraw) {
        rebuildSurface();
        m_needs_redraw = false;
    }
}

bool LyricsWidget::hasLyrics() const
{
    return m_lyrics && m_lyrics->hasLyrics();
}

void LyricsWidget::clearLyrics()
{
    m_lyrics.reset();
    m_current_line_text.clear();
    m_preview_lines.clear();
    m_needs_redraw = false;
    
    // Create a minimal empty surface
    setSurface(std::make_unique<Surface>(1, 1, true));
    
    // Set position to indicate no lyrics
    Rect pos = getPos();
    pos.width(0);
    pos.height(0);
    setPos(pos);
}

void LyricsWidget::BlitTo(Surface& target)
{
    if (!hasLyrics() || !hasDisplayText()) {
        return; // Don't draw anything if no lyrics
    }
    
    Widget::BlitTo(target);
}

void LyricsWidget::rebuildSurface()
{
    if (!hasLyrics()) {
        clearLyrics();
        return;
    }
    
    auto lyrics_surface = createLyricsSurface();
    if (lyrics_surface) {
        setSurface(std::move(lyrics_surface));
        
        // Update widget position
        Rect pos = getPos();
        pos.width(getSurface().width());
        pos.height(getSurface().height());
        pos.x((m_widget_width - pos.width()) / 2); // Center horizontally
        pos.y(50); // Position 50px from top of screen
        setPos(pos);
    }
}

std::unique_ptr<::Surface> LyricsWidget::createLyricsSurface()
{
    if (!m_font || !hasLyrics() || !hasDisplayText()) {
        return std::make_unique<::Surface>(1, 1, true);
    }
    
    const int LINE_SPACING = 6;
    const int PADDING = 12;

    std::vector<std::unique_ptr<Surface>> line_surfaces;
    std::vector<uint8_t> line_alphas;

    auto render_line = [&](const std::string& text, SDL_Color color, uint8_t alpha) {
        if (text.empty()) {
            return;
        }

        auto surface = m_font->Render(TagLib::String(text, TagLib::String::UTF8),
                                      color.r, color.g, color.b);
        if (surface && surface->isValid()) {
            line_alphas.push_back(alpha);
            line_surfaces.push_back(std::move(surface));
        }
    };

    render_line(m_current_line_text, SDL_Color{255, 255, 255, 255}, 255);
    for (const auto& line : m_preview_lines) {
        render_line(line, SDL_Color{180, 180, 180, 255}, 180);
    }

    if (line_surfaces.empty()) {
        return std::make_unique<Surface>(1, 1, true);
    }

    int max_width = 0;
    int total_height = 0;
    for (size_t i = 0; i < line_surfaces.size(); ++i) {
        max_width = std::max(max_width, static_cast<int>(line_surfaces[i]->width()));
        total_height += static_cast<int>(line_surfaces[i]->height());
        if (i + 1 < line_surfaces.size()) {
            total_height += LINE_SPACING;
        }
    }
    
    // Add padding
    int surface_width = max_width + (PADDING * 2);
    int surface_height = total_height + (PADDING * 2);
    
    // Limit width to available space
    surface_width = std::min(surface_width, m_widget_width - 40);
    
    // Create the final surface with alpha support
    auto final_surface = std::make_unique<Surface>(surface_width, surface_height, true);
    
    // Draw semi-transparent background
    final_surface->roundedBoxRGBA(0, 0, surface_width - 1, surface_height - 1, 8, 0, 0, 0, 160);
    
    int y_offset = PADDING;

    for (size_t i = 0; i < line_surfaces.size(); ++i) {
        auto& surface = line_surfaces[i];
        int x_offset = (surface_width - surface->width()) / 2;
        Rect rect(x_offset, y_offset, 0, 0);
        surface->SetAlpha(SDL_SRCALPHA, line_alphas[i]);
        final_surface->Blit(*surface, rect);
        y_offset += surface->height();
        if (i + 1 < line_surfaces.size()) {
            y_offset += LINE_SPACING;
        }
    }
    
    return final_surface;
}

int LyricsWidget::calculateRequiredHeight()
{
    if (!m_font || !hasDisplayText()) {
        return 0;
    }
    
    const int LINE_SPACING = 6;
    const int PADDING = 12;
    
    // Estimate height based on font size and number of lines
    int font_height = 16; // Approximate font height
    int lines_to_show = 1 + std::min(static_cast<int>(m_preview_lines.size()), 2);
    
    return (font_height * lines_to_show) + (LINE_SPACING * (lines_to_show - 1)) + (PADDING * 2);
}

bool LyricsWidget::updateDisplayedText(unsigned int current_time_ms)
{
    std::string new_current_text;
    std::vector<std::string> new_preview_lines;

    const LyricLine* current_line = m_lyrics->getCurrentLine(current_time_ms);
    if (current_line && !current_line->text.empty()) {
        new_current_text = current_line->text;

        auto upcoming_lines = m_lyrics->getUpcomingLines(current_time_ms, 3);
        for (size_t i = 1; i < upcoming_lines.size(); ++i) {
            if (!upcoming_lines[i]->text.empty()) {
                new_preview_lines.push_back(upcoming_lines[i]->text);
            }
        }
    } else {
        // Before the first synced timestamp, show the first few non-empty lines as preview
        // so the widget becomes visible instead of waiting for the first active lyric line.
        for (const auto& line : m_lyrics->getLines()) {
            if (!line.text.empty()) {
                new_preview_lines.push_back(line.text);
            }
            if (new_preview_lines.size() == 3) {
                break;
            }
        }
    }

    if (new_current_text == m_current_line_text && new_preview_lines == m_preview_lines) {
        return false;
    }

    m_current_line_text = std::move(new_current_text);
    m_preview_lines = std::move(new_preview_lines);
    return true;
}

bool LyricsWidget::hasDisplayText() const
{
    if (!m_current_line_text.empty()) {
        return true;
    }

    return std::any_of(m_preview_lines.begin(), m_preview_lines.end(),
                       [](const std::string& line) { return !line.empty(); });
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
