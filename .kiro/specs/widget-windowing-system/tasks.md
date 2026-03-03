# Implementation Plan: Widget and Windowing System

## Overview

This implementation plan covers the complete Widget and Windowing System for PsyMP3, organized into four phases following the design document. The system provides hierarchical composition, Z-order management, event handling, and window management with proper thread safety using the Public/Private Lock Pattern.

## Phase 1: Core Widget System

### Task 1.1: Implement Widget Base Class with Hierarchy

- [ ] 1.1.1 Create Widget base class header and implementation
  - Define Widget class with private member variables (m_parent, m_children, m_z_order, m_visible, m_enabled, m_mouse_transparent, m_bounds, m_surface, m_dirty)
  - Implement constructor and destructor
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.7, 15.1_

- [ ] 1.1.2 Implement child management methods
  - Implement add_child() and remove_child() with ownership transfer
  - Implement destroy() for recursive cleanup
  - _Requirements: 1.2, 1.7_

- [ ] 1.1.3 Implement property accessors
  - Implement get_bounds(), set_bounds(), get_z_order(), set_z_order()
  - Implement is_visible(), set_visible(), is_mouse_transparent(), set_mouse_transparent()
  - _Requirements: 1.4, 15.1_

- [ ] 1.1.4 Implement rendering infrastructure
  - Implement render() and invalidate() public methods
  - Implement render_unlocked() and invalidate_unlocked() private methods
  - _Requirements: 1.3, 1.6, 15.1_

- [ ] 1.1.5 Implement event handling infrastructure
  - Implement handle_event() public method
  - Implement handle_event_unlocked() private method (pure virtual)
  - Implement hit_test() and transform_coordinates() methods
  - _Requirements: 1.5, 3.4, 3.5_

- [ ] 1.1.6 Write property test for widget hierarchy
  - **Property 1: Hierarchy integrity**
  - **Validates: Requirements 1.1, 1.2, 1.7**
  - Test that parent-child relationships are maintained correctly
  - Test that destruction propagates correctly

- [ ] 1.1.7 Write unit tests for Widget class
  - Test child management operations
  - Test property accessors
  - Test coordinate transformation
  - _Requirements: 1.1, 1.2, 1.4, 1.5_

### Task 1.2: Implement Z-Order Management System

- [ ] 1.2.1 Create ZOrderManager class
  - Define ZOrderManager with m_widgets map (Integer to List<Widget*>)
  - Implement add_widget() and remove_widget() public methods
  - Implement get_widgets_in_z_order() for sorted retrieval
  - _Requirements: 2.1, 2.8, 15.1_

- [ ] 1.2.2 Implement Z-order constants
  - Define Z_ORDER_DESKTOP = 0, Z_ORDER_UI = 1, Z_ORDER_LOW = 10
  - Define Z_ORDER_NORMAL = 50, Z_ORDER_HIGH = 100, Z_ORDER_MAX = 999
  - _Requirements: 2.1_

- [ ] 1.2.3 Implement Z-order sorting
  - Implement sort_z_order_unlocked() for maintaining order within levels
  - Implement bring_to_front() for reordering
  - _Requirements: 2.8_

- [ ] 1.2.4 Write property test for Z-order invariants
  - **Property 2: Layering order**
  - **Validates: Requirements 2.2, 2.3, 2.4, 2.5, 2.6, 2.7**
  - Test that Z-order levels are rendered in correct order
  - Test that MAX priority toasts appear above all other elements

- [ ] 1.2.5 Write unit tests for ZOrderManager
  - Test widget addition and removal
  - Test Z-order sorting
  - Test bring_to_front functionality
  - _Requirements: 2.1, 2.8_

### Task 1.3: Implement Event Handling System

- [ ] 1.3.1 Create EventData structure and EventCaptureManager
  - Define EventData structure with event_type enum and all fields
  - Define EventCaptureManager with m_captured_widget and m_capture_active
  - Implement capture() and release() methods
  - _Requirements: 3.2, 3.3, 3.6, 3.7, 10.1, 10.7, 15.3_

- [ ] 1.3.2 Implement CoordinateTransformer class
  - Implement transform_to_widget() and transform_from_widget()
  - Implement hit_test_widget() for bounds checking
  - Implement get_absolute_position() for coordinate calculation
  - _Requirements: 3.4, 3.5_

- [ ] 1.3.3 Implement event delegation in Widget
  - Update handle_event_unlocked() to implement delegation logic
  - Implement proper event propagation (return true/false)
  - Test event capture and release
  - _Requirements: 3.1, 3.6, 3.7_

- [ ] 1.3.4 Write property test for event propagation
  - **Property 3: Event delegation**
  - **Validates: Requirements 3.1, 3.6, 3.7**
  - Test that events flow from top to bottom in Z-order
  - Test that handled events stop propagation

- [ ] 1.3.5 Write unit tests for event system
  - Test event capture and release
  - Test coordinate transformation
  - Test hit testing
  - _Requirements: 3.2, 3.3, 3.4, 3.5_

### Task 1.4: Implement Surface-Based Rendering

- [ ] 1.4.1 Create SurfaceManager class
  - Define SurfaceManager with m_surfaces map
  - Implement create_surface() and destroy_surface() methods
  - Implement get_surface() for surface retrieval
  - _Requirements: 4.1, 4.2, 4.7, 15.5, 15.6_

- [ ] 1.4.2 Implement InvalidationManager class
  - Define InvalidationManager with m_invalid_regions map
  - Implement invalidate() and get_invalid_regions() methods
  - Implement process_invalidations() for batch processing
  - _Requirements: 4.4, 4.7_

- [ ] 1.4.3 Implement Widget rendering
  - Update render_unlocked() to render widget content
  - Implement render_children_unlocked() for child rendering
  - Implement surface compositing to parent
  - _Requirements: 4.1, 4.2, 4.5, 4.6_

- [ ] 1.4.4 Write property test for rendering order
  - **Property 4: Rendering order**
  - **Validates: Requirements 4.3, 4.5, 4.6**
  - Test that children render after parent content
  - Test that compositing produces correct visual hierarchy

- [ ] 1.4.5 Write unit tests for rendering system
  - Test surface creation and destruction
  - Test invalidation and repainting
  - Test rendering order
  - _Requirements: 4.1, 4.2, 4.3, 4.4_

## Phase 2: Window System

### Task 2.1: Implement WindowFrameWidget

- [ ] 2.1.1 Create WindowFrameWidget class
  - Define WindowFrameWidget inheriting from LayoutWidget
  - Implement m_titlebar, m_client_area, m_resize_handles, m_window_state, m_min_size, m_max_size
  - Implement constructor with title parameter
  - _Requirements: 5.1, 5.7, 5.8, 15.1_

- [ ] 2.1.2 Implement window operations
  - Implement close(), minimize(), maximize(), restore()
  - Implement set_resizable() for mode configuration
  - _Requirements: 5.1, 5.7, 9.7_

- [ ] 2.1.3 Implement content management
  - Implement set_client_content() and get_client_content()
  - Implement update_window_layout_unlocked() for layout management
  - _Requirements: 5.1, 9.8_

- [ ] 2.1.4 Implement titlebar event handling
  - Implement handle_titlebar_event() for titlebar interactions
  - Implement handle_resize_event() for resize handle interactions
  - _Requirements: 5.2, 5.3, 5.4, 5.5, 10.3_

- [ ] 2.1.5 Write property test for window operations
  - **Property 5: Window lifecycle**
  - **Validates: Requirements 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8**
  - Test that window operations work correctly
  - Test that lifecycle events are triggered

- [ ] 2.1.6 Write unit tests for WindowFrameWidget
  - Test window operations
  - Test content management
  - Test event handling
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8_

### Task 2.2: Implement TitlebarWidget

- [ ] 2.2.1 Create TitlebarWidget class
  - Define TitlebarWidget inheriting from LayoutWidget
  - Implement title display and control buttons
  - Implement drag handling for window movement
  - _Requirements: 5.2, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.8_

- [ ] 2.2.2 Implement titlebar rendering
  - Implement render_unlocked() with Windows 3.x style decorations
  - Implement proper 3D bevel effects
  - _Requirements: 9.1, 9.6_

- [ ] 2.2.3 Implement control button handling
  - Implement close, minimize, maximize button callbacks
  - Implement double-click handling for maximize/close
  - _Requirements: 5.3, 5.4, 9.3_

- [ ] 2.2.4 Write unit tests for TitlebarWidget
  - Test title display
  - Test control button handling
  - Test drag operations
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_

### Task 2.3: Implement Window State Management

- [ ] 2.3.1 Create WindowStateManager class
  - Define WindowState enum (NORMAL, MINIMIZED, MAXIMIZED)
  - Implement calculate_resized_bounds() methods
  - _Requirements: 5.6, 9.7_

- [ ] 2.3.2 Implement window lifecycle events
  - Define WindowEvent enum
  - Define WindowCallback type
  - Implement event callbacks for all window events
  - _Requirements: 5.6, 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_

- [ ] 2.3.3 Write property test for window state transitions
  - **Property 6: State transitions**
  - **Validates: Requirements 5.6, 5.7, 5.8**
  - Test that state transitions are valid
  - Test that events are triggered correctly

- [ ] 2.3.4 Write unit tests for window state management
  - Test state transitions
  - Test event callbacks
  - Test resize calculations
  - _Requirements: 5.6, 5.7, 5.8, 9.7_

## Phase 3: Specialized Widgets

### Task 3.1: Implement ToastWidget

- [ ] 3.1.1 Create ToastWidget class
  - Define ToastWidget inheriting from Widget
  - Implement m_message, m_duration, m_start_time, m_dismiss_callback, m_user_data
  - Implement constructor with message and duration parameters
  - _Requirements: 6.3, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 3.1.2 Implement toast operations
  - Implement update() for auto-dismiss timing
  - Implement set_dismiss_callback() for cleanup
  - Implement is_expired() for timing checks
  - _Requirements: 8.1, 8.3, 8.6_

- [ ] 3.1.3 Implement toast rendering
  - Implement render_unlocked() with rounded corners and semi-transparent background
  - Implement centered positioning and content sizing
  - _Requirements: 8.4, 8.5_

- [ ] 3.1.4 Write property test for toast lifecycle
  - **Property 7: Toast lifecycle**
  - **Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8**
  - Test that toasts auto-dismiss correctly
  - Test that toasts are mouse-transparent

- [ ] 3.1.5 Write unit tests for ToastWidget
  - Test auto-dismiss timing
  - Test callback execution
  - Test rendering
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

### Task 3.2: Implement TransparentWindowWidget

- [ ] 3.2.1 Create TransparentWindowWidget class
  - Define TransparentWindowWidget inheriting from Widget
  - Implement m_opacity and m_mouse_transparent
  - Implement constructor with opacity parameter
  - _Requirements: 6.4, 15.1_

- [ ] 3.2.2 Implement opacity management
  - Implement get_opacity() and set_opacity() methods
  - Implement render_unlocked() with alpha blending
  - _Requirements: 4.3, 6.4_

- [ ] 3.2.3 Write unit tests for TransparentWindowWidget
  - Test opacity management
  - Test rendering with transparency
  - Test mouse transparency
  - _Requirements: 4.3, 6.4_

### Task 3.3: Implement ButtonWidget

- [ ] 3.3.1 Create ButtonWidget class
  - Define ButtonWidget inheriting from DrawableWidget
  - Implement m_text, m_font, m_pressed, m_hovered, m_click_callback, m_user_data
  - Implement constructor with text parameter
  - _Requirements: 6.7, 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8_

- [ ] 3.3.2 Implement button operations
  - Implement set_click_callback() for event handling
  - Implement click() for programmatic triggering
  - _Requirements: 6.7, 14.5_

- [ ] 3.3.3 Implement button rendering
  - Implement render_unlocked() with visual states
  - Implement hover and pressed state indicators
  - _Requirements: 14.1, 14.2, 14.3, 14.4_

- [ ] 3.3.4 Write property test for button interactions
  - **Property 8: Button state consistency**
  - **Validates: Requirements 6.7, 14.1, 14.2, 14.3, 14.4, 14.5**
  - Test that button states update correctly
  - Test that click events trigger properly

- [ ] 3.3.5 Write unit tests for ButtonWidget
  - Test click handling
  - Test visual states
  - Test callback execution
  - _Requirements: 6.7, 14.1, 14.2, 14.3, 14.4, 14.5_

### Task 3.4: Implement LayoutWidget

- [ ] 3.4.1 Create LayoutWidget class
  - Define LayoutWidget inheriting from LayoutWidget
  - Implement container functionality without subclassing
  - _Requirements: 6.6_

- [ ] 3.4.2 Write unit tests for LayoutWidget
  - Test child management
  - Test layout operations
  - _Requirements: 6.6_

## Phase 4: Application Integration

### Task 4.1: Implement ApplicationWidget Singleton

- [ ] 4.1.1 Create ApplicationWidget class
  - Define ApplicationWidget inheriting from Widget
  - Implement singleton pattern with m_instance
  - Implement m_desktop_widgets, m_windows, m_toasts collections
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 15.1_

- [ ] 4.1.2 Implement singleton management
  - Implement get_instance(), create_instance(), destroy_instance()
  - Implement screen coverage initialization
  - _Requirements: 6.1_

- [ ] 4.1.3 Implement desktop management
  - Implement add_desktop_widget() and remove_desktop_widget()
  - Implement render_widgets_by_z_order() helper
  - _Requirements: 6.2, 6.5_

- [ ] 4.1.4 Implement window management
  - Implement add_window(), remove_window(), bring_window_to_front()
  - Implement render() with proper Z-order composition
  - _Requirements: 6.3, 6.5, 15.5_

- [ ] 4.1.5 Implement toast management
  - Implement show_toast() and dismiss_toast()
  - Implement process_toasts_unlocked() for auto-dismiss
  - _Requirements: 6.4, 6.7, 8.1, 8.6_

- [ ] 4.1.6 Implement event handling
  - Implement handle_event() with delegation to windows and desktop
  - Implement handle_event_unlocked() for internal processing
  - _Requirements: 6.5, 15.3_

- [ ] 4.1.7 Implement lifecycle management
  - Implement shutdown() for cleanup
  - Implement render() with proper surface management
  - _Requirements: 6.8, 15.7, 15.8_

- [ ] 4.1.8 Write property test for singleton behavior
  - **Property 9: Singleton integrity**
  - **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8**
  - Test that only one instance exists
  - Test that all collections are properly managed

- [ ] 4.1.9 Write unit tests for ApplicationWidget
  - Test singleton management
  - Test desktop and window management
  - Test toast management
  - Test event handling
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

### Task 4.2: Integrate with Existing UI Elements

- [ ] 4.2.1 Integrate SpectrumAnalyzerWidget
  - Update SpectrumAnalyzerWidget to inherit from DrawableWidget
  - Implement integration with FFT processing
  - _Requirements: 13.2_

- [ ] 4.2.2 Integrate PlayerProgressBarWidget
  - Update PlayerProgressBarWidget to inherit from DrawableWidget
  - Implement integration with player position tracking
  - _Requirements: 13.3_

- [ ] 4.2.3 Update existing widgets
  - Update existing widgets to use new base classes
  - Ensure proper Z-order assignment
  - _Requirements: 13.5_

- [ ] 4.2.4 Write integration tests
  - Test SpectrumAnalyzerWidget integration
  - Test PlayerProgressBarWidget integration
  - Test overall UI rendering
  - _Requirements: 13.1, 13.2, 13.3_

### Task 4.3: Thread Safety Implementation

- [ ] 4.3.1 Implement Public/Private Lock Pattern
  - Add mutable std::mutex m_mutex to all widget classes
  - Update all public methods to acquire locks
  - Update private methods to have _unlocked variants
  - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7, 15.8_

- [ ] 4.3.2 Document lock acquisition order
  - Document lock acquisition order at class level
  - Document order: Global locks → Manager locks → Widget locks → System locks
  - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7, 15.8_

- [ ] 4.3.3 Implement cross-thread UI updates
  - Create UIThreadDispatcher for event queue management
  - Implement post_event() and process_events() methods
  - _Requirements: 15.2_

- [ ] 4.3.4 Write property test for thread safety
  - **Property 10: Thread safety**
  - **Validates: Requirements 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7, 15.8**
  - Test that concurrent access doesn't cause race conditions
  - Test that locks are properly released on exceptions

- [ ] 4.3.5 Write unit tests for thread safety
  - Test concurrent widget operations
  - Test cross-thread updates
  - Test lock acquisition
  - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7, 15.8_

### Task 4.4: Performance Optimization

- [ ] 4.4.1 Implement selective invalidation
  - Optimize invalidation to only update changed regions
  - Implement efficient region tracking
  - _Requirements: 11.4, 11.7_

- [ ] 4.4.2 Implement surface reuse
  - Implement surface pooling for frequently created widgets
  - Optimize surface allocation and deallocation
  - _Requirements: 11.5, 11.7_

- [ ] 4.4.3 Implement event filtering
  - Filter events before processing
  - Optimize event delegation
  - _Requirements: 11.3_

- [ ] 4.4.4 Write performance tests
  - Test rendering performance with complex UIs
  - Test event processing performance
  - Test memory usage with large widget hierarchies
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8_

## Checkpoints

- [ ] Checkpoint 1 - Phase 1 Complete
  - Ensure all core widget system components are implemented
  - Ensure all tests pass
  - Ask the user if questions arise.

- [ ] Checkpoint 2 - Phase 2 Complete
  - Ensure all window system components are implemented
  - Ensure all tests pass
  - Ask the user if questions arise.

- [ ] Checkpoint 3 - Phase 3 Complete
  - Ensure all specialized widgets are implemented
  - Ensure all tests pass
  - Ask the user if questions arise.

- [ ] Checkpoint 4 - Phase 4 Complete
  - Ensure all integration and thread safety is implemented
  - Ensure all tests pass
  - Ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- All threading follows the Public/Private Lock Pattern
- All rendering uses SDL surface-based compositing
- All coordinate systems are relative to parent widgets
