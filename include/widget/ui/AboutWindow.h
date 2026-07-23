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

// The client area of the About dialog. Renders the shared about/version/
// copyright text (Core::about_message()); the license paragraphs are single
// logical lines that word-wrap (reflow) to the current width, so the dialog can
// be resized and the text re-flows. Content taller than the view scrolls with
// the mouse wheel (and a drawn scrollbar). An "Ok" button along the bottom
// dismisses it. Wrap it in a resizable WindowFrameWidget; drive re-flow from the
// frame's resize callback via onClientResized().
class AboutWindow : public PsyMP3::Widget::Foundation::DrawableWidget {
public:
    explicit AboutWindow(::Font* font);
    ~AboutWindow() override = default;

    // Invoked when the "Ok" button is pressed (wire this to close the dialog).
    void setOnOk(std::function<void()> cb) { m_on_ok = std::move(cb); }

    // Re-flow the text and re-lay-out the button/scroll for a new client size.
    // Call once after the frame is built and again from the frame's onResize.
    void onClientResized(int width, int height);

    bool handleMouseWheel(int delta, int relative_x, int relative_y) override;

protected:
    void draw(Surface& surface) override;

private:
    void reflow(int content_width);   // rebuild m_content for the given text width
    void layoutButton();
    int viewportHeight() const;       // visible text height (excludes padding + button strip)
    void clampScroll();

    ::Font* m_font;
    std::vector<std::string> m_source_lines; // paragraph-as-line source text
    std::unique_ptr<Surface> m_content;      // full rendered (tall) text surface
    int m_content_width = 0;
    int m_content_height = 0;
    int m_line_height = 0;
    int m_scroll = 0;                        // vertical scroll offset in px
    ButtonWidget* m_ok = nullptr;            // owned via addChild()
    std::function<void()> m_on_ok;

    static constexpr int kPad = 12;          // margin around the text block
    static constexpr int kButtonStrip = 36;  // reserved bottom area for the button
    static constexpr int kButtonW = 72;
    static constexpr int kButtonH = 22;
    static constexpr int kScrollStep = 28;   // px per wheel notch
    static constexpr int kScrollbarW = 6;
    static constexpr int kMinContentWidth = 596; // → ~620px window
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // ABOUTWINDOW_H
