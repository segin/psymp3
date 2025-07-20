# Design Document

## Overview

The Rect class refactor enhances the existing rectangle utility class with comprehensive geometric operations while maintaining 100% backward compatibility. The design focuses on adding value-semantic operations, modern C++ best practices, and debugging support without changing the existing API or memory layout.

The refactor transforms Rect from a simple data container into a full-featured geometric utility class suitable for complex UI layout operations, collision detection, and coordinate system transformations.

## Architecture

### Current Architecture Analysis

The existing Rect class has a simple structure:
- Private members: `int16_t m_x, m_y` and `uint16_t m_width, m_height`
- Basic constructors for different initialization patterns
- Simple getter/setter methods with inline implementations
- No complex operations or validation

### Enhanced Architecture

The enhanced Rect class maintains the same memory layout and adds:
- **Geometric Operations Layer**: Methods for intersection, union, containment testing
- **Transformation Layer**: Methods for moving, resizing, and adjusting rectangles
- **Utility Layer**: Convenience methods for edges, corners, and center points
- **Validation Layer**: Methods for checking validity and normalizing data
- **Modern C++ Layer**: Operators, string conversion, and standard library compatibility

```
┌─────────────────────────────────────────┐
│           Modern C++ Layer              │
│  (operators, string conversion, etc.)   │
├─────────────────────────────────────────┤
│           Validation Layer              │
│    (isValid, normalized, toString)      │
├─────────────────────────────────────────┤
│           Utility Layer                 │
│  (edges, corners, center, area, etc.)   │
├─────────────────────────────────────────┤
│         Transformation Layer            │
│ (translate, resize, adjust, center)     │
├─────────────────────────────────────────┤
│        Geometric Operations             │
│ (contains, intersects, union, etc.)     │
├─────────────────────────────────────────┤
│         Existing Core API               │
│    (constructors, getters, setters)     │
└─────────────────────────────────────────┘
```

## Components and Interfaces

### Core Data Members (Unchanged)
```cpp
private:
    int16_t m_x;        // X coordinate (signed for negative positions)
    int16_t m_y;        // Y coordinate (signed for negative positions)  
    uint16_t m_width;   // Width (unsigned, always positive)
    uint16_t m_height;  // Height (unsigned, always positive)
```

### Existing Interface (Preserved)
```cpp
// Constructors (unchanged)
Rect();
Rect(int16_t x, int16_t y, uint16_t w, uint16_t h);
Rect(uint16_t w, uint16_t h);

// Getters/Setters (unchanged)
int16_t x() const;
int16_t y() const;
uint16_t width() const;
uint16_t height() const;
void x(int16_t val);
void y(int16_t val);
void width(uint16_t a);
void height(uint16_t a);
```

### New Geometric Operations Interface
```cpp
// Point containment
bool contains(int16_t x, int16_t y) const;
bool contains(const Rect& other) const;

// Rectangle intersection
bool intersects(const Rect& other) const;
Rect intersection(const Rect& other) const;
Rect united(const Rect& other) const;  // Note: 'union' is a C++ keyword

// Expansion/contraction
Rect expanded(int16_t margin) const;
Rect expanded(int16_t dx, int16_t dy) const;
Rect shrunk(int16_t margin) const;
Rect shrunk(int16_t dx, int16_t dy) const;
void expand(int16_t margin);
void expand(int16_t dx, int16_t dy);
void shrink(int16_t margin);
void shrink(int16_t dx, int16_t dy);
```

### New Utility Interface
```cpp
// Edge and corner access
int16_t left() const;    // Same as x()
int16_t top() const;     // Same as y()
int16_t right() const;   // x() + width()
int16_t bottom() const;  // y() + height()

// Center points
int16_t centerX() const;
int16_t centerY() const;
std::pair<int16_t, int16_t> center() const;

// Area and validation
uint32_t area() const;
bool isEmpty() const;
bool isValid() const;
```

### New Transformation Interface
```cpp
// Translation (moving)
void translate(int16_t dx, int16_t dy);
Rect translated(int16_t dx, int16_t dy) const;
void moveTo(int16_t x, int16_t y);
Rect movedTo(int16_t x, int16_t y) const;

// Resizing
void resize(uint16_t width, uint16_t height);
Rect resized(uint16_t width, uint16_t height) const;

// Combined adjustments
void adjust(int16_t dx, int16_t dy, int16_t dw, int16_t dh);
Rect adjusted(int16_t dx, int16_t dy, int16_t dw, int16_t dh) const;

// Centering operations
void centerIn(const Rect& container);
Rect centeredIn(const Rect& container) const;
```

### New Modern C++ Interface
```cpp
// Comparison operators
bool operator==(const Rect& other) const;
bool operator!=(const Rect& other) const;

// String representation
std::string toString() const;

// Normalization (handle negative dimensions)
Rect normalized() const;
void normalize();
```

## Data Models

### Coordinate System Model
- **Origin**: Top-left corner (0, 0)
- **X-axis**: Increases rightward (can be negative for off-screen positioning)
- **Y-axis**: Increases downward (can be negative for off-screen positioning)
- **Width/Height**: Always positive values representing size
- **Precision**: Limited by int16_t range (-32,768 to 32,767) for coordinates
- **Size Limits**: Limited by uint16_t range (0 to 65,535) for dimensions

### Rectangle Validity Model
A rectangle is considered valid when:
- Width and height are non-zero
- Coordinates are within int16_t range
- No integer overflow occurs in calculations

### Intersection Model
- **Empty Intersection**: Returns Rect(0, 0, 0, 0) when rectangles don't overlap
- **Partial Intersection**: Returns the overlapping area
- **Full Containment**: Returns the smaller rectangle when one contains the other

### Union Model
- **Bounding Box**: Returns the smallest rectangle that contains both input rectangles
- **Empty Rectangles**: Ignores empty rectangles in union calculations
- **Coordinate Overflow**: Handles potential overflow in coordinate calculations

## Error Handling

### Overflow Protection
```cpp
// Safe coordinate calculations with overflow checking
int16_t safeAdd(int16_t a, int16_t b) const;
uint16_t safeAdd(uint16_t a, uint16_t b) const;
```

### Invalid Rectangle Handling
- Methods return sensible defaults for invalid rectangles
- `isEmpty()` returns true for zero-width or zero-height rectangles
- `isValid()` performs comprehensive validation
- Geometric operations handle edge cases gracefully

### Boundary Conditions
- Negative coordinates are supported for positioning
- Zero-size rectangles are handled consistently
- Maximum coordinate values are respected
- Intersection with empty rectangles returns empty result

## Testing Strategy

### Unit Test Categories

1. **Backward Compatibility Tests**
   - Verify all existing functionality remains unchanged
   - Test constructor behavior matches original implementation
   - Validate getter/setter behavior is identical

2. **Geometric Operation Tests**
   - Point containment testing with edge cases
   - Rectangle intersection with various overlap scenarios
   - Union operations with different rectangle configurations
   - Expansion/shrinking with positive and negative values

3. **Transformation Tests**
   - Translation with positive and negative offsets
   - Resizing with various dimension changes
   - Combined adjustment operations
   - Centering operations within containers

4. **Edge Case Tests**
   - Empty rectangles (zero width or height)
   - Negative coordinates
   - Maximum coordinate values
   - Overflow conditions
   - Invalid rectangle states

5. **Modern C++ Feature Tests**
   - Equality/inequality operators
   - String representation accuracy
   - Copy/move semantics
   - Container compatibility

### Integration Test Strategy
- Test with existing Widget system to ensure no regressions
- Validate Surface blitting operations continue to work
- Test layout operations in LayoutWidget
- Verify mouse hit testing in event handling code

### Performance Test Considerations
- Benchmark geometric operations for performance impact
- Ensure inline methods maintain performance
- Test memory usage remains unchanged
- Validate no performance regression in hot paths

## Implementation Notes

### Header Organization
- Keep existing public interface at the top for clarity
- Group new methods by functionality
- Maintain inline implementations for simple operations
- Use const-correctness throughout

### Const-Correctness Strategy
- All query methods are const
- Mutating methods clearly separated from const methods
- Provide both mutating and non-mutating versions of transformations

### Naming Conventions
- Follow existing camelCase convention
- Use descriptive names for new methods
- Avoid abbreviations where possible
- Maintain consistency with existing codebase style

### Memory Layout Preservation
- No new member variables
- No virtual functions (maintain POD-like behavior)
- Same size and alignment as original class
- Compatible with existing serialization if any