/*
 * WindowWidget.cpp - Implementation for modular floating window widget
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

int WindowWidget::s_next_z_order = 1;

WindowWidget::WindowWidget(int width, int height, const std::string& title)
    : Widget()
    , m_window_width(width)
    , m_window_height(height)
    , m_z_order(s_next_z_order++)
{
    // Set initial position and size
    Rect pos(100, 100, width, height); // Default position
    setPos(pos);
    
    // Create titlebar widget
    m_titlebar_widget = std::make_unique<TitlebarWidget>(
        width - (BORDER_WIDTH * 2), TITLEBAR_HEIGHT, title);
    
    // Set up titlebar drag callbacks
    m_titlebar_widget->setOnDrag([this](int dx, int dy) {
        Rect current_pos = getPos();
        current_pos.x(current_pos.x() + dx);
        current_pos.y(current_pos.y() + dy);
        setPos(current_pos);
        updateLayout(); // Update child widget positions when window moves
    });
    
    m_titlebar_widget->setOnDragStart([this](int x, int y) {
        bringToFront();
    });
    
    // Create default body widget
    m_body_widget = createDefaultBodyWidget();
    
    // Update layout
    updateLayout();
    
    // Create border surface
    setSurface(createBorderSurface());
}

void WindowWidget::BlitTo(Surface& target)
{
    // First render the window border/background
    Widget::BlitTo(target);
    
    // Then render child widgets on top
    if (m_titlebar_widget) {
        m_titlebar_widget->BlitTo(target);
    }
    
    if (m_body_widget) {
        m_body_widget->BlitTo(target);
    }
}

bool WindowWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Check titlebar first
    if (m_titlebar_widget) {
        Rect titlebar_pos = m_titlebar_widget->getPos();
        if (relative_x >= titlebar_pos.x() && relative_x < titlebar_pos.x() + titlebar_pos.width() &&
            relative_y >= titlebar_pos.y() && relative_y < titlebar_pos.y() + titlebar_pos.height()) {
            
            return m_titlebar_widget->handleMouseDown(event, 
                relative_x - titlebar_pos.x(), relative_y - titlebar_pos.y());
        }
    }
    
    // Check body widget
    if (m_body_widget) {
        Rect body_pos = m_body_widget->getPos();
        if (relative_x >= body_pos.x() && relative_x < body_pos.x() + body_pos.width() &&
            relative_y >= body_pos.y() && relative_y < body_pos.y() + body_pos.height()) {
            
            // Body widget clicked, bring window to front
            bringToFront();
            return true;
        }
    }
    
    return false;
}

bool WindowWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    // Forward to titlebar (it handles its own bounds checking during drag)
    if (m_titlebar_widget && m_titlebar_widget->handleMouseMotion(event, relative_x, relative_y)) {
        return true;
    }
    
    return false;
}

bool WindowWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Forward to titlebar
    if (m_titlebar_widget && m_titlebar_widget->handleMouseUp(event, relative_x, relative_y)) {
        return true;
    }
    
    return false;
}

const std::string& WindowWidget::getTitle() const
{
    if (m_titlebar_widget) {
        return m_titlebar_widget->getTitle();
    }
    static const std::string empty_title;
    return empty_title;
}

void WindowWidget::setTitle(const std::string& title)
{
    if (m_titlebar_widget) {
        m_titlebar_widget->setTitle(title);
    }
}

void WindowWidget::setBodyWidget(std::unique_ptr<Widget> body_widget)
{
    m_body_widget = std::move(body_widget);
    updateLayout();
}

void WindowWidget::bringToFront()
{
    m_z_order = s_next_z_order++;
}

void WindowWidget::updateLayout()
{
    Rect window_pos = getPos();
    
    // Position titlebar
    if (m_titlebar_widget) {
        Rect titlebar_pos(window_pos.x() + BORDER_WIDTH, 
                         window_pos.y() + BORDER_WIDTH,
                         m_window_width - (BORDER_WIDTH * 2), 
                         TITLEBAR_HEIGHT);
        m_titlebar_widget->setPos(titlebar_pos);
    }
    
    // Position body widget
    if (m_body_widget) {
        Rect body_pos(window_pos.x() + BORDER_WIDTH,
                     window_pos.y() + BORDER_WIDTH + TITLEBAR_HEIGHT,
                     m_window_width - (BORDER_WIDTH * 2),
                     m_window_height - TITLEBAR_HEIGHT - (BORDER_WIDTH * 2));
        m_body_widget->setPos(body_pos);
    }
}

std::unique_ptr<Widget> WindowWidget::createDefaultBodyWidget()
{
    auto body_widget = std::make_unique<Widget>();
    
    // Create white content surface
    int content_width = m_window_width - (BORDER_WIDTH * 2);
    int content_height = m_window_height - TITLEBAR_HEIGHT - (BORDER_WIDTH * 2);
    
    auto content_surface = std::make_unique<Surface>(content_width, content_height, true);
    uint32_t white_color = content_surface->MapRGB(255, 255, 255);
    content_surface->FillRect(white_color);
    
    body_widget->setSurface(std::move(content_surface));
    
    // Set size
    Rect body_rect(0, 0, content_width, content_height);
    body_widget->setPos(body_rect);
    
    return body_widget;
}

std::unique_ptr<Surface> WindowWidget::createBorderSurface()
{
    auto border_surface = std::make_unique<Surface>(m_window_width, m_window_height, true);
    
    // Start with transparent surface
    uint32_t transparent = border_surface->MapRGBA(0, 0, 0, 0);
    border_surface->FillRect(transparent);
    
    // Draw window border outline only (dark gray)
    border_surface->rectangle(0, 0, m_window_width - 1, m_window_height - 1, 64, 64, 64, 255);
    
    return border_surface;
}