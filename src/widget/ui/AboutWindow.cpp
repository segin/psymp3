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
    : DrawableWidget(1, 1) // resized below / by the frame
    , m_font(font)
{
    // Split the shared about text into its source lines (license paragraphs are
    // already single logical lines; copyright lines stay separate). Blank lines
    // are kept as paragraph spacers.
    // Note: a named string avoids the most-vexing-parse that clang/libc++
    // rejects for `std::istringstream ss(std::string(about_message()))`.
    const std::string about_text = PsyMP3::Core::about_message();
    std::istringstream ss(about_text);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        m_source_lines.push_back(line);
    }

    // Standard vertical scrollbar (same widget ListView uses), driving the
    // pixel scroll offset from its 0..1 value.
    auto sb = std::make_unique<ScrollbarWidget>(kScrollbarW, 10, ScrollbarOrientation::Vertical);
    sb->setValue(0.0);
    sb->setOnChange([this](double value) {
        m_scroll = static_cast<int>(value * maxScroll() + 0.5);
        invalidate();
    });
    m_scrollbar = sb.get();
    addChild(std::move(sb));

    // "Ok" button along the bottom, centered; dismisses the dialog. It is the
    // dialog's default button (bold border; Enter activates it).
    auto btn = std::make_unique<ButtonWidget>(kButtonW, kButtonH);
    btn->setText("Ok", font);
    btn->setDefault(true);
    btn->setOnClick([this] { if (m_on_ok) m_on_ok(); });
    m_ok = btn.get();
    addChild(std::move(btn));

    // Initial size: min content width, and content height capped at
    // kInitialMaxHeight. The cap matters because the text now carries the full
    // third-party license set, whose natural height runs to tens of thousands of
    // pixels -- a widget surface that size would be allocated for nothing.
    // showAboutWindow() sets the real size and re-flows via onClientResized()
    // regardless; this is only a sane starting point.
    reflow(kMinContentWidth);
    const int width = kMinContentWidth + kPad * 2 + kScrollbarW + kScrollbarGap;
    const int height = std::min(m_content_height + kButtonStrip + kPad * 2,
                                kInitialMaxHeight);
    onResize(width, height);
    layoutButton();
    layoutScrollbar();
    syncScrollbar();
}

int AboutWindow::measureLineHeight()
{
    // One probe render rather than the max over every rendered line: the text is
    // far too long to render eagerly. The probe carries an ascender, a
    // descender, a diacritic and a full-height bar, so it is at least as tall as
    // any real line.
    if (m_font) {
        auto s = m_font->RenderLCD(TagLib::String("AÇgjÖ|", TagLib::String::UTF8),
                                   20, 20, 20, 240, 240, 240);
        if (s && s->isValid() && s->height() > 0) {
            return static_cast<int>(s->height());
        }
    }
    return 14;
}

Surface* AboutWindow::lineSurface(std::size_t index)
{
    auto it = m_line_cache.find(index);
    if (it != m_line_cache.end()) {
        return it->second.get();
    }
    if (m_line_cache.size() >= kLineCacheMax) {
        m_line_cache.clear(); // cheap and bounded; the viewport re-renders next draw
    }

    std::unique_ptr<Surface> rendered;
    const std::string& text = m_wrapped_lines[index];
    if (m_font && !text.empty()) {
        // Light mode: dark text, LCD-blended against the light background.
        // UTF-8 so "©" renders correctly (TagLib's std::string ctor is Latin-1).
        auto s = m_font->RenderLCD(TagLib::String(text, TagLib::String::UTF8),
                                   20, 20, 20, 240, 240, 240);
        if (s && s->isValid()) {
            rendered = std::move(s);
        }
    }
    // A null entry is cached too, so blank spacers and failed renders are not
    // retried on every draw.
    return m_line_cache.emplace(index, std::move(rendered)).first->second.get();
}

void AboutWindow::reflow(int content_width)
{
    m_content_width = std::max(1, content_width);
    m_line_height = measureLineHeight();

    // Word-wrap only; the glyphs are rendered lazily by lineSurface(). Empty
    // source lines are kept as paragraph spacers.
    m_line_cache.clear();
    m_wrapped_lines.clear();
    for (const std::string& src : m_source_lines) {
        if (src.empty()) {
            m_wrapped_lines.emplace_back();
            continue;
        }
        for (std::string& wrapped : Label::wrapText(m_font, src, m_content_width)) {
            m_wrapped_lines.push_back(std::move(wrapped));
        }
    }
    m_content_height = static_cast<int>(m_wrapped_lines.size()) * m_line_height;
}

int AboutWindow::viewportHeight() const
{
    return std::max(0, getPos().height() - kPad * 2 - kButtonStrip);
}

int AboutWindow::maxScroll() const
{
    return std::max(0, m_content_height - viewportHeight());
}

void AboutWindow::clampScroll()
{
    m_scroll = std::max(0, std::min(m_scroll, maxScroll()));
}

void AboutWindow::layoutButton()
{
    if (!m_ok) {
        return;
    }
    const Rect p = getPos();
    const int x = (p.width() - kButtonW) / 2;
    const int y = p.height() - kButtonStrip + (kButtonStrip - kButtonH) / 2;
    m_ok->setGeometry(Rect(x, y, kButtonW, kButtonH));
}

void AboutWindow::layoutScrollbar()
{
    if (!m_scrollbar) {
        return;
    }
    const Rect p = getPos();
    m_scrollbar->setGeometry(Rect(p.width() - kPad - kScrollbarW, kPad,
                                  kScrollbarW, viewportHeight()));
}

void AboutWindow::syncScrollbar()
{
    if (!m_scrollbar) {
        return;
    }
    const int ms = maxScroll();
    m_scrollbar->setEnabled(ms > 0);
    m_scrollbar->setValue(ms > 0 ? static_cast<double>(m_scroll) / ms : 0.0);
    const int vh = viewportHeight();
    if (ms > 0) {
        m_scrollbar->setSteps(static_cast<double>(kScrollStep) / ms,
                              vh > 0 ? static_cast<double>(vh) / ms : 0.2);
    }
}

void AboutWindow::onClientResized(int width, int height)
{
    setPos(Rect(getPos().x(), getPos().y(), width, height));
    // Only re-wrap/re-render the text when the WIDTH changes — a vertical-only
    // resize (or a repeated width during a drag) reuses the cached content
    // surface, so resizing no longer re-renders every glyph each mouse-move.
    const int content_w = std::max(1, width - kPad * 2 - kScrollbarW - kScrollbarGap);
    if (content_w != m_content_width) {
        reflow(content_w);
    }
    layoutButton();
    layoutScrollbar();
    clampScroll();
    syncScrollbar();
    invalidate();
}

bool AboutWindow::handleMouseWheel(int delta, int /*relative_x*/, int /*relative_y*/)
{
    // Wheel up (positive delta) scrolls the content up (toward the top).
    m_scroll -= delta * kScrollStep;
    clampScroll();
    syncScrollbar(); // keep the scrollbar thumb in step with the wheel
    invalidate();
    return true;
}

void AboutWindow::draw(Surface& surface)
{
    const Rect pos = getPos();
    surface.FillRect(surface.MapRGB(240, 240, 240));

    const int view_h = viewportHeight();
    const int view_w = m_content_width;

    // Render and blit only the lines the viewport actually shows. They go into a
    // temp surface first, which clips them to the text viewport and so keeps
    // them out of the button strip; then that goes onto the widget surface.
    if (view_h > 0 && view_w > 0 && m_line_height > 0 && !m_wrapped_lines.empty()) {
        auto slice = std::make_unique<Surface>(view_w, view_h, true);
        slice->FillRect(slice->MapRGB(240, 240, 240));

        // Half-visible lines at both edges are included; the slice clips them.
        const std::size_t first = static_cast<std::size_t>(
            std::max(0, m_scroll) / m_line_height);
        const std::size_t last = std::min(
            m_wrapped_lines.size(),
            static_cast<std::size_t>((std::max(0, m_scroll) + view_h) / m_line_height) + 1);

        for (std::size_t i = first; i < last; ++i) {
            Surface* s = lineSurface(i);
            if (!s || !s->isValid()) {
                continue; // blank spacer
            }
            const int y = static_cast<int>(i) * m_line_height - m_scroll;
            slice->Blit(*s, Rect(0, y, s->width(), s->height()));
        }
        surface.Blit(*slice, Rect(kPad, kPad, view_w, view_h));
    }

    // The scrollbar is a standard ScrollbarWidget child, drawn on top of this
    // surface by the widget tree (see layoutScrollbar/syncScrollbar).

    // 1px border.
    surface.rectangle(0, 0, static_cast<int16_t>(pos.width() - 1),
                      static_cast<int16_t>(pos.height() - 1),
                      surface.MapRGB(140, 140, 150));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
