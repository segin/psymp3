/*
 * Button.cpp - Implementation for generic reusable button widget
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

ButtonWidget* ButtonWidget::s_focused_widget = nullptr;
std::vector<ButtonWidget*> ButtonWidget::s_default_buttons;

ButtonWidget::~ButtonWidget()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    auto it = std::find(s_default_buttons.begin(), s_default_buttons.end(), this);
    if (it != s_default_buttons.end()) {
        s_default_buttons.erase(it);
    }
}

void ButtonWidget::takeFocus()
{
    if (s_focused_widget == this) {
        return;
    }
    ButtonWidget* prev = s_focused_widget;
    s_focused_widget = this;
    if (prev) {
        prev->cancelKeyPress();
        prev->rebuildSurface();
    }
    rebuildSurface();
    // The bold border migrates from any default button to the focused one.
    for (ButtonWidget* d : s_default_buttons) {
        if (d != this && d != prev) d->rebuildSurface();
    }
}

void ButtonWidget::clearFocusedWidget()
{
    if (!s_focused_widget) {
        return;
    }
    ButtonWidget* prev = s_focused_widget;
    s_focused_widget = nullptr;
    prev->cancelKeyPress();
    prev->rebuildSurface();
    // ...and back to the default button once no button holds focus.
    for (ButtonWidget* d : s_default_buttons) {
        if (d != prev) d->rebuildSurface();
    }
}

void ButtonWidget::setDefault(bool is_default)
{
    if (m_default == is_default) {
        return;
    }
    m_default = is_default;
    auto it = std::find(s_default_buttons.begin(), s_default_buttons.end(), this);
    if (is_default && it == s_default_buttons.end()) {
        s_default_buttons.push_back(this);
    } else if (!is_default && it != s_default_buttons.end()) {
        s_default_buttons.erase(it);
    }
    rebuildSurface();
}

void ButtonWidget::activate()
{
    if (m_enabled && m_on_click) {
        // Copy first: the callback may destroy this button (e.g. About's Ok).
        auto on_click = m_on_click;
        on_click();
    }
}

bool ButtonWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    if (!s_focused_widget) {
        return false;
    }
    switch (keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Enter activates immediately on key-down.
            s_focused_widget->activate();
            return true;
        case SDLK_SPACE:
            // Space sinks the button and holds it pressed; the activation
            // happens on release (handleFocusedKeyUp), like real Windows.
            // Key auto-repeat lands here again — the flag makes it a no-op.
            if (!s_focused_widget->m_key_pressed) {
                s_focused_widget->m_key_pressed = true;
                s_focused_widget->m_pressed = true;
                s_focused_widget->rebuildSurface();
            }
            return true;
        default:
            return false;
    }
}

bool ButtonWidget::handleFocusedKeyUp(const SDL_keysym& keysym)
{
    if (!s_focused_widget || keysym.sym != SDLK_SPACE ||
        !s_focused_widget->m_key_pressed) {
        return false;
    }
    ButtonWidget& w = *s_focused_widget;
    w.m_key_pressed = false;
    w.m_pressed = false;
    w.rebuildSurface();
    w.activate(); // may destroy the button; touch nothing afterwards
    return true;
}

void ButtonWidget::cancelKeyPress()
{
    // Focus left while Space was held: the press is abandoned, not committed.
    if (m_key_pressed) {
        m_key_pressed = false;
        m_pressed = false;
        rebuildSurface();
    }
}

ButtonWidget::ButtonWidget(int width, int height, ButtonSymbol symbol)
    : Widget()
    , m_symbol(symbol)
    , m_pressed(false)
    , m_hovered(false)
    , m_enabled(true)
    , m_global_mouse_tracking(false)
    , m_font(nullptr)
{
    // Set button size
    Rect button_rect(0, 0, width, height);
    setPos(button_rect);
    
    // Create initial surface
    rebuildSurface();
}

bool ButtonWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!m_enabled || event.button != SDL_BUTTON_LEFT) {
        return false;
    }
    
    // Check if click is within button bounds
    Rect pos = getPos();
    if (relative_x >= 0 && relative_x < pos.width() &&
        relative_y >= 0 && relative_y < pos.height()) {
        if (isFocusable()) {
            takeFocus(); // clicking a push button also gives it keyboard focus
        }
        m_pressed = true;

        // Grab the mouse so the matching up event is delivered here even if the
        // cursor leaves the button before release. Without capture a release off
        // the button never reaches us, m_pressed stays set forever, and a later
        // unrelated release over the button would spuriously fire m_on_click.
        captureMouse();

        rebuildSurface();
        return true;
    }
    
    return false;
}

bool ButtonWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!m_enabled || event.button != SDL_BUTTON_LEFT) {
        return false;
    }
    
    if (m_pressed) {
        // Clear the pressed state and release the capture taken in
        // handleMouseDown, regardless of where the release landed.
        m_pressed = false;
        releaseMouse();

        // Only fire the click when the release lands over the button after the
        // captured press; a release elsewhere cancels the press silently.
        Rect pos = getPos();
        bool mouse_over_button = (relative_x >= 0 && relative_x < pos.width() &&
                                 relative_y >= 0 && relative_y < pos.height());

        if (mouse_over_button) {
            if (m_on_click) {
                m_on_click();
            }
        }

        rebuildSurface();
        return true;
    }
    
    return false;
}

bool ButtonWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (!m_enabled) {
        return false;
    }
    
    // Update hover state
    Rect pos = getPos();
    bool was_hovered = m_hovered;
    m_hovered = (relative_x >= 0 && relative_x < pos.width() && 
                relative_y >= 0 && relative_y < pos.height());
    
    if (was_hovered != m_hovered) {
        rebuildSurface();
    }
    
    return m_hovered;
}

void ButtonWidget::setSymbol(ButtonSymbol symbol)
{
    if (m_symbol != symbol) {
        m_symbol = symbol;
        rebuildSurface();
    }
}

void ButtonWidget::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!enabled) {
            m_pressed = false;
            m_hovered = false;
        }
        rebuildSurface();
    }
}

void ButtonWidget::setText(const TagLib::String& text, Font* font)
{
    if (m_text == text && m_font == font) {
        return;
    }

    m_text = text;
    m_font = font;
    rebuildSurface();
}

void ButtonWidget::setGeometry(const Rect& bounds)
{
    setPos(bounds);
    rebuildSurface();
}

void ButtonWidget::rebuildSurface()
{
    Rect pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    
    // Draw button background
    drawButtonBackground(*surface, m_pressed);
    
    // Draw button symbol
    if (!m_text.isEmpty() && m_font != nullptr) {
        drawButtonText(*surface, m_enabled);
    } else {
        drawButtonSymbol(*surface, m_symbol, m_enabled);
    }
    
    setSurface(std::move(surface));
}

void ButtonWidget::drawButtonBackground(Surface& surface, bool pressed)
{
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();

    // Button background (light gray)
    surface.box(0, 0, width - 1, height - 1, 192, 192, 192, 255);

    // Text push buttons render in the authentic Windows 3.1 style: a black
    // outline with the corner pixels notched (rounded), a 1px white highlight
    // inside the top/left, and a chunky 2px grey shadow inside the
    // bottom/right, stepped where the bevels meet. The outline doubles to 2px
    // on the button Enter activates (the focused one, else the window's
    // default). Symbol buttons (scrollbar arrows, titlebar controls) keep the
    // plain bevel their parents frame.
    if (!m_text.isEmpty()) {
        // Black outline, outer ring corners notched. The notched corner pixels
        // are punched fully transparent (Surface::pixel writes raw, unblended
        // values) so the parent's background shows through, rather than
        // leaving the face-grey fill behind on light backgrounds.
        surface.hline(1, width - 2, 0, 0, 0, 0, 255);
        surface.hline(1, width - 2, height - 1, 0, 0, 0, 255);
        surface.vline(0, 1, height - 2, 0, 0, 0, 255);
        surface.vline(width - 1, 1, height - 2, 0, 0, 0, 255);
        surface.pixel(0, 0, 0, 0, 0, 0);
        surface.pixel(width - 1, 0, 0, 0, 0, 0);
        surface.pixel(0, height - 1, 0, 0, 0, 0);
        surface.pixel(width - 1, height - 1, 0, 0, 0, 0);
        const int in = drawsBoldBorder() ? 2 : 1; // bevel inset = border width
        if (in == 2) {
            surface.rectangle(1, 1, width - 2, height - 2, 0, 0, 0, 255);
        }

        if (pressed) {
            // Pushed in: a single grey shadow along the inside top/left; the
            // label shifts one pixel down-right (see drawButtonText).
            surface.hline(in, width - 1 - in, in, 128, 128, 128, 255);
            surface.vline(in, in, height - 1 - in, 128, 128, 128, 255);
        } else {
            // 2px white top/left highlight, stepped at the far ends.
            surface.hline(in, width - 2 - in, in, 255, 255, 255, 255);
            surface.hline(in, width - 3 - in, in + 1, 255, 255, 255, 255);
            surface.vline(in, in, height - 2 - in, 255, 255, 255, 255);
            surface.vline(in + 1, in, height - 3 - in, 255, 255, 255, 255);
            // 2px grey bottom/right shadow, stepped at the meeting corners.
            surface.hline(in, width - 1 - in, height - 1 - in, 128, 128, 128, 255);
            surface.hline(in + 1, width - 1 - in, height - 2 - in, 128, 128, 128, 255);
            surface.vline(width - 1 - in, in, height - 1 - in, 128, 128, 128, 255);
            surface.vline(width - 2 - in, in + 1, height - 2 - in, 128, 128, 128, 255);
        }
        return;
    }

    if (pressed) {
        // Pressed button - inverted bevel (dark on top/left, light on bottom/right)
        // Top and left shadow (dark gray) - with 45-degree corner cut
        surface.hline(1, width - 2, 0, 128, 128, 128, 255); // Top line: start at (2,1) in one-indexed
        surface.vline(0, 1, height - 2, 128, 128, 128, 255); // Left line: start at (1,2) in one-indexed
        
        // Bottom and right highlight (white/light) - exact coordinates as specified
        // Outer highlight lines: (1,18)-(18,18) and (18,1)-(18,18)
        surface.hline(0, width - 1, height - 1, 255, 255, 255, 255); // (1,18)-(18,18)
        surface.vline(width - 1, 0, height - 1, 255, 255, 255, 255); // (18,1)-(18,18)
        
        // Inner shadow lines: (2,2)-(17,2) and (2,2)-(2,17) for pressed state
        surface.hline(1, width - 2, 1, 128, 128, 128, 255); // (2,2)-(17,2)
        surface.vline(1, 1, height - 2, 128, 128, 128, 255); // (2,2)-(2,17)
    } else {
        // Normal button - standard 3D bevel (light on top/left, dark on bottom/right)
        // Top and left highlight (white/light) - covering the full corner
        surface.hline(0, width - 2, 0, 255, 255, 255, 255); // Top line: start at (1,1) in one-indexed
        surface.vline(0, 0, height - 2, 255, 255, 255, 255); // Left line: start at (1,1) in one-indexed
        
        // Bottom and right shadow (dark gray) - exact coordinates as specified
        // Outer shading lines: (1,18)-(18,18) and (18,1)-(18,18)
        surface.hline(0, width - 1, height - 1, 128, 128, 128, 255); // (1,18)-(18,18)
        surface.vline(width - 1, 0, height - 1, 128, 128, 128, 255); // (18,1)-(18,18)
        
        // Inner shading lines: (2,17)-(17,17) and (17,2)-(17,17)
        surface.hline(1, width - 2, height - 2, 128, 128, 128, 255); // (2,17)-(17,17)
        surface.vline(width - 2, 1, height - 2, 128, 128, 128, 255); // (17,2)-(17,17)
    }
}

void ButtonWidget::drawButtonSymbol(Surface& surface, ButtonSymbol symbol, bool enabled)
{
    Rect pos = getPos();
    int center_x = pos.width() / 2;
    int center_y = pos.height() / 2;
    
    // Use black for enabled, gray for disabled
    uint8_t r = enabled ? 0 : 128;
    uint8_t g = enabled ? 0 : 128;
    uint8_t b = enabled ? 0 : 128;
    
    switch (symbol) {
        case ButtonSymbol::None:
            // No symbol to draw
            break;
            
        case ButtonSymbol::Minimize:
        case ButtonSymbol::ScrollDown:
            // Downward triangle
            {
                Sint16 x1 = center_x - 3;
                Sint16 y1 = center_y - 1;
                Sint16 x2 = center_x + 3;
                Sint16 y2 = center_y - 1;
                Sint16 x3 = center_x;
                Sint16 y3 = center_y + 2;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::Maximize:
        case ButtonSymbol::ScrollUp:
            // Upward triangle
            {
                Sint16 x1 = center_x;
                Sint16 y1 = center_y - 2;
                Sint16 x2 = center_x + 3;
                Sint16 y2 = center_y + 1;
                Sint16 x3 = center_x - 3;
                Sint16 y3 = center_y + 1;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::ScrollLeft:
            // Leftward triangle
            {
                Sint16 x1 = center_x + 1;
                Sint16 y1 = center_y - 3;
                Sint16 x2 = center_x + 1;
                Sint16 y2 = center_y + 3;
                Sint16 x3 = center_x - 2;
                Sint16 y3 = center_y;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::ScrollRight:
            // Rightward triangle
            {
                Sint16 x1 = center_x - 1;
                Sint16 y1 = center_y - 3;
                Sint16 x2 = center_x - 1;
                Sint16 y2 = center_y + 3;
                Sint16 x3 = center_x + 2;
                Sint16 y3 = center_y;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::Restore:
            // Two overlapping triangles
            {
                // Maximize triangle (bottom-left)
                Sint16 max_x1 = center_x - 3;
                Sint16 max_y1 = center_y - 1;
                Sint16 max_x2 = center_x;
                Sint16 max_y2 = center_y + 2;
                Sint16 max_x3 = center_x - 6;
                Sint16 max_y3 = center_y + 2;
                surface.filledTriangle(max_x1, max_y1, max_x2, max_y2, max_x3, max_y3, r, g, b, 255);
                
                // Minimize triangle (top-right)
                Sint16 min_x1 = center_x - 3;
                Sint16 min_y1 = center_y + 2;
                Sint16 min_x2 = center_x + 3;
                Sint16 min_y2 = center_y + 2;
                Sint16 min_x3 = center_x;
                Sint16 min_y3 = center_y + 5;
                surface.filledTriangle(min_x1, min_y1, min_x2, min_y2, min_x3, min_y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::Close:
            // X symbol (two diagonal lines)
            {
                int size = 3;
                for (int i = -size; i <= size; ++i) {
                    // Main diagonal
                    surface.pixel(center_x + i, center_y + i, r, g, b, 255);
                    // Anti-diagonal
                    surface.pixel(center_x + i, center_y - i, r, g, b, 255);
                }
            }
            break;
    }
}

void ButtonWidget::drawButtonText(Surface& surface, bool enabled)
{
    if (m_font == nullptr || m_text.isEmpty()) {
        return;
    }

    // ClearType (LCD subpixel) text, pre-blended against the button's light-gray
    // face (192,192,192) so the label matches the crisp Label/Toast rendering.
    const uint8_t fg = enabled ? 0 : 128;
    auto text_surface = m_font->RenderLCD(m_text, fg, fg, fg, 192, 192, 192);
    if (!text_surface) {
        return;
    }

    Rect pos = getPos();
    const int offset = m_pressed ? 1 : 0;
    const int text_x = std::max(2, (pos.width() - text_surface->width()) / 2 + offset);
    const int text_y = std::max(2, (pos.height() - text_surface->height()) / 2 + offset);
    surface.Blit(*text_surface, Rect(text_x, text_y, text_surface->width(), text_surface->height()));

    // Keyboard focus: the classic dotted rectangle. Vertically it sits on the
    // face just inside the bevels (inside the white above, the grey below);
    // horizontally it hugs the label's bounding box.
    if (s_focused_widget == this) {
        const int in = drawsBoldBorder() ? 2 : 1; // border width, as drawn
        const int x0 = text_x - 3;
        const int y0 = in + 2;
        const int x1 = text_x + text_surface->width() + 2;
        const int y1 = pos.height() - 3 - in;
        for (int x = x0; x <= x1; ++x) {
            if (((x + y0) & 1) == 0) surface.pixel(x, y0, 0, 0, 0, 255);
            if (((x + y1) & 1) == 0) surface.pixel(x, y1, 0, 0, 0, 255);
        }
        for (int y = y0 + 1; y < y1; ++y) {
            if (((x0 + y) & 1) == 0) surface.pixel(x0, y, 0, 0, 0, 255);
            if (((x1 + y) & 1) == 0) surface.pixel(x1, y, 0, 0, 0, 255);
        }
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
