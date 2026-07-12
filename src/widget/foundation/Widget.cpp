/*
 * Widget.cpp - Extensible widget class, extends Surface.
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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
namespace Foundation {

// Static mouse capture widget
Widget* Widget::s_mouse_captured_widget = nullptr;

namespace {

class ClipRectGuard {
public:
    ClipRectGuard(Surface& target, const Rect& clip_rect)
        : m_surface(target.getHandle()), m_active(m_surface != nullptr)
    {
        if (!m_active) {
            return;
        }

        SDL_GetClipRect(m_surface, &m_previous_clip);

        Rect previous_rect(m_previous_clip.x, m_previous_clip.y,
                           static_cast<uint16_t>(m_previous_clip.w),
                           static_cast<uint16_t>(m_previous_clip.h));
        Rect next_rect = previous_rect.intersection(clip_rect);

        SDL_Rect sdl_clip = {
            next_rect.x(),
            next_rect.y(),
            next_rect.width(),
            next_rect.height()
        };
        SDL_SetClipRect(m_surface, &sdl_clip);
    }

    ~ClipRectGuard()
    {
        if (m_active) {
            SDL_SetClipRect(m_surface, &m_previous_clip);
        }
    }

private:
    SDL_Surface* m_surface;
    SDL_Rect m_previous_clip{};
    bool m_active;
};

Rect localViewport(const Widget& widget)
{
    const Rect& pos = widget.getPos();
    return Rect(0, 0, pos.width(), pos.height());
}

Rect visibleChildRect(const Widget& parent, const Widget& child)
{
    return child.getPos().intersection(localViewport(parent));
}

bool translateToDescendant(const Widget& ancestor, const Widget& descendant,
                           int ancestor_x, int ancestor_y,
                           int& descendant_x, int& descendant_y)
{
    descendant_x = ancestor_x;
    descendant_y = ancestor_y;

    const Widget* current = &descendant;
    while (current && current != &ancestor) {
        const Rect& pos = current->getPos();
        descendant_x -= pos.x();
        descendant_y -= pos.y();
        current = current->getParent();
    }

    return current == &ancestor;
}

bool pointHitsRect(const Rect& rect, int x, int y)
{
    return rect.isValid() &&
           x >= rect.x() && x < rect.x() + rect.width() &&
           y >= rect.y() && y < rect.y() + rect.height();
}

} // namespace


Widget::Widget()
    : m_parent(nullptr), m_z_order(0), m_visible(true), m_enabled(true), m_mouse_transparent(false)
{
    //ctor
}

Widget::~Widget()
{
    // Release mouse capture if this widget holds it
    if (s_mouse_captured_widget == this) {
        s_mouse_captured_widget = nullptr;
    }
    
    // Destroy all children
    destroy_unlocked();
}

// Move constructor: takes ownership of the Surface's internal SDL_Surface*
Widget::Widget(Surface&& other) :
    Surface(std::move(other)), // Move the base Surface part
    m_pos(0, 0),
    m_parent(nullptr),
    m_z_order(0),
    m_visible(true),
    m_enabled(true),
    m_mouse_transparent(false)
{
}

// Move constructor with position: takes ownership of the Surface and sets position
Widget::Widget(Surface&& other, const Rect& position) :
    Surface(std::move(other)), // Move the base Surface part
    m_pos(position),
    m_parent(nullptr),
    m_z_order(0),
    m_visible(true),
    m_enabled(true),
    m_mouse_transparent(false)
{
}

void Widget::BlitTo(Surface& target)
{
    if (!m_visible) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (this->isValid()) {
        target.Blit(*this, m_pos);
    }

    ClipRectGuard child_clip(target, m_pos);
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, m_pos);
    }
}

void Widget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    if (!m_visible) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                      parent_absolute_pos.y() + m_pos.y(),
                      m_pos.width(), m_pos.height());

    if (this->isValid()) {
        target.Blit(*this, absolute_pos);
    }

    ClipRectGuard child_clip(target, absolute_pos);
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, absolute_pos);
    }
}

void Widget::setPos(const Rect& position)
{
    m_pos = position;
}

void Widget::setSurface(std::unique_ptr<Surface> surface)
{
    if (surface && surface->isValid()) {
        // Since Widget IS-A Surface, we can move the handle.
        this->m_handle = std::move(surface->m_handle);
    } else {
        this->m_handle.reset();
    }
}

void Widget::addChild(std::unique_ptr<Widget> child)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    addChild_unlocked(std::move(child));
}

Surface& Widget::getSurface() {
    // A Widget IS-A Surface, so it can return a reference to its Surface base.
    return *this;
}

bool Widget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // If a widget has mouse capture, forward directly to it regardless of
    // position — mirroring handleMouseMotion/handleMouseUp. (Previously
    // mouse-downs were dropped tree-wide during capture, so e.g. a second
    // button pressed mid-drag was silently swallowed.)
    if (s_mouse_captured_widget) {
        if (s_mouse_captured_widget == this) {
            return true; // Will be handled by the specific widget's override
        }

        int captured_relative_x = 0;
        int captured_relative_y = 0;
        if (translateToDescendant(*this, *s_mouse_captured_widget,
                                  relative_x, relative_y,
                                  captured_relative_x, captured_relative_y)) {
            return s_mouse_captured_widget->handleMouseDown(
                event, captured_relative_x, captured_relative_y);
        }

        // Not our captured widget, don't handle
        return false;
    }

    // Forward to children in reverse order (front to back for event handling)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        const auto& child = *it;
        
        // Skip mouse-transparent widgets for hit testing
        if (child->isMouseTransparent()) {
            continue;
        }
        
        Rect child_visible_pos = visibleChildRect(*this, *child);
        const Rect& child_pos = child->getPos();
        
        // Check if mouse is within child bounds
        if (pointHitsRect(child_visible_pos, relative_x, relative_y)) {
            
            // Calculate relative coordinates for child
            int child_relative_x = relative_x - child_pos.x();
            int child_relative_y = relative_y - child_pos.y();
            
            // Forward event to child
            if (child->handleMouseDown(event, child_relative_x, child_relative_y)) {
                return true; // Event was handled by child
            }
        }
    }
    
    // Event not handled by any child
    return false;
}

bool Widget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    // If a widget has mouse capture, forward directly to it regardless of position
    if (s_mouse_captured_widget) {
        // Find the captured widget and forward the event
        if (s_mouse_captured_widget == this) {
            // This widget has capture, handle the event
            return true; // Will be handled by the specific widget's override
        }
        
        int captured_relative_x = 0;
        int captured_relative_y = 0;
        if (translateToDescendant(*this, *s_mouse_captured_widget,
                                  relative_x, relative_y,
                                  captured_relative_x, captured_relative_y)) {
            return s_mouse_captured_widget->handleMouseMotion(
                event, captured_relative_x, captured_relative_y);
        }
        
        // Not our captured widget, don't handle
        return false;
    }
    
    // Normal processing - forward to children in reverse order
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        const auto& child = *it;
        
        // Skip mouse-transparent widgets for hit testing
        if (child->isMouseTransparent()) {
            continue;
        }
        
        Rect child_visible_pos = visibleChildRect(*this, *child);
        const Rect& child_pos = child->getPos();
        
        // Check if mouse is within child bounds
        if (pointHitsRect(child_visible_pos, relative_x, relative_y)) {
            
            // Calculate relative coordinates for child
            int child_relative_x = relative_x - child_pos.x();
            int child_relative_y = relative_y - child_pos.y();
            
            // Forward event to child
            if (child->handleMouseMotion(event, child_relative_x, child_relative_y)) {
                return true; // Event was handled by child
            }
        }
    }
    
    // Event not handled by any child
    return false;
}

bool Widget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // If a widget has mouse capture, forward directly to it regardless of position
    if (s_mouse_captured_widget) {
        // Find the captured widget and forward the event
        if (s_mouse_captured_widget == this) {
            // This widget has capture, handle the event
            return true; // Will be handled by the specific widget's override
        }
        
        int captured_relative_x = 0;
        int captured_relative_y = 0;
        if (translateToDescendant(*this, *s_mouse_captured_widget,
                                  relative_x, relative_y,
                                  captured_relative_x, captured_relative_y)) {
            return s_mouse_captured_widget->handleMouseUp(
                event, captured_relative_x, captured_relative_y);
        }
        
        // Not our captured widget, don't handle
        return false;
    }
    
    // Normal processing - forward to children in reverse order
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        const auto& child = *it;
        
        // Skip mouse-transparent widgets for hit testing
        if (child->isMouseTransparent()) {
            continue;
        }
        
        Rect child_visible_pos = visibleChildRect(*this, *child);
        const Rect& child_pos = child->getPos();
        
        // Check if mouse is within child bounds
        if (pointHitsRect(child_visible_pos, relative_x, relative_y)) {
            
            // Calculate relative coordinates for child
            int child_relative_x = relative_x - child_pos.x();
            int child_relative_y = relative_y - child_pos.y();
            
            // Forward event to child
            if (child->handleMouseUp(event, child_relative_x, child_relative_y)) {
                return true; // Event was handled by child
            }
        }
    }
    
    // Event not handled by any child
    return false;
}
void Widget::captureMouse()
{
    s_mouse_captured_widget = this;
}

void Widget::releaseMouse()
{
    if (s_mouse_captured_widget == this) {
        s_mouse_captured_widget = nullptr;
    }
}

bool Widget::hasMouseCapture() const
{
    return s_mouse_captured_widget == this;
}

Widget* Widget::getMouseCapturedWidget()
{
    return s_mouse_captured_widget;
}

void Widget::setMouseTransparent(bool transparent)
{
    m_mouse_transparent = transparent;
}

bool Widget::isMouseTransparent() const
{
    return m_mouse_transparent;
}

void Widget::invalidate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    invalidate_unlocked();
}

void Widget::invalidateArea(const Rect& area)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    invalidateArea_unlocked(area);
}

// Event handling infrastructure methods

bool Widget::handleEvent(const SDL_Event& event, int relative_x, int relative_y)
{
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            return handleMouseDown(event.button, relative_x, relative_y);
        case SDL_MOUSEBUTTONUP:
            return handleMouseUp(event.button, relative_x, relative_y);
        case SDL_MOUSEMOTION:
            return handleMouseMotion(event.motion, relative_x, relative_y);
        default:
            return false;
    }
}

bool Widget::hitTest(int x, int y) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return hitTest_unlocked(x, y);
}

std::pair<int, int> Widget::transformCoordinates(int parent_x, int parent_y) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return transformCoordinates_unlocked(parent_x, parent_y);
}


// Private unlocked methods - assume lock is already held

void Widget::invalidate_unlocked()
{
    // If we have a parent, notify it that we need repainting
    if (m_parent) {
        m_parent->invalidateArea_unlocked(m_pos);
    }
    // For root widgets (like ApplicationWidget), mark as needing full repaint
    // This will be handled at the application level
}

void Widget::invalidateArea_unlocked(const Rect& area)
{
    // If we have a parent, propagate the invalidation upward
    if (m_parent) {
        // Transform area coordinates to parent's coordinate system
        Rect parent_area(m_pos.x() + area.x(), m_pos.y() + area.y(), 
                        area.width(), area.height());
        m_parent->invalidateArea_unlocked(parent_area);
    }
    // For root widgets, this would trigger a repaint of the specified area
}

bool Widget::hitTest_unlocked(int x, int y) const
{
    // x,y are in this widget's LOCAL coordinate space (callers pass
    // relative_x/relative_y), so test against local bounds with origin (0,0) —
    // not m_pos, whose origin is the widget's position in the PARENT's space.
    // Testing against m_pos made any widget not at (0,0) unclickable (e.g.
    // CheckboxWidget, ScrollbarWidget). Matches ButtonWidget's inline check.
    return x >= 0 && y >= 0 && x < m_pos.width() && y < m_pos.height();
}

std::pair<int, int> Widget::transformCoordinates_unlocked(int parent_x, int parent_y) const
{
    // Transform from parent space to widget space
    int widget_x = parent_x - m_pos.x();
    int widget_y = parent_y - m_pos.y();
    return std::make_pair(widget_x, widget_y);
}

void Widget::addChild_unlocked(std::unique_ptr<Widget> child)
{
    if (child) {
        child->m_parent = this; // Set parent pointer before taking ownership
        m_children.push_back(std::move(child));
    }
}

void Widget::removeChild(Widget* child)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    removeChild_unlocked(child);
}

void Widget::removeChild_unlocked(Widget* child)
{
    if (child) {
        auto it = std::find_if(m_children.begin(), m_children.end(),
            [child](const std::unique_ptr<Widget>& c) { return c.get() == child; });
        if (it != m_children.end()) {
            m_children.erase(it);
        }
    }
}

void Widget::destroy()
{
    // If owned by a parent, ask it to remove us. removeChild erases the
    // unique_ptr, which triggers our destructor; execution ends here.
    if (m_parent) {
        m_parent->removeChild(this);
        return;
    }
    // Root widget with no parent — just clear children.
    std::lock_guard<std::mutex> lock(m_mutex);
    destroy_unlocked();
}

void Widget::destroy_unlocked()
{
    for (auto& child : m_children) {
        child->destroy_unlocked();
    }
    m_children.clear();
}


} // namespace Foundation
} // namespace Widget
} // namespace PsyMP3
