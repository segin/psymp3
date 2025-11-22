/*
 * Widget.h - Extensible widget class.
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef WIDGET_H
#define WIDGET_H

namespace PsyMP3 {
namespace Widget {
namespace Foundation {

/**
 * @brief A Widget is a UI element that can be rendered to a surface and handle mouse events.
 * 
 * The Widget class serves as the foundation for the UI toolkit, providing hierarchical composition,
 * event handling, and rendering capabilities. Widgets can contain child widgets, creating complex
 * UI structures with proper event delegation and layered rendering.
 * 
 * Key features:
 * - Hierarchical composition with parent-child relationships
 * - Mouse event handling with capture and transparency support
 * - Surface-based rendering with automatic child widget blitting
 * - Position and bounds management
 * - Move semantics for efficient widget management
 * 
 * @see DrawableWidget for widgets that require custom drawing
 * @see LayoutWidget for container widgets without subclassing
 */
class Widget : public Surface
{
    public:
        /**
         * @brief Default constructor for Widget.
         * 
         * Creates a widget with no surface content and default position (0,0).
         * Mouse transparency is disabled by default.
         */
        Widget();
        
        /**
         * @brief Virtual destructor to ensure proper cleanup of derived classes.
         * 
         * Automatically releases mouse capture if this widget currently holds it.
         */
        virtual ~Widget();
        
        /**
         * @brief Copy constructor is deleted.
         * 
         * Widget objects are not copyable as they contain unique_ptr via Surface base.
         */
        Widget(const Widget&) = delete;
        
        /**
         * @brief Copy assignment operator is deleted.
         * 
         * Widget objects are not copyable as they contain unique_ptr via Surface base.
         */
        Widget& operator=(const Widget&) = delete;
        
        /**
         * @brief Move constructor (defaulted).
         * 
         * Allows efficient transfer of widget ownership.
         */
        Widget(Widget&&) = default;
        
        /**
         * @brief Move assignment operator (defaulted).
         * 
         * Allows efficient transfer of widget ownership.
         */
        Widget& operator=(Widget&&) = default;
        
        /**
         * @brief Constructs a Widget by taking ownership of a Surface.
         * 
         * @param other Surface to move into this widget
         */
        explicit Widget(Surface&& other);
        
        /**
         * @brief Constructs a Widget by taking ownership of a Surface and setting position.
         * 
         * @param other Surface to move into this widget
         * @param position Initial position and size of the widget
         */
        Widget(Surface&& other, const Rect& position);
        
        /**
         * @brief Renders this widget and all its children to the target surface.
         * 
         * This method first blits the widget's own surface content, then recursively
         * renders all child widgets on top. Child widgets are rendered in the order
         * they were added (first added = bottom layer, last added = top layer).
         * 
         * @param target The surface to render to
         */
        virtual void BlitTo(Surface& target);
        
        /**
         * @brief Sets the position and size of this widget.
         * 
         * @param position Rectangle defining the widget's position and dimensions
         */
        void setPos(const Rect& position);
        
        /**
         * @brief Gets the current position and size of this widget.
         * 
         * @return Const reference to the widget's position rectangle
         */
        const Rect& getPos() const { return m_pos; }
        
        /**
         * @brief Sets the surface content for this widget.
         * 
         * Takes ownership of the provided surface. This is typically used by
         * container widgets that don't have their own visual content.
         * 
         * @param surface Unique pointer to the surface to set
         */
        void setSurface(std::unique_ptr<Surface> surface);
        
        /**
         * @brief Adds a child widget to this widget.
         * 
         * Takes ownership of the child widget. Child widgets are rendered in the
         * order they are added, with later children appearing on top of earlier ones.
         * 
         * @param child Unique pointer to the child widget to add
         */
        void addChild(std::unique_ptr<Widget> child);
        
        /**
         * @brief Marks this widget as needing a repaint and notifies parent.
         * 
         * This method marks the widget as dirty and requests its parent to
         * repaint the area where this widget is located. The parent is responsible
         * for clearing the background and repainting all affected children.
         */
        void invalidate();
        
        /**
         * @brief Marks a specific area of this widget as needing repaint.
         * 
         * @param area Rectangle defining the area that needs repainting
         */
        void invalidateArea(const Rect& area);
        
        /**
         * @brief Handles mouse button down events.
         * 
         * This method implements hierarchical event delegation. It first forwards
         * the event to child widgets in reverse order (top to bottom), allowing
         * the topmost widget to handle the event first. If no child handles the
         * event, the method returns false.
         * 
         * Mouse-transparent widgets are skipped during hit testing.
         * 
         * @param event The SDL mouse button event
         * @param relative_x X coordinate relative to this widget's position
         * @param relative_y Y coordinate relative to this widget's position
         * @return true if the event was handled, false otherwise
         */
        virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
        
        /**
         * @brief Handles mouse motion events.
         * 
         * Similar to handleMouseDown, this method delegates to child widgets.
         * If a widget has mouse capture, the event is forwarded directly to
         * the captured widget regardless of mouse position.
         * 
         * @param event The SDL mouse motion event
         * @param relative_x X coordinate relative to this widget's position
         * @param relative_y Y coordinate relative to this widget's position
         * @return true if the event was handled, false otherwise
         */
        virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y);
        
        /**
         * @brief Handles mouse button up events.
         * 
         * Similar to handleMouseDown, this method delegates to child widgets.
         * If a widget has mouse capture, the event is forwarded directly to
         * the captured widget.
         * 
         * @param event The SDL mouse button event
         * @param relative_x X coordinate relative to this widget's position
         * @param relative_y Y coordinate relative to this widget's position
         * @return true if the event was handled, false otherwise
         */
        virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
        
        /**
         * @brief Captures mouse input to this widget.
         * 
         * When a widget has mouse capture, all mouse events are forwarded
         * directly to it regardless of mouse position. This is useful for
         * drag operations or modal interactions.
         * 
         * Only one widget can have mouse capture at a time. Calling this
         * method releases capture from any other widget.
         */
        void captureMouse();
        
        /**
         * @brief Releases mouse capture from this widget.
         * 
         * If this widget currently has mouse capture, it is released.
         * If another widget has capture, this method has no effect.
         */
        void releaseMouse();
        
        /**
         * @brief Checks if this widget currently has mouse capture.
         * 
         * @return true if this widget has mouse capture, false otherwise
         */
        bool hasMouseCapture() const;
        
        /**
         * @brief Gets the widget that currently has mouse capture.
         * 
         * @return Pointer to the widget with mouse capture, or nullptr if none
         */
        static Widget* getMouseCapturedWidget();
        
        /**
         * @brief Sets the mouse transparency state of this widget.
         * 
         * Mouse-transparent widgets are skipped during hit testing, allowing
         * mouse events to pass through to widgets behind them. This is useful
         * for overlay widgets like tooltips or decorative frames that should
         * not interfere with user interaction.
         * 
         * @param transparent true to make the widget transparent to mouse events
         */
        void setMouseTransparent(bool transparent);
        
        /**
         * @brief Checks if this widget is transparent to mouse events.
         * 
         * @return true if the widget is mouse-transparent, false otherwise
         */
        bool isMouseTransparent() const;
        
        /**
         * @brief Recursively renders this widget and its children with absolute positioning.
         * 
         * This method handles coordinate transformation for nested widget hierarchies.
         * Made public to allow container widgets like ApplicationWidget to properly
         * render their children with correct positioning.
         * 
         * @param target The surface to render to
         * @param parent_absolute_pos The absolute position of the parent widget
         */
        virtual void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos);
    protected:
        
        /**
         * @brief Gets a reference to this widget's surface.
         * 
         * Since Widget inherits from Surface, this method returns a reference
         * to the Surface base class.
         * 
         * @return Reference to this widget's surface
         */
        Surface& getSurface();
        
        /**
         * @brief The position and size of this widget.
         * 
         * Coordinates are relative to the parent widget, or absolute if this
         * is a root widget.
         */
        Rect m_pos;
        
        /**
         * @brief Collection of child widgets owned by this widget.
         * 
         * Child widgets are stored as unique_ptr to ensure proper ownership
         * and automatic cleanup. Children are rendered in the order they
         * appear in this vector.
         */
        std::vector<std::unique_ptr<Widget>> m_children;
        
        /**
         * @brief Pointer to the parent widget (non-owning).
         * 
         * Used for notifying parent of invalidation requests.
         * Null for root widgets.
         */
        Widget* m_parent;
        
        /**
         * @brief Flag indicating if this widget is transparent to mouse events.
         * 
         * When true, this widget is skipped during hit testing, allowing
         * mouse events to pass through to widgets behind it.
         */
        bool m_mouse_transparent;
        
        /**
         * @brief Static pointer to the widget that currently has mouse capture.
         * 
         * Only one widget system-wide can have mouse capture at a time.
         * When a widget has capture, all mouse events are forwarded to it
         * regardless of mouse position.
         */
        static Widget* s_mouse_captured_widget;
    private:
};

} // namespace Foundation
} // namespace Widget
} // namespace PsyMP3

#endif // WIDGET_H
