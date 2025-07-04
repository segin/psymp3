/*
 * LyricsWidget.cpp - Implementation for synchronized lyrics display
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

#include "psymp3.h"

LyricsWidget::LyricsWidget(Font* font, int width)
    : Widget()
    , m_font(font)
    , m_lyrics(nullptr)
    , m_last_update_time(0)
    , m_widget_width(width)
    , m_needs_redraw(false)
{
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
    }
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
    
    // Get current lyric line
    const LyricLine* current_line = m_lyrics->getCurrentLine(current_time_ms);
    std::string new_current_text = current_line ? current_line->text : "";
    
    // Get upcoming lines for context
    auto upcoming_lines = m_lyrics->getUpcomingLines(current_time_ms, 3);
    std::vector<std::string> new_preview_lines;
    
    // Skip the current line and add the next ones
    for (size_t i = 1; i < upcoming_lines.size(); ++i) {
        if (!upcoming_lines[i]->text.empty()) {
            new_preview_lines.push_back(upcoming_lines[i]->text);
        }
    }
    
    // Check if we need to redraw
    if (new_current_text != m_current_line_text || new_preview_lines != m_preview_lines) {
        m_current_line_text = new_current_text;
        m_preview_lines = new_preview_lines;
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
    if (!hasLyrics() || m_current_line_text.empty()) {
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
        pos.y(20); // Position near top of screen
        setPos(pos);
    }
}

std::unique_ptr<Surface> LyricsWidget::createLyricsSurface()
{
    if (!m_font || (!hasLyrics() && m_current_line_text.empty())) {
        return std::make_unique<Surface>(1, 1, true);
    }
    
    const int LINE_SPACING = 6;
    const int PADDING = 12;
    
    // Render current line (highlighted)
    SDL_Color current_color = {255, 255, 255, 255}; // White
    auto current_surface = m_font->Render(TagLib::String(m_current_line_text, TagLib::String::UTF8), 
                                         current_color.r, current_color.g, current_color.b);
    
    if (!current_surface || !current_surface->isValid()) {
        return std::make_unique<Surface>(1, 1, true);
    }
    
    // Render preview lines (dimmed)
    SDL_Color preview_color = {180, 180, 180, 255}; // Light gray
    std::vector<std::unique_ptr<Surface>> preview_surfaces;
    
    for (const auto& line : m_preview_lines) {
        if (!line.empty()) {
            auto surface = m_font->Render(TagLib::String(line, TagLib::String::UTF8),
                                        preview_color.r, preview_color.g, preview_color.b);
            if (surface && surface->isValid()) {
                preview_surfaces.push_back(std::move(surface));
            }
        }
    }
    
    // Calculate total dimensions
    int max_width = current_surface->width();
    for (const auto& surface : preview_surfaces) {
        max_width = std::max(max_width, static_cast<int>(surface->width()));
    }
    
    int total_height = current_surface->height();
    for (const auto& surface : preview_surfaces) {
        total_height += surface->height() + LINE_SPACING;
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
    
    // Position and blit current line
    int y_offset = PADDING;
    int x_offset = (surface_width - current_surface->width()) / 2;
    
    Rect current_rect(x_offset, y_offset, 0, 0);
    current_surface->SetAlpha(SDL_SRCALPHA, 255);
    final_surface->Blit(*current_surface, current_rect);
    
    y_offset += current_surface->height() + LINE_SPACING;
    
    // Position and blit preview lines
    for (const auto& surface : preview_surfaces) {
        x_offset = (surface_width - surface->width()) / 2;
        Rect rect(x_offset, y_offset, 0, 0);
        surface->SetAlpha(SDL_SRCALPHA, 180); // More transparent for preview
        final_surface->Blit(*surface, rect);
        y_offset += surface->height() + LINE_SPACING;
    }
    
    return final_surface;
}

int LyricsWidget::calculateRequiredHeight()
{
    if (!m_font || m_current_line_text.empty()) {
        return 0;
    }
    
    const int LINE_SPACING = 6;
    const int PADDING = 12;
    
    // Estimate height based on font size and number of lines
    int font_height = 16; // Approximate font height
    int lines_to_show = 1 + std::min(static_cast<int>(m_preview_lines.size()), 2);
    
    return (font_height * lines_to_show) + (LINE_SPACING * (lines_to_show - 1)) + (PADDING * 2);
}