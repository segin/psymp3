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
    void reflow(int content_width);   // re-wrap m_source_lines for the given text width
    void layoutButton();
    void layoutScrollbar();
    void syncScrollbar();             // push value/enabled/steps to the scrollbar
    int viewportHeight() const;       // visible text height (excludes padding + button strip)
    int maxScroll() const;
    void clampScroll();
    int measureLineHeight();          // one probe render, covers ascender+descender
    /// Rendered glyphs for wrapped line @p index, or nullptr for a blank spacer.
    /// Renders on first use and memoises; the cache is dropped wholesale once it
    /// outgrows kLineCacheMax.
    Surface* lineSurface(std::size_t index);

    ::Font* m_font;
    std::vector<std::string> m_source_lines;  // paragraph-as-line source text
    std::vector<std::string> m_wrapped_lines; // m_source_lines wrapped to m_content_width
    // Rendered lines, built on demand. The About text carries the full
    // third-party license set (~1,500 lines), so rendering every line up front
    // -- and compositing one tall surface -- would cost ~50 MB and a visible
    // stall, repeated on every width change during a resize drag. Only the
    // lines actually on screen are ever rendered.
    std::unordered_map<std::size_t, std::unique_ptr<Surface>> m_line_cache;
    int m_content_width = 0;
    int m_content_height = 0;
    int m_line_height = 0;
    int m_scroll = 0;                        // vertical scroll offset in px
    ButtonWidget* m_ok = nullptr;            // owned via addChild()
    ScrollbarWidget* m_scrollbar = nullptr;  // owned via addChild()
    std::function<void()> m_on_ok;

    static constexpr int kPad = 12;          // margin around the text block
    static constexpr int kButtonStrip = 36;  // reserved bottom area for the button
    static constexpr int kButtonW = 72;
    static constexpr int kButtonH = 24;
    static constexpr int kScrollStep = 28;   // px per wheel notch
    static constexpr int kScrollbarW = 17;   // matches ListViewWidget::SCROLLBAR_WIDTH
    static constexpr int kScrollbarGap = 2;  // gap between text and scrollbar
    static constexpr int kMinContentWidth = 596; // → ~620px window
    // Rendered lines kept before the cache is dropped. Generous next to a
    // viewport (a few dozen lines) but far below the full text, so scrolling
    // end to end cannot accumulate unbounded surfaces.
    static constexpr std::size_t kLineCacheMax = 512;
    // Cap on the height the constructor asks for before showAboutWindow() sets
    // the real one. Without it the full license text would size the widget to
    // tens of thousands of pixels.
    static constexpr int kInitialMaxHeight = 600;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // ABOUTWINDOW_H
