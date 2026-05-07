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

namespace {

void drawRoundedCorner(::Surface& surface, int cx, int cy, int radius,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a, int corner)
{
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }

            int px = cx;
            int py = cy;
            bool draw = false;

            switch (corner) {
                case 0:
                    if (dx <= 0 && dy <= 0) { px = cx + dx; py = cy + dy; draw = true; }
                    break;
                case 1:
                    if (dx >= 0 && dy <= 0) { px = cx + dx; py = cy + dy; draw = true; }
                    break;
                case 2:
                    if (dx <= 0 && dy >= 0) { px = cx + dx; py = cy + dy; draw = true; }
                    break;
                case 3:
                    if (dx >= 0 && dy >= 0) { px = cx + dx; py = cy + dy; draw = true; }
                    break;
            }

            if (draw && px >= 0 && px < surface.width() && py >= 0 && py < surface.height()) {
                surface.pixel(px, py, r, g, b, a);
            }
        }
    }
}

void drawToastStyleRoundedRect(::Surface& surface, int x, int y, int width, int height, int radius,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    const int max_radius = std::min(width / 2, height / 2);
    radius = std::min(radius, max_radius);

    if (radius <= 0) {
        surface.box(x, y, x + width - 1, y + height - 1, r, g, b, a);
        return;
    }

    if (height > 2 * radius && width > 2 * radius) {
        surface.box(x + radius, y, x + width - radius - 1, y + height - 1, r, g, b, a);
        surface.box(x, y + radius, x + radius - 1, y + height - radius - 1, r, g, b, a);
        surface.box(x + width - radius, y + radius, x + width - 1, y + height - radius - 1, r, g, b, a);
    }

    drawRoundedCorner(surface, x + radius, y + radius, radius, r, g, b, a, 0);
    drawRoundedCorner(surface, x + width - radius - 1, y + radius, radius, r, g, b, a, 1);
    drawRoundedCorner(surface, x + radius, y + height - radius - 1, radius, r, g, b, a, 2);
    drawRoundedCorner(surface, x + width - radius - 1, y + height - radius - 1, radius, r, g, b, a, 3);
}

void applyRelativeOpacity(::Surface& surface, float opacity)
{
    SDL_Surface* sdl_surface = surface.getHandle();
    if (!sdl_surface || !sdl_surface->pixels) {
        return;
    }

    const bool must_lock = SDL_MUSTLOCK(sdl_surface);
    if (must_lock && SDL_LockSurface(sdl_surface) != 0) {
        return;
    }

    for (int y = 0; y < surface.height(); y++) {
        auto* row = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(sdl_surface->pixels) + y * sdl_surface->pitch);
        for (int x = 0; x < surface.width(); x++) {
            uint32_t& pixel = row[x];
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, sdl_surface->format, &r, &g, &b, &a);
            if (a > 0) {
                a = static_cast<uint8_t>(a * opacity);
            }
            pixel = SDL_MapRGBA(sdl_surface->format, r, g, b, a);
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(sdl_surface);
    }
}

} // namespace

LyricsWidget::LyricsWidget(Font* font, int width)
    : TransparentWindowWidget(width, 100, 0.85f, true)  // Match toast opacity, mouse-transparent
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
    Rect pos = getPos();
    m_last_drawn_width = pos.width();
    m_last_drawn_height = pos.height();
    m_has_last_drawn_area = (m_last_drawn_width > 0 && m_last_drawn_height > 0);

    m_lyrics.reset();
    m_current_line_text.clear();
    m_preview_lines.clear();
    m_needs_redraw = false;
    
    // Create a minimal empty surface
    setSurface(std::make_unique<Surface>(1, 1, true));
    
    // Set position to indicate no lyrics
    pos.width(0);
    pos.height(0);
    setPos(pos);
}

void LyricsWidget::BlitTo(Surface& target)
{
    if (!hasLyrics() || !hasDisplayText()) {
        clearLastDrawnArea(target);
        return; // Don't draw anything if no lyrics
    }

    Widget::BlitTo(target);

    Rect pos = getPos();
    m_last_drawn_width = pos.width();
    m_last_drawn_height = pos.height();
    m_has_last_drawn_area = true;
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

    auto render_line = [&](const std::string& text, SDL_Color color) {
        if (text.empty()) {
            return;
        }

        auto surface = m_font->Render(TagLib::String(text, TagLib::String::UTF8),
                                      color.r, color.g, color.b);
        if (surface && surface->isValid()) {
            line_surfaces.push_back(std::move(surface));
        }
    };

    render_line(m_current_line_text, SDL_Color{0, 192, 192, 255});
    for (const auto& line : m_preview_lines) {
        render_line(line, SDL_Color{255, 215, 0, 255});
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

    // Enable per-pixel alpha blending on blit so transparent pixels stay
    // transparent instead of being copied as solid black RGB. Without this,
    // SDL_BLENDMODE_NONE (the default for SDL_CreateRGBSurface) ignores alpha
    // entirely. ToastWidget::draw() does the same for the same reason.
    if (final_surface->getHandle()) {
        SDL_SetSurfaceBlendMode(final_surface->getHandle(), SDL_BLENDMODE_BLEND);
    }

    final_surface->FillRect(final_surface->MapRGBA(0, 0, 0, 0));
    drawToastStyleRoundedRect(*final_surface, 0, 0, surface_width, surface_height, 8, 100, 100, 100, 255);
    if (surface_width > 4 && surface_height > 4) {
        drawToastStyleRoundedRect(*final_surface, 1, 1, surface_width - 2, surface_height - 2, 7, 50, 50, 50, 255);
    }

    int y_offset = PADDING;

    for (size_t i = 0; i < line_surfaces.size(); ++i) {
        auto& surface = line_surfaces[i];
        int x_offset = (surface_width - surface->width()) / 2;
        Rect rect(x_offset, y_offset, 0, 0);
        final_surface->Blit(*surface, rect);
        y_offset += surface->height();
        if (i + 1 < line_surfaces.size()) {
            y_offset += LINE_SPACING;
        }
    }

    // Apply opacity after blitting text, mirroring ToastWidget::draw() so that
    // background and text are dimmed together rather than text landing at full alpha.
    applyRelativeOpacity(*final_surface, getOpacity());

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

void LyricsWidget::clearLastDrawnArea(Surface& target)
{
    if (!m_has_last_drawn_area || m_last_drawn_width <= 0 || m_last_drawn_height <= 0) {
        return;
    }

    Rect pos = getPos();
    int clear_x = pos.x();
    int clear_y = pos.y();

    if (pos.width() == 0 || pos.height() == 0) {
        clear_x = (m_widget_width - m_last_drawn_width) / 2;
        clear_y = 50;
    }

    target.box(clear_x, clear_y,
               clear_x + m_last_drawn_width - 1,
               clear_y + m_last_drawn_height - 1,
               target.MapRGB(0, 0, 0));
    m_has_last_drawn_area = false;
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
