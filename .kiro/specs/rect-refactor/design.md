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

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Point Containment Correctness
*For any* rectangle and any point (x, y), the `contains(x, y)` method should return true if and only if the point lies within the rectangle bounds (x >= left && x < right && y >= top && y < bottom), with proper handling of empty rectangles returning false.

**Validates: Requirements 1.1**

### Property 2: Rectangle Intersection Detection
*For any* two rectangles A and B, `A.intersects(B)` should return true if and only if there exists at least one point that is contained in both rectangles, with empty rectangles never intersecting.

**Validates: Requirements 1.2**

### Property 3: Intersection Calculation Correctness
*For any* two rectangles A and B, the result of `A.intersection(B)` should be a rectangle C such that every point in C is contained in both A and B, and C is the largest such rectangle (or empty if no intersection exists).

**Validates: Requirements 1.3**

### Property 4: Union Bounding Box Property
*For any* two rectangles A and B, the result of `A.united(B)` should be the smallest rectangle C that contains both A and B completely, with empty rectangles being ignored in the calculation.

**Validates: Requirements 1.4**

### Property 5: Expansion Preserves Center
*For any* rectangle and any margin value, expanding the rectangle uniformly should preserve the center point (within integer division precision) while increasing dimensions by 2*margin in each direction.

**Validates: Requirements 1.5**

### Property 6: Shrinking Preserves Center
*For any* rectangle and any margin value (where margin < width/2 and margin < height/2), shrinking the rectangle uniformly should preserve the center point (within integer division precision) while decreasing dimensions by 2*margin in each direction.

**Validates: Requirements 1.6**

### Property 7: Edge Calculation Consistency
*For any* rectangle, the edge methods should satisfy: `right() == x() + width()`, `bottom() == y() + height()`, `left() == x()`, and `top() == y()`.

**Validates: Requirements 2.1, 2.2**

### Property 8: Center Point Calculation
*For any* rectangle, the center point should be calculated as `(x() + width()/2, y() + height()/2)` with integer division, and the `center()` method should return the same values as `(centerX(), centerY())`.

**Validates: Requirements 2.3**

### Property 9: Corner Coordinate Derivation
*For any* rectangle, corner methods should return coordinates derived from edges: `topLeft() == (left(), top())`, `topRight() == (right(), top())`, `bottomLeft() == (left(), bottom())`, `bottomRight() == (right(), bottom())`.

**Validates: Requirements 2.4**

### Property 10: Empty Rectangle Detection
*For any* rectangle, `isEmpty()` should return true if and only if `width() == 0 || height() == 0`.

**Validates: Requirements 2.5**

### Property 11: Area Calculation
*For any* rectangle, `area()` should equal `width() * height()` computed as uint32_t to prevent overflow.

**Validates: Requirements 2.6**

### Property 12: Translation Preserves Dimensions
*For any* rectangle and any offset (dx, dy), translating the rectangle should preserve width and height while changing position by exactly (dx, dy), with both in-place and const versions producing equivalent results.

**Validates: Requirements 3.1**

### Property 13: MoveTo Sets Absolute Position
*For any* rectangle and any target position (x, y), `moveTo(x, y)` should set the rectangle's position to exactly (x, y) while preserving dimensions, with both in-place and const versions producing equivalent results.

**Validates: Requirements 3.2**

### Property 14: Resize Preserves Position
*For any* rectangle and any dimensions (w, h), resizing should preserve the top-left position while setting dimensions to exactly (w, h), with both in-place and const versions producing equivalent results.

**Validates: Requirements 3.3**

### Property 15: Adjust Modifies Position and Size
*For any* rectangle and any adjustments (dx, dy, dw, dh), the `adjust()` method should modify position by (dx, dy) and dimensions by (dw, dh), with proper clamping to prevent negative dimensions.

**Validates: Requirements 3.4**

### Property 16: Centering Calculation
*For any* rectangle R and container C, after `R.centerIn(C)`, the rectangle R should be positioned such that `R.centerX() == C.centerX()` and `R.centerY() == C.centerY()` (within ±1 pixel due to integer division).

**Validates: Requirements 3.5**

### Property 17: Backward Compatibility - API Preservation
*For any* rectangle created with existing constructors and manipulated with existing getters/setters, the behavior should be identical to the original implementation, with all existing code paths producing the same results.

**Validates: Requirements 4.1, 4.2, 4.3**

### Property 18: Container Compatibility
*For any* Rect object, it should be usable in standard containers (std::vector, std::map, etc.) with proper copy/move semantics, and should work correctly with standard algorithms (std::sort, std::find, etc.).

**Validates: Requirements 5.1, 5.4**

### Property 19: Equality Comparison Correctness
*For any* two rectangles A and B, `A == B` should return true if and only if all four member values are equal (x, y, width, height), and `A != B` should return the logical negation of `A == B`.

**Validates: Requirements 5.2**

### Property 20: String Representation Accuracy
*For any* rectangle, `toString()` should produce a string in the format "Rect(x, y, width, height)" with all values correctly represented, providing clear debugging output.

**Validates: Requirements 5.3, 7.2**

### Property 21: Negative Coordinate Handling
*For any* rectangle with negative coordinates, all geometric operations (contains, intersects, intersection, union) should handle negative values correctly according to the standard coordinate system.

**Validates: Requirements 6.1**

### Property 22: Overflow Protection
*For any* operation that could cause coordinate overflow (addition, subtraction), the result should be clamped to the valid range of int16_t or uint16_t, ensuring predictable behavior at boundary conditions.

**Validates: Requirements 6.2, 6.4**

### Property 23: Precision Maintenance
*For any* coordinate calculation within the valid range of int16_t/uint16_t, the result should maintain maximum precision without unnecessary truncation, with only unavoidable integer division causing precision loss.

**Validates: Requirements 6.3**

### Property 24: Validation Correctness
*For any* rectangle, `isValid()` should return true if and only if the rectangle has non-zero dimensions and all coordinates are within valid ranges without overflow conditions.

**Validates: Requirements 7.1**

### Property 25: Invalid Rectangle Handling
*For any* invalid rectangle (negative dimensions, overflow conditions), all geometric operations should handle it gracefully without crashes, returning sensible defaults (typically empty rectangles or false for boolean queries).

**Validates: Requirements 7.3**

### Property 26: Normalization Correctness
*For any* rectangle with negative width or height, `normalized()` should return an equivalent rectangle with positive dimensions by adjusting the position accordingly, such that the same geometric area is represented.

**Validates: Requirements 7.4**

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