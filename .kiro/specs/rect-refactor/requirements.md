# Requirements Document

## Introduction

The `Rect` class is a fundamental utility class in PsyMP3 that handles rectangle geometry for UI elements. Currently, it provides basic coordinate and size management but lacks many common geometric operations that would be useful throughout the codebase. This refactor aims to enhance the `Rect` class with additional functionality while maintaining full backward compatibility with existing code.

The class is extensively used throughout the widget system, surface operations, layout management, and UI positioning. Any changes must be carefully designed to avoid breaking the existing codebase while providing valuable new functionality.

## Requirements

### Requirement 1

**User Story:** As a developer working with UI positioning, I want enhanced geometric operations on rectangles, so that I can perform common calculations without writing repetitive code.

#### Acceptance Criteria

1. WHEN I need to check if a point is inside a rectangle THEN the Rect class SHALL provide a `contains(x, y)` method
2. WHEN I need to check if two rectangles overlap THEN the Rect class SHALL provide an `intersects(other_rect)` method
3. WHEN I need to find the intersection of two rectangles THEN the Rect class SHALL provide an `intersection(other_rect)` method
4. WHEN I need to find the union of two rectangles THEN the Rect class SHALL provide a `union(other_rect)` method
5. WHEN I need to expand a rectangle by a margin THEN the Rect class SHALL provide `expand(margin)` and `expand(dx, dy)` methods
6. WHEN I need to shrink a rectangle by a margin THEN the Rect class SHALL provide `shrink(margin)` and `shrink(dx, dy)` methods

### Requirement 2

**User Story:** As a developer working with coordinate systems, I want convenient access to rectangle edges and corners, so that I can easily work with positioning calculations.

#### Acceptance Criteria

1. WHEN I need the right edge coordinate THEN the Rect class SHALL provide a `right()` method returning `x() + width()`
2. WHEN I need the bottom edge coordinate THEN the Rect class SHALL provide a `bottom()` method returning `y() + height()`
3. WHEN I need the center point THEN the Rect class SHALL provide `centerX()` and `centerY()` methods
4. WHEN I need corner coordinates THEN the Rect class SHALL provide `topLeft()`, `topRight()`, `bottomLeft()`, and `bottomRight()` methods
5. WHEN I need to check if the rectangle is empty THEN the Rect class SHALL provide an `isEmpty()` method
6. WHEN I need the area of the rectangle THEN the Rect class SHALL provide an `area()` method

### Requirement 3

**User Story:** As a developer working with rectangle transformations, I want to move and resize rectangles easily, so that I can perform layout operations efficiently.

#### Acceptance Criteria

1. WHEN I need to move a rectangle by an offset THEN the Rect class SHALL provide `translate(dx, dy)` and `translated(dx, dy)` methods
2. WHEN I need to move a rectangle to a specific position THEN the Rect class SHALL provide `moveTo(x, y)` and `movedTo(x, y)` methods
3. WHEN I need to resize a rectangle THEN the Rect class SHALL provide `resize(width, height)` and `resized(width, height)` methods
4. WHEN I need to adjust rectangle size THEN the Rect class SHALL provide `adjust(dx, dy, dw, dh)` and `adjusted(dx, dy, dw, dh)` methods
5. WHEN I need to center a rectangle within another THEN the Rect class SHALL provide `centerIn(other_rect)` and `centeredIn(other_rect)` methods

### Requirement 4

**User Story:** As a developer working with existing code, I want all current functionality to remain unchanged, so that no existing code breaks during the refactor.

#### Acceptance Criteria

1. WHEN existing code uses current constructors THEN they SHALL continue to work exactly as before
2. WHEN existing code uses current getter methods THEN they SHALL return the same values as before
3. WHEN existing code uses current setter methods THEN they SHALL behave exactly as before
4. WHEN existing code creates Rect objects THEN the memory layout SHALL remain compatible
5. WHEN existing code passes Rect objects by value or reference THEN it SHALL continue to work without modification

### Requirement 5

**User Story:** As a developer working with modern C++, I want the Rect class to follow modern C++ best practices, so that it integrates well with standard library containers and algorithms.

#### Acceptance Criteria

1. WHEN I use Rect objects in containers THEN the class SHALL be properly copyable and movable
2. WHEN I compare Rect objects THEN the class SHALL provide equality and inequality operators
3. WHEN I debug Rect objects THEN the class SHALL provide a string representation method
4. WHEN I use Rect with standard algorithms THEN the class SHALL follow value semantics
5. WHEN I need to serialize Rect data THEN the class SHALL provide clear access to all member data

### Requirement 6

**User Story:** As a developer working with coordinate precision, I want to understand the coordinate system limitations, so that I can make informed decisions about precision requirements.

#### Acceptance Criteria

1. WHEN I work with negative coordinates THEN the class SHALL handle signed position values correctly
2. WHEN I work with large coordinate values THEN the class SHALL document any overflow behavior
3. WHEN I work with coordinate calculations THEN the class SHALL maintain precision within the limits of int16_t/uint16_t
4. WHEN coordinate values exceed type limits THEN the class SHALL behave predictably
5. WHEN I need higher precision coordinates THEN the class design SHALL allow for future extension to larger types

### Requirement 7

**User Story:** As a developer debugging UI layout issues, I want comprehensive validation and debugging support, so that I can quickly identify and fix positioning problems.

#### Acceptance Criteria

1. WHEN I need to validate rectangle data THEN the class SHALL provide an `isValid()` method
2. WHEN I need to debug rectangle values THEN the class SHALL provide a `toString()` method
3. WHEN I work with invalid rectangles THEN the class SHALL handle them gracefully
4. WHEN I need to normalize rectangle data THEN the class SHALL provide a `normalized()` method for negative dimensions
5. WHEN debugging layout issues THEN the class SHALL provide clear, readable output for all values