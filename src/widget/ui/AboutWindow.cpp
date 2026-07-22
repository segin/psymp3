/*
 * AboutWindow.cpp - In-app "About PsyMP3" dialog client widget.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Widget {
namespace UI {

AboutWindow::AboutWindow(::Font* font)
    : DrawableWidget(1, 1) // resized to fit the text below
    , m_font(font)
{
    // Pre-render each line of the shared about text (Core::about_message()).
    // Rendering once here both measures the content (to size the window) and
    // caches the surfaces for draw(). Blank lines are kept as null entries so
    // paragraph spacing is preserved.
    int max_width = 0;
    int total_height = 0;

    std::istringstream ss(std::string(PsyMP3::Core::about_message()));
    std::string line;
    while (std::getline(ss, line, '\n')) {
        if (m_font && !line.empty()) {
            auto surface = m_font->Render(line, 230, 230, 230);
            if (surface && surface->isValid()) {
                max_width = std::max(max_width, static_cast<int>(surface->width()));
                m_line_height = std::max(m_line_height, static_cast<int>(surface->height()));
                m_lines.push_back(std::move(surface));
                continue;
            }
        }
        // Blank line (or a line that failed to render): keep a placeholder so
        // the vertical spacing matches the source text.
        m_lines.push_back(nullptr);
    }

    if (m_line_height <= 0) {
        m_line_height = 14; // sensible fallback if nothing rendered
    }
    total_height = static_cast<int>(m_lines.size()) * m_line_height;

    const int width = max_width + kPadding * 2;
    const int height = total_height + kPadding * 2;
    onResize(width, height); // sets the widget size and triggers the first draw()
}

void AboutWindow::draw(Surface& surface)
{
    // Dark background with a subtle 1px border, matching the app's dialogs.
    surface.FillRect(surface.MapRGB(24, 24, 32));
    const Rect pos = getPos();
    surface.rectangle(0, 0, static_cast<int16_t>(pos.width() - 1),
                      static_cast<int16_t>(pos.height() - 1),
                      surface.MapRGB(80, 80, 96));

    int y = kPadding;
    for (const auto& line : m_lines) {
        if (line && line->isValid()) {
            Rect dst(kPadding, y, line->width(), line->height());
            surface.Blit(*line, dst);
        }
        y += m_line_height;
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
