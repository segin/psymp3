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
#include <typeinfo>
#include <cstring>

namespace PsyMP3 {
namespace Widget {
namespace Foundation {

// Static mouse capture widget
Widget* Widget::s_mouse_captured_widget = nullptr;

// Helper function to get a clean widget type name for debugging
static std::string getWidgetTypeName(const Widget* widget) {
    const char* mangled_name = typeid(*widget).name();
    
    // On most compilers, the mangled name starts with a digit followed by the class name
    // Try to extract a readable class name
    const char* class_name = mangled_name;
    
    // Skip leading digits and length prefixes (common in GCC mangling)
    while (*class_name && std::isdigit(*class_name)) {
        class_name++;
    }
    
    // For most widget classes, extract just the class name part
    std::string result(class_name);
    
    // If it's still mangled or empty, fall back to the full mangled name
    if (result.empty()) {
        result = mangled_name;
    }
    
    return result;
}

Widget::Widget()
    : m_parent(nullptr), m_mouse_transparent(false)
{
    //ctor
}

Widget::~Widget()
{
    //dtor
}

// Move constructor: takes ownership of the Surface's internal SDL_Surface*
Widget::Widget(Surface&& other) :
    Surface(std::move(other)), // Move the base Surface part
    m_pos(0, 0),
    m_parent(nullptr),
    m_mouse_transparent(false)
{
}

// Move constructor with position: takes ownership of the Surface and sets position
Widget::Widget(Surface&& other, const Rect& position) :
    Surface(std::move(other)), // Move the base Surface part
    m_pos(position),
    m_parent(nullptr),
    m_mouse_transparent(false)
{
}

void Widget::BlitTo(Surface& target)
{
    std::string type_name = getWidgetTypeName(this);
    Debug::log("widget", "Widget::BlitTo [", type_name, "] - pos(", m_pos.x(), ",", m_pos.y(), ") size(", m_pos.width(), "x", m_pos.height(), ") valid=", this->isValid());
    
    // Blit this widget's own surface content first (if it has any)
    if (this->isValid()) {
        Debug::log("widget", "  Blitting widget surface to target");
        target.Blit(*this, m_pos);
    }

    // Then, recursively blit all children, passing this widget's position as the parent offset.
    for (const auto& child : m_children) {
        std::string child_type = getWidgetTypeName(child.get());
        Debug::log("widget", "  Recursively blitting child [", child_type, "]");
        child->recursiveBlitTo(target, m_pos);
    }
}

void Widget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    std::string type_name = getWidgetTypeName(this);
    
    // Calculate the child's absolute on-screen position by adding its relative position to the parent's absolute position.
    Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                      parent_absolute_pos.y() + m_pos.y(),
                      m_pos.width(), m_pos.height());

    Debug::log("widget", "  recursiveBlitTo [", type_name, "] - abs_pos(", absolute_pos.x(), ",", absolute_pos.y(), ") size(", absolute_pos.width(), "x", absolute_pos.height(), ") valid=", this->isValid());

    // Blit the child's own surface content.
    if (this->isValid()) {
        target.Blit(*this, absolute_pos);
    }

    // Recursively blit this child's children.
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
    if (child) {
        child->m_parent = this; // Set parent pointer before taking ownership
        m_children.push_back(std::move(child));
    }
}

Surface& Widget::getSurface() {
    // A Widget IS-A Surface, so it can return a reference to its Surface base.
    return *this;
}

bool Widget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // If a widget has mouse capture, forward directly to it
    if (s_mouse_captured_widget && s_mouse_captured_widget != this) {
        return false; // Let the captured widget handle it
    }
    
    // Forward to children in reverse order (front to back for event handling)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        const auto& child = *it;
        
        // Skip mouse-transparent widgets for hit testing
        if (child->isMouseTransparent()) {
            continue;
        }
        
        Rect child_pos = child->getPos();
        
        // Check if mouse is within child bounds
        if (relative_x >= child_pos.x() && relative_x < child_pos.x() + child_pos.width() &&
            relative_y >= child_pos.y() && relative_y < child_pos.y() + child_pos.height()) {
            
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
        
        // Check if captured widget is one of our children
        for (const auto& child : m_children) {
            if (child.get() == s_mouse_captured_widget) {
                // Calculate relative coordinates for captured child
                Rect child_pos = child->getPos();
                int child_relative_x = relative_x - child_pos.x();
                int child_relative_y = relative_y - child_pos.y();
                return child->handleMouseMotion(event, child_relative_x, child_relative_y);
            }
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
        
        Rect child_pos = child->getPos();
        
        // Check if mouse is within child bounds
        if (relative_x >= child_pos.x() && relative_x < child_pos.x() + child_pos.width() &&
            relative_y >= child_pos.y() && relative_y < child_pos.y() + child_pos.height()) {
            
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
        
        // Check if captured widget is one of our children
        for (const auto& child : m_children) {
            if (child.get() == s_mouse_captured_widget) {
                // Calculate relative coordinates for captured child
                Rect child_pos = child->getPos();
                int child_relative_x = relative_x - child_pos.x();
                int child_relative_y = relative_y - child_pos.y();
                return child->handleMouseUp(event, child_relative_x, child_relative_y);
            }
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
        
        Rect child_pos = child->getPos();
        
        // Check if mouse is within child bounds
        if (relative_x >= child_pos.x() && relative_x < child_pos.x() + child_pos.width() &&
            relative_y >= child_pos.y() && relative_y < child_pos.y() + child_pos.height()) {
            
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
    // If we have a parent, notify it that we need repainting
    if (m_parent) {
        m_parent->invalidateArea(m_pos);
    }
    // For root widgets (like ApplicationWidget), mark as needing full repaint
    // This will be handled at the application level
}

void Widget::invalidateArea(const Rect& area)
{
    // If we have a parent, propagate the invalidation upward
    if (m_parent) {
        // Transform area coordinates to parent's coordinate system
        Rect parent_area(m_pos.x() + area.x(), m_pos.y() + area.y(), 
                        area.width(), area.height());
        m_parent->invalidateArea(parent_area);
    }
    // For root widgets, this would trigger a repaint of the specified area
}

} // namespace Foundation
} // namespace Widget
} // namespace PsyMP3
