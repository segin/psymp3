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
    std::istringstream ss(std::string(PsyMP3::Core::about_message()));
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

    // "Ok" button along the bottom, centered; dismisses the dialog.
    auto btn = std::make_unique<ButtonWidget>(kButtonW, kButtonH);
    btn->setText("Ok", font);
    btn->setOnClick([this] { if (m_on_ok) m_on_ok(); });
    m_ok = btn.get();
    addChild(std::move(btn));

    // Initial size: min content width, natural (unclamped) content height. The
    // frame/showAboutWindow clamps this to the desktop and drives re-flow via
    // onClientResized().
    reflow(kMinContentWidth);
    const int width = kMinContentWidth + kPad * 2 + kScrollbarW + kScrollbarGap;
    const int height = m_content_height + kButtonStrip + kPad * 2;
    onResize(width, height);
    layoutButton();
    layoutScrollbar();
    syncScrollbar();
}

void AboutWindow::reflow(int content_width)
{
    m_content_width = std::max(1, content_width);
    m_line_height = 0;

    // Word-wrap each source line to the content width and render it.
    std::vector<std::unique_ptr<Surface>> rendered;
    for (const std::string& src : m_source_lines) {
        if (src.empty()) {
            rendered.push_back(nullptr); // paragraph spacer
            continue;
        }
        for (const std::string& wrapped : Label::wrapText(m_font, src, m_content_width)) {
            if (m_font && !wrapped.empty()) {
                // UTF-8 so "©" renders correctly (TagLib's std::string ctor is Latin-1).
                auto s = m_font->Render(TagLib::String(wrapped, TagLib::String::UTF8), 230, 230, 230);
                if (s && s->isValid()) {
                    m_line_height = std::max(m_line_height, static_cast<int>(s->height()));
                    rendered.push_back(std::move(s));
                    continue;
                }
            }
            rendered.push_back(nullptr);
        }
    }
    if (m_line_height <= 0) {
        m_line_height = 14;
    }
    m_content_height = static_cast<int>(rendered.size()) * m_line_height;

    // Compose the full (tall) content surface once; draw() blits a scrolled
    // slice of it each frame.
    m_content = std::make_unique<Surface>(m_content_width, std::max(1, m_content_height), true);
    m_content->FillRect(m_content->MapRGB(24, 24, 32));
    int y = 0;
    for (const auto& s : rendered) {
        if (s && s->isValid()) {
            m_content->Blit(*s, Rect(0, y, s->width(), s->height()));
        }
        y += m_line_height;
    }
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
    // Reserve the scrollbar column so text never runs under the bar.
    reflow(std::max(1, width - kPad * 2 - kScrollbarW - kScrollbarGap));
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
    surface.FillRect(surface.MapRGB(24, 24, 32));

    const int view_h = viewportHeight();
    const int view_w = m_content_width;

    // Blit a vertically-scrolled slice of the content into a temp surface (which
    // clips it to the text viewport, keeping it out of the button strip), then
    // onto the widget surface.
    if (m_content && view_h > 0 && view_w > 0) {
        auto slice = std::make_unique<Surface>(view_w, view_h, true);
        slice->FillRect(slice->MapRGB(24, 24, 32));
        slice->Blit(*m_content, Rect(0, -m_scroll, m_content_width, m_content_height));
        surface.Blit(*slice, Rect(kPad, kPad, view_w, view_h));
    }

    // The scrollbar is a standard ScrollbarWidget child, drawn on top of this
    // surface by the widget tree (see layoutScrollbar/syncScrollbar).

    // 1px border.
    surface.rectangle(0, 0, static_cast<int16_t>(pos.width() - 1),
                      static_cast<int16_t>(pos.height() - 1),
                      surface.MapRGB(80, 80, 96));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
