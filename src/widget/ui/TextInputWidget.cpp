/*
 * TextInputWidget.cpp - Windows 3.1 style single-line text input widget
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

TextInputWidget* TextInputWidget::s_focused_widget = nullptr;

namespace {

#ifndef PSYMP3_DRAW_WIN31_SUNKEN_FRAME
#define PSYMP3_DRAW_WIN31_SUNKEN_FRAME
void drawWin31SunkenFrame(::Surface& surface, int x, int y, int width, int height)
{
    surface.box(x, y, x + width - 1, y + height - 1, 255, 255, 255, 255);
    surface.hline(x, x + width - 1, y, 128, 128, 128, 255);
    surface.vline(x, y, y + height - 1, 128, 128, 128, 255);
    surface.hline(x, x + width - 1, y + height - 1, 255, 255, 255, 255);
    surface.vline(x + width - 1, y, y + height - 1, 255, 255, 255, 255);
    surface.hline(x + 1, x + width - 2, y + 1, 0, 0, 0, 255);
    surface.vline(x + 1, y + 1, y + height - 2, 0, 0, 0, 255);
}
#endif

std::string narrowText(const TagLib::String& text)
{
    return text.to8Bit(true);
}

} // namespace

TextInputWidget::TextInputWidget(int width, int height, Font* font, const TagLib::String& text)
    : Widget()
    , m_font(font)
    , m_text(text)
    , m_caret_index(narrowText(text).size())
    , m_pressed(false)
    , m_hovered(false)
    , m_focused(false)
{
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

TextInputWidget::~TextInputWidget()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
}

bool TextInputWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT || !handlesPoint(relative_x, relative_y)) {
        return false;
    }

    m_pressed = true;
    focus();
    return true;
}

bool TextInputWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button != SDL_BUTTON_LEFT || !m_pressed) {
        return false;
    }

    m_pressed = false;
    m_hovered = handlesPoint(relative_x, relative_y);
    rebuildSurface();
    return true;
}

bool TextInputWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;

    const bool hovered = handlesPoint(relative_x, relative_y);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        rebuildSurface();
    }

    return hovered || m_pressed;
}

void TextInputWidget::setText(const TagLib::String& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    m_caret_index = narrowText(m_text).size();
    rebuildSurface();
    if (m_on_change) {
        m_on_change(m_text);
    }
}

void TextInputWidget::setPlaceholder(const TagLib::String& placeholder)
{
    if (m_placeholder == placeholder) {
        return;
    }

    m_placeholder = placeholder;
    rebuildSurface();
}

void TextInputWidget::clearFocusedWidget()
{
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
}

bool TextInputWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    if (!s_focused_widget) {
        return false;
    }

    TextInputWidget& widget = *s_focused_widget;
    std::string text = narrowText(widget.m_text);

    switch (keysym.sym) {
        case SDLK_BACKSPACE:
            return widget.eraseBeforeCaret();
        case SDLK_DELETE:
            return widget.eraseAtCaret();
        case SDLK_LEFT:
            if (widget.m_caret_index > 0) {
                --widget.m_caret_index;
                widget.rebuildSurface();
            }
            return true;
        case SDLK_RIGHT:
            if (widget.m_caret_index < text.size()) {
                ++widget.m_caret_index;
                widget.rebuildSurface();
            }
            return true;
        case SDLK_HOME:
            widget.m_caret_index = 0;
            widget.rebuildSurface();
            return true;
        case SDLK_END:
            widget.m_caret_index = text.size();
            widget.rebuildSurface();
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return true;
        default:
            break;
    }

    return false;
}

bool TextInputWidget::handleFocusedTextInput(const char* text)
{
    if (!s_focused_widget || !text || text[0] == '\0') {
        return false;
    }

    TextInputWidget& widget = *s_focused_widget;
    const std::string input(text);

    bool changed = false;
    for (unsigned char c : input) {
        if (c >= 32 && c <= 126) {
            changed = widget.insertCharacter(static_cast<char>(c)) || changed;
        }
    }

    return changed;
}

void TextInputWidget::focus()
{
    if (s_focused_widget && s_focused_widget != this) {
        s_focused_widget->blur();
    }

    s_focused_widget = this;
    m_focused = true;
    m_caret_index = narrowText(m_text).size();
    rebuildSurface();
}

void TextInputWidget::blur()
{
    m_pressed = false;
    m_focused = false;
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    rebuildSurface();
}

bool TextInputWidget::handlesPoint(int relative_x, int relative_y) const
{
    const Rect& pos = getPos();
    return relative_x >= 0 && relative_x < pos.width() &&
           relative_y >= 0 && relative_y < pos.height();
}

bool TextInputWidget::insertCharacter(char c)
{
    std::string text = narrowText(m_text);
    if (m_caret_index > text.size()) {
        m_caret_index = text.size();
    }

    text.insert(text.begin() + static_cast<std::ptrdiff_t>(m_caret_index), c);
    ++m_caret_index;
    m_text = TagLib::String(text, TagLib::String::UTF8);
    rebuildSurface();
    if (m_on_change) {
        m_on_change(m_text);
    }
    return true;
}

bool TextInputWidget::eraseBeforeCaret()
{
    std::string text = narrowText(m_text);
    if (m_caret_index == 0 || text.empty()) {
        return true;
    }

    text.erase(text.begin() + static_cast<std::ptrdiff_t>(m_caret_index - 1));
    --m_caret_index;
    m_text = TagLib::String(text, TagLib::String::UTF8);
    rebuildSurface();
    if (m_on_change) {
        m_on_change(m_text);
    }
    return true;
}

bool TextInputWidget::eraseAtCaret()
{
    std::string text = narrowText(m_text);
    if (m_caret_index >= text.size()) {
        return true;
    }

    text.erase(text.begin() + static_cast<std::ptrdiff_t>(m_caret_index));
    m_text = TagLib::String(text, TagLib::String::UTF8);
    rebuildSurface();
    if (m_on_change) {
        m_on_change(m_text);
    }
    return true;
}

void TextInputWidget::rebuildSurface()
{
    const Rect& pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));
    drawWin31SunkenFrame(*surface, 0, 0, pos.width(), pos.height());

    const int inner_left = 4;
    const int inner_top = 3;
    const int inner_width = std::max(1, pos.width() - 8);

    std::string full_utf8 = narrowText(m_text);
    std::string display_utf8 = full_utf8;
    size_t visible_start = 0;
    std::unique_ptr<Surface> text_surface;

    while (visible_start < full_utf8.size()) {
        display_utf8 = full_utf8.substr(visible_start);
        if (m_font) {
            text_surface = m_font->Render(TagLib::String(display_utf8, TagLib::String::UTF8), 0, 0, 0);
        }
        if (!text_surface || text_surface->width() + 1 <= inner_width) {
            break;
        }
        ++visible_start;
    }

    if (full_utf8.empty() && !m_placeholder.isEmpty() && !m_focused && m_font) {
        text_surface = m_font->Render(m_placeholder, 128, 128, 128);
    }

    if (text_surface) {
        const int text_y = std::max(inner_top, (pos.height() - text_surface->height()) / 2);
        surface->Blit(*text_surface, Rect(inner_left, text_y, text_surface->width(), text_surface->height()));
    }

    if (m_focused) {
        size_t local_caret = m_caret_index < visible_start ? 0 : m_caret_index - visible_start;
        local_caret = std::min(local_caret, display_utf8.size());
        int caret_x = inner_left;

        if (local_caret > 0 && m_font) {
            auto prefix_surface = m_font->Render(
                TagLib::String(display_utf8.substr(0, local_caret), TagLib::String::UTF8), 0, 0, 0);
            if (prefix_surface) {
                caret_x += prefix_surface->width();
            }
        }

        caret_x = std::min(caret_x, inner_left + inner_width - 1);
        surface->vline(caret_x, inner_top, pos.height() - inner_top - 1, 0, 0, 0, 255);
    }

    if (m_hovered && !m_focused) {
        surface->rectangle(2, 2, pos.width() - 3, pos.height() - 3, 192, 192, 192, 255);
    }

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
