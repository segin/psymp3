# Rect Usage Analysis - PsyMP3 Codebase

## Current Rect Implementation

### File Structure
- **Header**: `include/rect.h`
- **Implementation**: `src/rect.cpp`
- **Backup Files Created**: `include/rect.h.backup`, `src/rect.cpp.backup`

### Current Class Structure
```cpp
class Rect {
private:
    int16_t m_x;        // X coordinate (signed for negative positions)
    int16_t m_y;        // Y coordinate (signed for negative positions)
    uint16_t m_width;   // Width (unsigned, always positive)
    uint16_t m_height;  // Height (unsigned, always positive)

public:
    Rect();                                          // Default constructor
    Rect(int16_t x, int16_t y, uint16_t w, uint16_t h); // Full constructor
    Rect(uint16_t w, uint16_t h);                   // Width/height only (x=0, y=0)
    ~Rect();                                        // Destructor
    
    // Getters (inline)
    int16_t x() const { return m_x; }
    int16_t y() const { return m_y; }
    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }
    
    // Setters (inline)
    void x(int16_t val) { m_x = val; }
    void y(int16_t val) { m_y = val; }
    void width(uint16_t a) { m_width = a; }
    void height(uint16_t a) { m_height = a; }
};
```

## Critical Usage Patterns Identified

### 1. Widget System (Primary Usage)

**File**: `include/Widget.h`, `src/Widget.cpp`

**Usage Patterns**:
- **Member Variable**: `Rect m_pos` - stores widget position and size
- **Method Parameters**: `const Rect& position` in constructors and setters
- **Return Values**: `const Rect& getPos()` - returns position reference
- **Coordinate Calculations**: Extensive use of `.x()`, `.y()`, `.width()`, `.height()` methods

**Critical Code Paths**:
```cpp
// Widget positioning
void setPos(const Rect& position);
const Rect& getPos() const { return m_pos; }

// Mouse event handling - hit testing
if (relative_x >= child_pos.x() && relative_x < child_pos.x() + child_pos.width() &&
    relative_y >= child_pos.y() && relative_y < child_pos.y() + child_pos.height())

// Coordinate transformation
Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                  parent_absolute_pos.y() + m_pos.y(),
                  m_pos.width(), m_pos.height());

// Invalidation area calculations
Rect parent_area(m_pos.x() + area.x(), m_pos.y() + area.y(), 
                area.width(), area.height());
```

### 2. Surface Blitting Operations

**File**: `include/surface.h`, `src/surface.cpp`

**Usage Patterns**:
- **Method Parameter**: `void Blit(Surface& src, const Rect& rect)`
- **Position Specification**: Used to specify destination rectangle for blitting

**Critical Code Paths**:
```cpp
// Surface blitting with position
target.Blit(*this, m_pos);
target.Blit(*this, absolute_pos);
```

### 3. ApplicationWidget Window Management

**File**: `src/ApplicationWidget.cpp`

**Usage Patterns**:
- **Window Positioning**: Screen-sized rectangles for application bounds
- **Hit Testing**: Mouse coordinate checking against window bounds
- **Coordinate Transformation**: Converting between window and screen coordinates

**Critical Code Paths**:
```cpp
// Screen rectangle creation
Rect screen_rect(0, 0, display.width(), display.height());
setPos(screen_rect);

// Window hit testing
Rect window_pos = window->getPos();
if (relative_x >= window_pos.x() && relative_x < window_pos.x() + window_pos.width() &&
    relative_y >= window_pos.y() && relative_y < window_pos.y() + window_pos.height())

// Coordinate transformation for child windows
int window_relative_x = relative_x - window_pos.x();
int window_relative_y = relative_y - window_pos.y();
```

### 4. Player UI Layout

**File**: `src/player.cpp`

**Usage Patterns**:
- **Static Rectangles**: Predefined positions for UI elements
- **Dynamic Positioning**: Centering calculations for overlays
- **Label Positioning**: Fixed positions for text elements

**Critical Code Paths**:
```cpp
// Static rectangle for blitting
static const Rect blit_dest_rect(0, 0, 640, 350);
graph->Blit(fade_surface, blit_dest_rect);

// Dynamic centering calculations
const Rect& current_pos = m_toast->getPos();
Rect new_pos = current_pos;
new_pos.x((graph->width() - current_pos.width()) / 2);
new_pos.y(350 - current_pos.height() - 40);

// Fixed UI element positioning
add_label("artist",   Rect(1, 354, 200, 16));
add_label("title",    Rect(1, 369, 200, 16));
add_label("album",    Rect(1, 384, 200, 16));
spectrum_widget->setPos(Rect(0, 0, 640, 350));
progress_frame_widget->setPos(Rect(399, 370, 222, 16));
```

## Most Frequently Used Operations

### 1. Coordinate Access (Critical Path)
- `.x()`, `.y()`, `.width()`, `.height()` - Used extensively in hit testing and positioning
- **Performance Impact**: High - called in every mouse event and rendering cycle

### 2. Rectangle Construction (High Frequency)
- `Rect(x, y, width, height)` - Primary constructor for positioning
- `Rect(width, height)` - Used for size-only rectangles
- **Performance Impact**: Medium - object creation overhead

### 3. Coordinate Arithmetic (Critical Path)
- `pos.x() + pos.width()` - Right edge calculation
- `pos.y() + pos.height()` - Bottom edge calculation
- **Performance Impact**: High - calculated repeatedly for bounds checking

### 4. Assignment and Copying (Medium Frequency)
- `Rect new_pos = current_pos` - Copy construction for modifications
- `setPos(rect)` - Assignment of new positions
- **Performance Impact**: Low - simple member copying

## Compatibility Requirements

### 1. Memory Layout (CRITICAL)
- Current layout: `int16_t m_x, m_y; uint16_t m_width, m_height`
- **Must remain unchanged** - Widget system depends on this layout
- Size: 8 bytes total (2+2+2+2)

### 2. Method Signatures (CRITICAL)
- All existing public methods must maintain exact signatures
- Inline implementations must remain inline for performance
- Return types and parameter types must not change

### 3. Constructor Behavior (CRITICAL)
- Default constructor behavior must be preserved
- Two-parameter constructor (width, height) must set x=0, y=0
- Four-parameter constructor must work exactly as before

### 4. Performance Characteristics (CRITICAL)
- Getter methods must remain inline and fast
- No virtual functions (maintains POD-like behavior)
- No additional memory overhead

## Integration Points

### 1. Widget Hierarchy
- **Root**: ApplicationWidget uses Rect for screen bounds
- **Containers**: LayoutWidget, WindowWidget use Rect for child positioning
- **Leaves**: Label, ButtonWidget, etc. use Rect for individual positioning

### 2. Event System
- Mouse hit testing relies heavily on Rect coordinate calculations
- Event coordinate transformation uses Rect arithmetic extensively

### 3. Rendering Pipeline
- Surface blitting operations use Rect for destination positioning
- Widget rendering uses Rect for clipping and positioning

### 4. UI Layout System
- Fixed positioning in player.cpp uses static Rect objects
- Dynamic positioning uses Rect arithmetic for centering and alignment

## Risk Assessment

### High Risk Areas
1. **Mouse Event Handling** - Any changes to coordinate access could break hit testing
2. **Widget Positioning** - Changes to arithmetic operations could break layout
3. **Surface Blitting** - Changes to Rect parameter handling could break rendering

### Medium Risk Areas
1. **Constructor Behavior** - Changes could break existing object creation
2. **Copy Semantics** - Changes could affect assignment operations

### Low Risk Areas
1. **New Method Addition** - Should be safe if existing methods unchanged
2. **Internal Implementation** - Safe if public interface preserved

## Recommendations for Refactor

### 1. Preserve Existing Interface Completely
- Keep all current public methods with identical signatures
- Maintain inline implementations for performance-critical methods
- Preserve memory layout exactly

### 2. Add New Methods Carefully
- Group new methods logically in header file
- Use const-correctness for all new query methods
- Provide both mutating and non-mutating versions of transformations

### 3. Test Critical Paths Thoroughly
- Widget mouse event handling
- Surface blitting operations
- UI layout positioning
- ApplicationWidget window management

### 4. Performance Validation
- Ensure no regression in mouse event processing
- Validate rendering performance remains unchanged
- Check memory usage stays constant