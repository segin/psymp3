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

using Foundation::DrawableWidget;

namespace {

constexpr int LINE_SPACING = 6;
constexpr int PADDING = 12;
constexpr int OUTER_MARGIN = 20;
constexpr int CORNER_RADIUS = 8;
constexpr int TOP_OFFSET = 50;

void drawRoundedCorner(::Surface& surface, int cx, int cy, int radius,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a, int corner)
{
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }

            bool draw = false;
            switch (corner) {
                case 0: draw = (dx <= 0 && dy <= 0); break;
                case 1: draw = (dx >= 0 && dy <= 0); break;
                case 2: draw = (dx <= 0 && dy >= 0); break;
                case 3: draw = (dx >= 0 && dy >= 0); break;
            }

            if (draw) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < surface.width() && py >= 0 && py < surface.height()) {
                    surface.pixel(px, py, r, g, b, a);
                }
            }
        }
    }
}

void paintRoundedRectRGBA(::Surface& surface, int x, int y, int width, int height, int radius,
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

} // namespace

LyricsWidget::LyricsWidget(Font* font, int max_width)
    : TransparentWindowWidget(max_width, 1, 0.85f, true)
    , m_font(font)
    , m_lyrics(nullptr)
    , m_last_update_time(0)
    , m_max_width(max_width)
{
    setZOrder(ZOrder::UI);
    clearLyrics();
}

void LyricsWidget::setLyrics(std::shared_ptr<LyricsFile> lyrics)
{
    if (!lyrics || !lyrics->hasLyrics()) {
        clearLyrics();
        return;
    }

    m_lyrics = lyrics;
    m_last_update_time = 0;
    updateDisplayedText(0);
    rebuildLineCache();
    updateGeometry();
}

void LyricsWidget::updatePosition(unsigned int current_time_ms)
{
    if (!hasLyrics()) {
        return;
    }

    // Throttle to ~10Hz. Cast through int64_t so a backward seek (which would
    // otherwise wrap unsigned subtraction to a huge value) still triggers an
    // update on the next call rather than waiting out the throttle window.
    const int64_t delta = static_cast<int64_t>(current_time_ms)
                        - static_cast<int64_t>(m_last_update_time);
    if (delta >= 0 && delta < 100) {
        return;
    }

    m_last_update_time = current_time_ms;

    if (updateDisplayedText(current_time_ms)) {
        rebuildLineCache();
        updateGeometry();
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
    m_line_surfaces.clear();
    m_last_update_time = 0;

    Rect pos = getPos();
    pos.width(0);
    pos.height(0);
    setPos(pos);
}

void LyricsWidget::BlitTo(Surface& target)
{
    if (!hasLyrics() || !hasDisplayText() || m_line_surfaces.empty()) {
        clearLastDrawnArea(target);
        return;
    }

    DrawableWidget::BlitTo(target);

    Rect pos = getPos();
    m_last_drawn_x = pos.x();
    m_last_drawn_y = pos.y();
    m_last_drawn_width = pos.width();
    m_last_drawn_height = pos.height();
    m_has_last_drawn_area = (m_last_drawn_width > 0 && m_last_drawn_height > 0);
}

void LyricsWidget::draw(Surface& surface)
{
    if (m_line_surfaces.empty()) {
        return;
    }

    const int sw = surface.width();
    const int sh = surface.height();

    // SDL_CreateRGBSurface defaults to BLENDMODE_NONE on the source, which
    // would copy transparent pixels as solid black during the parent blit.
    if (surface.getHandle()) {
        SDL_SetSurfaceBlendMode(surface.getHandle(), SDL_BLENDMODE_BLEND);
    }

    surface.FillRect(surface.MapRGBA(0, 0, 0, 0));
    paintRoundedRectRGBA(surface, 0, 0, sw, sh, CORNER_RADIUS, 100, 100, 100, 255);
    if (sw > 4 && sh > 4) {
        paintRoundedRectRGBA(surface, 1, 1, sw - 2, sh - 2, CORNER_RADIUS - 1, 50, 50, 50, 255);
    }

    int y = PADDING;
    for (size_t i = 0; i < m_line_surfaces.size(); ++i) {
        auto& line = m_line_surfaces[i];
        const int x = (sw - static_cast<int>(line->width())) / 2;
        Rect rect(x, y, 0, 0);
        surface.Blit(*line, rect);
        y += static_cast<int>(line->height());
        if (i + 1 < m_line_surfaces.size()) {
            y += LINE_SPACING;
        }
    }

    surface.applyRelativeOpacity(getOpacity());
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
        // Before the first synced timestamp, surface a few preview lines so
        // the widget appears immediately instead of waiting for the first hit.
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

void LyricsWidget::rebuildLineCache()
{
    m_line_surfaces.clear();
    if (!m_font) {
        return;
    }

    const int max_inner_width = std::max(1, m_max_width - OUTER_MARGIN * 2 - PADDING * 2);

    auto add = [&](const std::string& text, SDL_Color color) {
        if (text.empty()) {
            return;
        }
        auto surface = renderLineFitted(text, color, max_inner_width);
        if (surface && surface->isValid()) {
            m_line_surfaces.push_back(std::move(surface));
        }
    };

    add(m_current_line_text, SDL_Color{0, 192, 192, 255});
    for (const auto& line : m_preview_lines) {
        add(line, SDL_Color{255, 215, 0, 255});
    }
}

void LyricsWidget::updateGeometry()
{
    if (m_line_surfaces.empty()) {
        Rect pos = getPos();
        pos.width(0);
        pos.height(0);
        setPos(pos);
        return;
    }

    int max_width = 0;
    int total_height = 0;
    for (size_t i = 0; i < m_line_surfaces.size(); ++i) {
        max_width = std::max(max_width, static_cast<int>(m_line_surfaces[i]->width()));
        total_height += static_cast<int>(m_line_surfaces[i]->height());
        if (i + 1 < m_line_surfaces.size()) {
            total_height += LINE_SPACING;
        }
    }

    int surface_width = std::min(max_width + PADDING * 2, m_max_width - OUTER_MARGIN * 2);
    surface_width = std::max(surface_width, PADDING * 2 + 1);
    const int surface_height = total_height + PADDING * 2;

    Rect pos = getPos();
    pos.x((m_max_width - surface_width) / 2);
    pos.y(TOP_OFFSET);
    pos.width(surface_width);
    pos.height(surface_height);
    setPos(pos);

    // Tells DrawableWidget that pos changed and the surface is stale.
    onResize(surface_width, surface_height);
}

void LyricsWidget::clearLastDrawnArea(Surface& target)
{
    if (!m_has_last_drawn_area) {
        return;
    }

    target.box(static_cast<int16_t>(m_last_drawn_x),
               static_cast<int16_t>(m_last_drawn_y),
               static_cast<int16_t>(m_last_drawn_x + m_last_drawn_width - 1),
               static_cast<int16_t>(m_last_drawn_y + m_last_drawn_height - 1),
               target.MapRGB(0, 0, 0));
    m_has_last_drawn_area = false;
}

std::unique_ptr<Surface> LyricsWidget::renderLineFitted(const std::string& text,
                                                       SDL_Color color,
                                                       int max_inner_width)
{
    auto render = [&](const std::string& s) {
        return m_font->Render(TagLib::String(s, TagLib::String::UTF8),
                              color.r, color.g, color.b);
    };

    auto surface = render(text);
    if (!surface || !surface->isValid()
        || static_cast<int>(surface->width()) <= max_inner_width) {
        return surface;
    }

    // Truncate trailing UTF-8 codepoints until "<prefix>…" fits, so a wide
    // line gets a visible ellipsis instead of being silently cropped.
    static constexpr char ellipsis[] = "\xE2\x80\xA6"; // U+2026
    std::string truncated = text;
    while (!truncated.empty()) {
        size_t i = truncated.size();
        do { --i; } while (i > 0 && (static_cast<unsigned char>(truncated[i]) & 0xC0) == 0x80);
        truncated.erase(i);
        if (truncated.empty()) {
            break;
        }
        auto candidate = render(truncated + ellipsis);
        if (candidate && candidate->isValid()
            && static_cast<int>(candidate->width()) <= max_inner_width) {
            return candidate;
        }
    }
    return surface;
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
