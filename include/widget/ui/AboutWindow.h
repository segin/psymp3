/*
 * AboutWindow.h - In-app "About PsyMP3" dialog client widget.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ABOUTWINDOW_H
#define ABOUTWINDOW_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// The client area of the About dialog: renders the shared about/version/
// copyright text (Core::about_message()) as static, self-sized multi-line text.
// Wrap it in a WindowFrameWidget titled "About PsyMP3" to get a movable,
// closable window. It self-sizes to the text in the constructor.
class AboutWindow : public PsyMP3::Widget::Foundation::DrawableWidget {
public:
    explicit AboutWindow(::Font* font);
    ~AboutWindow() override = default;

protected:
    void draw(Surface& surface) override;

private:
    ::Font* m_font;
    // One pre-rendered surface per text line (empty entries mark blank lines).
    std::vector<std::unique_ptr<Surface>> m_lines;
    int m_line_height = 0;

    static constexpr int kPadding = 12; // px margin around the text block
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // ABOUTWINDOW_H
