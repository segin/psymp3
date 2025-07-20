# **WIDGET AND WINDOWING SYSTEM REQUIREMENTS**

## **Introduction**

This specification defines the requirements for PsyMP3's Widget and Windowing system, which provides a comprehensive UI framework with hierarchical composition, Z-order management, event handling, and window management. The system supports both desktop UI elements and floating windows with proper layering and interaction.

The system consists of:
- **Base Widget Architecture**: Hierarchical composition with parent-child relationships
- **Z-Order Management**: Proper layering system for UI elements and windows
- **Event Handling**: Mouse event delegation with capture and transparency support
- **Window Management**: Floating windows with titlebars, decorations, and lifecycle management
- **Specialized Widgets**: Toast notifications, transparent overlays, drawable widgets
- **Application Integration**: Root widget managing the entire UI hierarchy

## **Requirements**

### **Requirement 1: Base Widget Architecture**

**User Story:** As a UI developer, I want a hierarchical widget system, so that I can compose complex interfaces from simple components.

#### **Acceptance Criteria**

1. **WHEN** creating widgets **THEN** the system **SHALL** support hierarchical parent-child relationships
2. **WHEN** adding child widgets **THEN** the parent **SHALL** take ownership and manage lifecycle
3. **WHEN** rendering widgets **THEN** the system **SHALL** automatically render children in proper order
4. **WHEN** positioning widgets **THEN** coordinates **SHALL** be relative to parent widget
5. **WHEN** handling events **THEN** the system **SHALL** delegate events through the widget hierarchy
6. **WHEN** invalidating widgets **THEN** the system **SHALL** propagate invalidation to parent widgets
7. **WHEN** destroying widgets **THEN** the system **SHALL** automatically clean up all child widgets
8. **WHEN** moving widgets **THEN** the system **SHALL** support efficient move semantics

### **Requirement 2: Z-Order Management System**

**User Story:** As a UI system, I want proper layering of UI elements, so that windows, overlays, and notifications appear in the correct order.

#### **Acceptance Criteria**

1. **WHEN** managing Z-order **THEN** the system **SHALL** use defined constants (DESKTOP=0, UI=1, LOW=10, NORMAL=50, HIGH=100, MAX=999)
2. **WHEN** rendering elements **THEN** desktop elements **SHALL** appear at the bottom layer (FFT, main UI, controls)
3. **WHEN** displaying UI overlays **THEN** they **SHALL** appear just above desktop (lyrics, seek indicators)
4. **WHEN** showing tool windows **THEN** they **SHALL** use LOW priority for secondary panels
5. **WHEN** displaying normal windows **THEN** they **SHALL** use NORMAL priority for settings, dialogs
6. **WHEN** showing modal dialogs **THEN** they **SHALL** use HIGH priority for critical messages
7. **WHEN** displaying toast notifications **THEN** they **SHALL** use MAX priority and be mouse-transparent
8. **WHEN** managing window order **THEN** the system **SHALL** support bringing windows to front

### **Requirement 3: Event Handling and Delegation**

**User Story:** As a widget, I want to receive and handle mouse events appropriately, so that user interactions work correctly throughout the UI hierarchy.

#### **Acceptance Criteria**

1. **WHEN** handling mouse events **THEN** the system **SHALL** delegate events from top to bottom in Z-order
2. **WHEN** a widget captures mouse **THEN** all subsequent events **SHALL** go to the captured widget
3. **WHEN** widgets are mouse-transparent **THEN** events **SHALL** pass through to widgets behind them
4. **WHEN** calculating coordinates **THEN** the system **SHALL** transform coordinates to widget-relative positions
5. **WHEN** hit testing **THEN** the system **SHALL** check widget bounds before delegating events
6. **WHEN** events are handled **THEN** widgets **SHALL** return true to stop further propagation
7. **WHEN** events are not handled **THEN** widgets **SHALL** return false to continue delegation
8. **WHEN** dragging operations occur **THEN** the system **SHALL** maintain mouse capture throughout the operation

### **Requirement 4: Surface-Based Rendering**

**User Story:** As a widget system, I want efficient rendering with proper compositing, so that complex UIs render correctly and performantly.

#### **Acceptance Criteria**

1. **WHEN** rendering widgets **THEN** each widget **SHALL** have its own SDL surface for content
2. **WHEN** compositing **THEN** the system **SHALL** blit widget surfaces to parent surfaces
3. **WHEN** handling transparency **THEN** the system **SHALL** support alpha blending and transparency
4. **WHEN** updating content **THEN** widgets **SHALL** support invalidation and selective repainting
5. **WHEN** rendering hierarchies **THEN** the system **SHALL** render children after parent content
6. **WHEN** calculating positions **THEN** the system **SHALL** handle absolute positioning for nested hierarchies
7. **WHEN** optimizing rendering **THEN** the system **SHALL** avoid unnecessary redraws when possible
8. **WHEN** handling large UIs **THEN** the system **SHALL** maintain acceptable rendering performance

### **Requirement 5: Window Management System**

**User Story:** As a user, I want floating windows with proper decorations and behavior, so that I can interact with dialogs, settings, and other windowed content.

#### **Acceptance Criteria**

1. **WHEN** creating windows **THEN** the system **SHALL** provide titlebar, borders, and control buttons
2. **WHEN** dragging titlebars **THEN** windows **SHALL** move smoothly with mouse movement
3. **WHEN** clicking close button **THEN** windows **SHALL** trigger close events and remove themselves
4. **WHEN** double-clicking titlebars **THEN** windows **SHALL** trigger appropriate actions (close/maximize)
5. **WHEN** resizing windows **THEN** the system **SHALL** support resize handles and maintain minimum sizes
6. **WHEN** managing window lifecycle **THEN** windows **SHALL** support event callbacks for all major actions
7. **WHEN** handling window focus **THEN** the system **SHALL** bring clicked windows to front
8. **WHEN** shutting down **THEN** all windows **SHALL** receive shutdown notifications for cleanup

### **Requirement 6: ApplicationWidget Root Management**

**User Story:** As the application, I want a root widget that manages the entire UI, so that all elements are properly coordinated and rendered.

#### **Acceptance Criteria**

1. **WHEN** initializing **THEN** ApplicationWidget **SHALL** be a singleton covering the entire screen
2. **WHEN** managing desktop **THEN** the root widget **SHALL** contain all desktop UI elements as children
3. **WHEN** managing windows **THEN** the root widget **SHALL** maintain a separate collection of floating windows
4. **WHEN** handling events **THEN** the root widget **SHALL** route events to appropriate windows or desktop elements
5. **WHEN** rendering **THEN** the root widget **SHALL** composite desktop and windows in proper Z-order
6. **WHEN** updating **THEN** the root widget **SHALL** handle window lifecycle and auto-dismiss timers
7. **WHEN** managing toasts **THEN** the root widget **SHALL** support toast replacement and removal
8. **WHEN** shutting down **THEN** the root widget **SHALL** notify all windows and clean up resources

### **Requirement 7: Specialized Widget Types**

**User Story:** As a UI developer, I want specialized widget types for common use cases, so that I can implement complex UI elements efficiently.

#### **Acceptance Criteria**

1. **WHEN** creating drawable widgets **THEN** the system **SHALL** provide DrawableWidget base class for custom rendering
2. **WHEN** displaying toast notifications **THEN** ToastWidget **SHALL** provide auto-dismiss and mouse transparency
3. **WHEN** creating transparent overlays **THEN** TransparentWindowWidget **SHALL** support opacity and pass-through
4. **WHEN** implementing window frames **THEN** WindowFrameWidget **SHALL** provide complete window decorations
5. **WHEN** creating titlebars **THEN** TitlebarWidget **SHALL** support dragging and title display
6. **WHEN** building layouts **THEN** LayoutWidget **SHALL** provide container functionality without subclassing
7. **WHEN** handling buttons **THEN** ButtonWidget **SHALL** provide click events and visual feedback
8. **WHEN** displaying text **THEN** Label widgets **SHALL** support text rendering with fonts

### **Requirement 8: Toast Notification System**

**User Story:** As a user, I want temporary notifications that don't interfere with my workflow, so that I can see status updates without disruption.

#### **Acceptance Criteria**

1. **WHEN** showing toasts **THEN** they **SHALL** appear at the top of Z-order (MAX priority)
2. **WHEN** displaying toasts **THEN** they **SHALL** be mouse-transparent (events pass through)
3. **WHEN** timing toasts **THEN** they **SHALL** auto-dismiss after configurable duration (SHORT=2s, LONG=3.5s)
4. **WHEN** styling toasts **THEN** they **SHALL** have rounded corners and semi-transparent background
5. **WHEN** positioning toasts **THEN** they **SHALL** be centered and sized to fit content
6. **WHEN** replacing toasts **THEN** new toasts **SHALL** dismiss existing ones
7. **WHEN** dismissing toasts **THEN** they **SHALL** trigger callback events for cleanup
8. **WHEN** handling multiple toasts **THEN** the system **SHALL** manage toast lifecycle efficiently

### **Requirement 9: Window Frame and Decorations**

**User Story:** As a user, I want windows with familiar decorations and controls, so that I can interact with them intuitively.

#### **Acceptance Criteria**

1. **WHEN** drawing window frames **THEN** the system **SHALL** use Windows 3.x style decorations
2. **WHEN** displaying titlebars **THEN** they **SHALL** show window title and control buttons
3. **WHEN** providing control buttons **THEN** windows **SHALL** have minimize, maximize, and close buttons
4. **WHEN** handling resize **THEN** windows **SHALL** provide resize handles on borders and corners
5. **WHEN** showing system menu **THEN** control menu **SHALL** provide standard window operations
6. **WHEN** drawing borders **THEN** the system **SHALL** use proper 3D bevel effects
7. **WHEN** handling window states **THEN** the system **SHALL** support resizable/non-resizable modes
8. **WHEN** managing client area **THEN** windows **SHALL** provide separate client area for content

### **Requirement 10: Event System and Callbacks**

**User Story:** As a window developer, I want comprehensive event handling, so that I can respond to all user interactions appropriately.

#### **Acceptance Criteria**

1. **WHEN** handling window events **THEN** the system **SHALL** support generic event handler with event data structure
2. **WHEN** processing clicks **THEN** the system **SHALL** distinguish between single and double clicks
3. **WHEN** managing drag operations **THEN** the system **SHALL** provide drag start, move, and end events
4. **WHEN** handling window controls **THEN** the system **SHALL** provide separate callbacks for close, minimize, maximize
5. **WHEN** processing resize **THEN** the system **SHALL** provide resize events with new dimensions
6. **WHEN** managing focus **THEN** the system **SHALL** provide focus gained and lost events
7. **WHEN** shutting down **THEN** the system **SHALL** provide shutdown events for cleanup
8. **WHEN** supporting custom events **THEN** the system **SHALL** allow user-defined event types

### **Requirement 11: Performance and Memory Management**

**User Story:** As a media player, I want efficient UI rendering that doesn't impact audio playback, so that the interface remains responsive.

#### **Acceptance Criteria**

1. **WHEN** rendering complex UIs **THEN** the system **SHALL** maintain smooth audio playback
2. **WHEN** managing widget hierarchies **THEN** the system **SHALL** use efficient memory allocation
3. **WHEN** handling events **THEN** the system **SHALL** minimize processing overhead
4. **WHEN** updating displays **THEN** the system **SHALL** use selective repainting when possible
5. **WHEN** managing surfaces **THEN** the system **SHALL** reuse surfaces efficiently
6. **WHEN** handling large numbers of widgets **THEN** the system **SHALL** maintain acceptable performance
7. **WHEN** cleaning up resources **THEN** the system **SHALL** properly free all allocated memory
8. **WHEN** handling animations **THEN** the system **SHALL** support smooth visual transitions

### **Requirement 12: Cross-Platform Compatibility**

**User Story:** As a PsyMP3 user, I want the UI to work consistently across different operating systems, so that the interface behaves the same everywhere.

#### **Acceptance Criteria**

1. **WHEN** running on different platforms **THEN** the widget system **SHALL** use SDL for consistent behavior
2. **WHEN** handling mouse events **THEN** the system **SHALL** abstract platform-specific differences
3. **WHEN** rendering surfaces **THEN** the system **SHALL** handle different pixel formats appropriately
4. **WHEN** managing fonts **THEN** the system **SHALL** support cross-platform font rendering
5. **WHEN** handling keyboard input **THEN** the system **SHALL** support platform-specific key mappings
6. **WHEN** managing windows **THEN** the system **SHALL** work within SDL window constraints
7. **WHEN** handling display scaling **THEN** the system **SHALL** support different DPI settings
8. **WHEN** managing resources **THEN** the system **SHALL** handle platform-specific resource management

### **Requirement 13: Integration with PsyMP3 Systems**

**User Story:** As a PsyMP3 component, I want the widget system to integrate seamlessly with other subsystems, so that the UI works cohesively with audio playback and other features.

#### **Acceptance Criteria**

1. **WHEN** integrating with audio **THEN** the widget system **SHALL** not interfere with audio thread priorities
2. **WHEN** displaying spectrum **THEN** SpectrumAnalyzerWidget **SHALL** integrate with FFT processing
3. **WHEN** showing progress **THEN** progress widgets **SHALL** integrate with player position tracking
4. **WHEN** handling configuration **THEN** widgets **SHALL** integrate with PsyMP3 settings system
5. **WHEN** managing themes **THEN** the system **SHALL** support consistent visual styling
6. **WHEN** handling debug output **THEN** widgets **SHALL** use PsyMP3 Debug logging system
7. **WHEN** managing resources **THEN** the system **SHALL** coordinate with other PsyMP3 resource management
8. **WHEN** handling shutdown **THEN** the widget system **SHALL** coordinate with application lifecycle

### **Requirement 14: Accessibility and Usability**

**User Story:** As a user, I want an accessible and intuitive interface, so that I can use PsyMP3 effectively regardless of my abilities.

#### **Acceptance Criteria**

1. **WHEN** designing widgets **THEN** the system **SHALL** support keyboard navigation where appropriate
2. **WHEN** providing visual feedback **THEN** widgets **SHALL** show clear hover and active states
3. **WHEN** displaying text **THEN** the system **SHALL** support readable fonts and appropriate sizing
4. **WHEN** using colors **THEN** the system **SHALL** provide sufficient contrast for readability
5. **WHEN** handling interactions **THEN** the system **SHALL** provide clear visual feedback
6. **WHEN** managing focus **THEN** the system **SHALL** show clear focus indicators
7. **WHEN** providing tooltips **THEN** the system **SHALL** support contextual help information
8. **WHEN** handling errors **THEN** the system **SHALL** provide clear error messages and recovery options

### **Requirement 15: Thread Safety and Concurrency**

**User Story:** As a multi-threaded application, I want thread-safe UI operations, so that the interface remains stable when accessed from different threads.

#### **Acceptance Criteria**

1. **WHEN** accessing widgets from multiple threads **THEN** the system **SHALL** protect shared state appropriately
2. **WHEN** updating UI from audio thread **THEN** the system **SHALL** handle cross-thread updates safely
3. **WHEN** handling events **THEN** the system **SHALL** ensure thread-safe event processing
4. **WHEN** managing widget lifecycle **THEN** the system **SHALL** handle concurrent creation/destruction safely
5. **WHEN** rendering **THEN** the system **SHALL** coordinate rendering operations across threads
6. **WHEN** managing resources **THEN** the system **SHALL** use thread-safe resource management
7. **WHEN** handling cleanup **THEN** destructors **SHALL** ensure no operations are in progress
8. **WHEN** debugging **THEN** logging operations **SHALL** be thread-safe