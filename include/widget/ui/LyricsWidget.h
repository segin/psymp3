/*
 * LyricsWidget.h - Widget for displaying synchronized lyrics
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

#ifndef LYRICSWIDGET_H
#define LYRICSWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Core {
class Font;
class Surface;
}
namespace Widget {
namespace UI {

using PsyMP3::Widget::Windowing::TransparentWindowWidget;

/**
 * @brief Displays synchronized lyrics at the top of the player window.
 *
 * The current line is shown in cyan, with up to two upcoming lines below in
 * gold. The widget hides itself when no lyrics are available.
 *
 * Composition follows the standard DrawableWidget contract: setLyrics()/
 * updatePosition() prepare per-line text surfaces and invalidate; the framework
 * then calls draw() to paint into a freshly-sized surface.
 */
class LyricsWidget : public TransparentWindowWidget
{
public:
    LyricsWidget(Core::Font* font, int max_width);

    void setLyrics(std::shared_ptr<::LyricsFile> lyrics);
    void updatePosition(unsigned int current_time_ms);
    bool hasLyrics() const;
    void clearLyrics();

    void BlitTo(Core::Surface& target) override;

protected:
    void draw(Core::Surface& surface) override;

private:
    Core::Font* m_font;
    std::shared_ptr<::LyricsFile> m_lyrics;
    unsigned int m_last_update_time;
    int m_max_width;

    std::string m_current_line_text;
    std::vector<std::string> m_preview_lines;
    std::vector<std::unique_ptr<Core::Surface>> m_line_surfaces;

    // Region painted on the target by the most recent visible blit. Used to
    // wipe leftover pixels when the widget hides, since the player's overlay
    // surface is not cleared between frames (see Player::updateState).
    int m_last_drawn_x = 0;
    int m_last_drawn_y = 0;
    int m_last_drawn_width = 0;
    int m_last_drawn_height = 0;
    bool m_has_last_drawn_area = false;

    bool updateDisplayedText(unsigned int current_time_ms);
    bool hasDisplayText() const;
    void rebuildLineCache();
    void updateGeometry();
    void clearLastDrawnArea(Core::Surface& target);
    std::unique_ptr<Core::Surface> renderLineFitted(const std::string& text,
                                                    SDL_Color color,
                                                    int max_inner_width);
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // LYRICSWIDGET_H
