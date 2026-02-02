# Implementation Plan: Rect Class Refactor

## Overview

This implementation plan refactors the Rect class to add comprehensive geometric operations, utility methods, and modern C++ features while maintaining 100% backward compatibility. The plan follows a logical progression from foundational infrastructure through core functionality to testing and validation.

## Tasks

- [x] 1. Set up testing infrastructure and RapidCheck integration
  - Add RapidCheck dependency to configure.ac for property-based testing
  - Create tests/test_rect_properties.cpp for property-based tests
  - Create tests/test_rect_unit.cpp for unit tests
  - Set up test fixtures and helper functions for Rect testing
  - Add test targets to tests/Makefile.am
  - _Requirements: 5.1, 5.4_

- [ ] 2. Implement foundational utility methods
  - [x] 2.1 Implement edge coordinate accessors
    - Add `left()` returning `x()` (alias for consistency)
    - Add `top()` returning `y()` (alias for consistency)
    - Add `right()` returning `x() + width()`
    - Add `bottom()` returning `y() + height()`
    - _Requirements: 2.1, 2.2_
  
  - [x] 2.2 Write property test for edge calculation consistency
    - **Property 7: Edge Calculation Consistency**
    - **Validates: Requirements 2.1, 2.2**
  
  - [x] 2.3 Implement center point calculations
    - Add `centerX()` returning `x() + width() / 2`
    - Add `centerY()` returning `y() + height() / 2`
    - Add `center()` returning `std::pair<int16_t, int16_t>`
    - _Requirements: 2.3_
  
  - [x] 2.4 Write property test for center point calculation
    - **Property 8: Center Point Calculation**
    - **Validates: Requirements 2.3**
  
  - [~] 2.5 Implement corner coordinate methods
    - Add `topLeft()` returning `(left(), top())`
    - Add `topRight()` returning `(right(), top())`
    - Add `bottomLeft()` returning `(left(), bottom())`
    - Add `bottomRight()` returning `(right(), bottom())`
    - _Requirements: 2.4_
  
  - [~] 2.6 Write property test for corner coordinate derivation
    - **Property 9: Corner Coordinate Derivation**
    - **Validates: Requirements 2.4**

- [ ] 3. Implement validation and area methods
  - [~] 3.1 Implement isEmpty and area methods
    - Add `isEmpty()` returning `width() == 0 || height() == 0`
    - Add `area()` returning `static_cast<uint32_t>(width()) * height()`
    - _Requirements: 2.5, 2.6_
  
  - [~] 3.2 Write property test for empty rectangle detection
    - **Property 10: Empty Rectangle Detection**
    - **Validates: Requirements 2.5**
  
  - [~] 3.3 Write property test for area calculation
    - **Property 11: Area Calculation**
    - **Validates: Requirements 2.6**
  
  - [~] 3.4 Implement validation and normalization
    - Add `isValid()` checking for valid coordinate ranges and non-zero dimensions
    - Add `normalized()` returning rectangle with positive dimensions
    - Add `normalize()` for in-place normalization
    - _Requirements: 7.1, 7.4_
  
  - [~] 3.5 Write property test for validation correctness
    - **Property 24: Validation Correctness**
    - **Validates: Requirements 7.1**
  
  - [~] 3.6 Write property test for normalization correctness
    - **Property 26: Normalization Correctness**
    - **Validates: Requirements 7.4**

- [ ] 4. Implement modern C++ operators and string representation
  - [~] 4.1 Implement comparison operators
    - Add `operator==(const Rect& other)` comparing all members
    - Add `operator!=(const Rect& other)` as negation of equality
    - _Requirements: 5.2_
  
  - [~] 4.2 Write property test for equality comparison correctness
    - **Property 19: Equality Comparison Correctness**
    - **Validates: Requirements 5.2**
  
  - [~] 4.3 Implement string representation
    - Add `toString()` returning "Rect(x, y, width, height)" format
    - Include proper formatting for negative coordinates
    - _Requirements: 5.3, 7.2_
  
  - [~] 4.4 Write property test for string representation accuracy
    - **Property 20: String Representation Accuracy**
    - **Validates: Requirements 5.3, 7.2**
  
  - [~] 4.5 Write unit tests for container compatibility
    - Test Rect in std::vector, std::map, std::set
    - Test with std::sort, std::find algorithms
    - Verify copy/move semantics work correctly
    - _Requirements: 5.1, 5.4_

- [ ] 5. Implement point and rectangle containment
  - [~] 5.1 Implement point containment
    - Add `contains(int16_t x, int16_t y)` checking point-in-rectangle
    - Handle empty rectangles (return false)
    - Handle boundary conditions correctly
    - _Requirements: 1.1_
  
  - [~] 5.2 Write property test for point containment correctness
    - **Property 1: Point Containment Correctness**
    - **Validates: Requirements 1.1**
  
  - [~] 5.3 Implement rectangle containment
    - Add `contains(const Rect& other)` checking if other is fully inside
    - Handle empty rectangles appropriately
    - _Requirements: 1.1_
  
  - [~] 5.4 Write unit tests for containment edge cases
    - Test empty rectangle containment
    - Test boundary point containment
    - Test negative coordinate containment
    - _Requirements: 1.1_

- [ ] 6. Implement intersection operations
  - [~] 6.1 Implement intersection detection
    - Add `intersects(const Rect& other)` checking for overlap
    - Handle empty rectangles (return false)
    - Handle touching edges correctly
    - _Requirements: 1.2_
  
  - [~] 6.2 Write property test for rectangle intersection detection
    - **Property 2: Rectangle Intersection Detection**
    - **Validates: Requirements 1.2**
  
  - [~] 6.3 Implement intersection calculation
    - Add `intersection(const Rect& other)` returning overlapping area
    - Return empty Rect(0, 0, 0, 0) for non-overlapping rectangles
    - Handle partial and full containment cases
    - _Requirements: 1.3_
  
  - [~] 6.4 Write property test for intersection calculation correctness
    - **Property 3: Intersection Calculation Correctness**
    - **Validates: Requirements 1.3**
  
  - [~] 6.5 Write unit tests for intersection edge cases
    - Test non-overlapping rectangles
    - Test touching edges (no overlap)
    - Test full containment
    - Test partial overlap scenarios
    - _Requirements: 1.2, 1.3_

- [ ] 7. Implement union operations
  - [~] 7.1 Implement bounding box union
    - Add `united(const Rect& other)` returning smallest containing rectangle
    - Handle empty rectangles (ignore in calculation)
    - Handle coordinate overflow safely
    - _Requirements: 1.4_
  
  - [~] 7.2 Write property test for union bounding box property
    - **Property 4: Union Bounding Box Property**
    - **Validates: Requirements 1.4**
  
  - [~] 7.3 Write unit tests for union edge cases
    - Test union with empty rectangles
    - Test union with negative coordinates
    - Test union with maximum coordinate values
    - _Requirements: 1.4_

- [ ] 8. Implement expansion and shrinking operations
  - [~] 8.1 Implement uniform expansion
    - Add `expand(int16_t margin)` for in-place uniform expansion
    - Add `expanded(int16_t margin)` returning expanded rectangle
    - Expand by margin in all directions (total 2*margin per dimension)
    - _Requirements: 1.5_
  
  - [~] 8.2 Implement directional expansion
    - Add `expand(int16_t dx, int16_t dy)` for directional expansion
    - Add `expanded(int16_t dx, int16_t dy)` returning expanded rectangle
    - _Requirements: 1.5_
  
  - [~] 8.3 Write property test for expansion preserves center
    - **Property 5: Expansion Preserves Center**
    - **Validates: Requirements 1.5**
  
  - [~] 8.4 Implement uniform shrinking
    - Add `shrink(int16_t margin)` for in-place uniform shrinking
    - Add `shrunk(int16_t margin)` returning shrunk rectangle
    - Clamp to prevent negative dimensions
    - _Requirements: 1.6_
  
  - [~] 8.5 Implement directional shrinking
    - Add `shrink(int16_t dx, int16_t dy)` for directional shrinking
    - Add `shrunk(int16_t dx, int16_t dy)` returning shrunk rectangle
    - _Requirements: 1.6_
  
  - [~] 8.6 Write property test for shrinking preserves center
    - **Property 6: Shrinking Preserves Center**
    - **Validates: Requirements 1.6**
  
  - [~] 8.7 Write unit tests for expansion/shrinking edge cases
    - Test expansion with negative margins
    - Test shrinking beyond zero dimensions
    - Test with maximum coordinate values
    - _Requirements: 1.5, 1.6_

- [ ] 9. Implement translation operations
  - [~] 9.1 Implement relative translation
    - Add `translate(int16_t dx, int16_t dy)` for in-place movement
    - Add `translated(int16_t dx, int16_t dy)` returning moved rectangle
    - Handle coordinate overflow safely
    - _Requirements: 3.1_
  
  - [~] 9.2 Write property test for translation preserves dimensions
    - **Property 12: Translation Preserves Dimensions**
    - **Validates: Requirements 3.1**
  
  - [~] 9.3 Implement absolute positioning
    - Add `moveTo(int16_t x, int16_t y)` for in-place positioning
    - Add `movedTo(int16_t x, int16_t y)` returning repositioned rectangle
    - _Requirements: 3.2_
  
  - [~] 9.4 Write property test for moveTo sets absolute position
    - **Property 13: MoveTo Sets Absolute Position**
    - **Validates: Requirements 3.2**
  
  - [~] 9.5 Write unit tests for translation edge cases
    - Test translation with negative offsets
    - Test translation causing overflow
    - Test moveTo with negative coordinates
    - _Requirements: 3.1, 3.2_

- [ ] 10. Implement resizing operations
  - [~] 10.1 Implement resize methods
    - Add `resize(uint16_t width, uint16_t height)` for in-place resizing
    - Add `resized(uint16_t width, uint16_t height)` returning resized rectangle
    - Preserve top-left position
    - _Requirements: 3.3_
  
  - [~] 10.2 Write property test for resize preserves position
    - **Property 14: Resize Preserves Position**
    - **Validates: Requirements 3.3**
  
  - [~] 10.3 Write unit tests for resize edge cases
    - Test resize to zero dimensions
    - Test resize to maximum dimensions
    - _Requirements: 3.3_

- [ ] 11. Implement combined adjustment operations
  - [~] 11.1 Implement adjust methods
    - Add `adjust(int16_t dx, int16_t dy, int16_t dw, int16_t dh)` for combined changes
    - Add `adjusted(int16_t dx, int16_t dy, int16_t dw, int16_t dh)` returning adjusted rectangle
    - Clamp dimensions to prevent negative values
    - Handle coordinate overflow
    - _Requirements: 3.4_
  
  - [~] 11.2 Write property test for adjust modifies position and size
    - **Property 15: Adjust Modifies Position and Size**
    - **Validates: Requirements 3.4**
  
  - [~] 11.3 Write unit tests for adjust edge cases
    - Test adjust causing negative dimensions
    - Test adjust with overflow conditions
    - _Requirements: 3.4_

- [ ] 12. Implement centering operations
  - [~] 12.1 Implement centerIn methods
    - Add `centerIn(const Rect& container)` for in-place centering
    - Add `centeredIn(const Rect& container)` returning centered rectangle
    - Handle cases where rectangle is larger than container
    - Calculate center with integer division precision
    - _Requirements: 3.5_
  
  - [~] 12.2 Write property test for centering calculation
    - **Property 16: Centering Calculation**
    - **Validates: Requirements 3.5**
  
  - [~] 12.3 Write unit tests for centering edge cases
    - Test centering larger rectangle in smaller container
    - Test centering with odd dimensions
    - Test centering with negative coordinates
    - _Requirements: 3.5_

- [ ] 13. Implement coordinate system safety and overflow protection
  - [~] 13.1 Add safe arithmetic helper functions
    - Implement `safeAdd(int16_t a, int16_t b)` with overflow clamping
    - Implement `safeAdd(uint16_t a, uint16_t b)` with overflow clamping
    - Implement `safeSub(int16_t a, int16_t b)` with underflow clamping
    - Document overflow behavior in comments
    - _Requirements: 6.2, 6.4_
  
  - [~] 13.2 Write property test for negative coordinate handling
    - **Property 21: Negative Coordinate Handling**
    - **Validates: Requirements 6.1**
  
  - [~] 13.3 Write property test for overflow protection
    - **Property 22: Overflow Protection**
    - **Validates: Requirements 6.2, 6.4**
  
  - [~] 13.4 Write property test for precision maintenance
    - **Property 23: Precision Maintenance**
    - **Validates: Requirements 6.3**
  
  - [~] 13.5 Write unit tests for boundary conditions
    - Test with INT16_MIN and INT16_MAX coordinates
    - Test with UINT16_MAX dimensions
    - Test operations at boundary values
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

- [ ] 14. Implement invalid rectangle handling
  - [~] 14.1 Add graceful handling for invalid rectangles
    - Ensure geometric operations return sensible defaults for invalid inputs
    - Document behavior for invalid rectangles in method comments
    - _Requirements: 7.3_
  
  - [~] 14.2 Write property test for invalid rectangle handling
    - **Property 25: Invalid Rectangle Handling**
    - **Validates: Requirements 7.3**
  
  - [~] 14.3 Write unit tests for invalid rectangle scenarios
    - Test operations on rectangles with negative dimensions
    - Test operations on rectangles with overflow conditions
    - Verify no crashes or undefined behavior
    - _Requirements: 7.3_

- [ ] 15. Verify backward compatibility
  - [~] 15.1 Create backward compatibility test suite
    - Test all existing constructors produce same results
    - Test all existing getters return same values
    - Test all existing setters behave identically
    - Verify memory layout unchanged (sizeof, alignment)
    - _Requirements: 4.1, 4.2, 4.3, 4.4_
  
  - [~] 15.2 Write property test for backward compatibility
    - **Property 17: Backward Compatibility - API Preservation**
    - **Validates: Requirements 4.1, 4.2, 4.3**

  - [~] 15.3 Write property test for container compatibility
    - **Property 18: Container Compatibility**
    - **Validates: Requirements 5.1, 5.4**

- [~] 16. Checkpoint - Ensure all tests pass
  - Run all unit tests and verify they pass
  - Run all property-based tests with minimum 100 iterations each
  - Fix any failing tests before proceeding
  - Ask user if questions arise

- [ ] 17. Integration testing with Widget system
  - [~] 17.1 Test with Widget positioning
    - Verify Widget::setRect() works with enhanced Rect
    - Test Widget::rect() returns compatible Rect objects
    - Test mouse hit detection with contains() method
    - _Requirements: 4.5_
  
  - [~] 17.2 Test with Surface operations
    - Verify Surface::blit() works with enhanced Rect
    - Test clipping operations use intersection correctly
    - Test surface coordinate transformations
    - _Requirements: 4.5_
  
  - [~] 17.3 Test with layout operations
    - Verify LayoutWidget uses new methods correctly
    - Test WindowWidget positioning with new operations
    - Test ToastWidget centering with centerIn()
    - _Requirements: 4.5_
  
  - [~] 17.4 Write integration tests for Widget system
    - Test complete widget positioning workflow
    - Test layout calculations with new methods
    - Test event handling with enhanced containment
    - _Requirements: 4.5_

- [ ] 18. Performance validation and optimization
  - [~] 18.1 Benchmark critical operations
    - Profile contains() performance in hot paths
    - Profile intersects() performance in rendering
    - Profile translation operations in layout
    - Compare performance with original implementation
    - _Requirements: 4.5, 5.4_
  
  - [~] 18.2 Optimize performance-critical methods
    - Ensure inline methods remain inline
    - Optimize frequently-called geometric operations
    - Verify no performance regression in Widget system
    - _Requirements: 4.5, 5.4_

- [ ] 19. Documentation and header organization
  - [~] 19.1 Organize header file structure
    - Group existing API at top for visibility
    - Group new methods by category (geometric, transformation, utility)
    - Add comprehensive Doxygen-style documentation
    - Document coordinate system and limitations
    - _Requirements: 4.1, 4.2, 4.3_
  
  - [~] 19.2 Add usage examples and best practices
    - Document common usage patterns
    - Add examples for complex operations
    - Document edge cases and error handling
    - Include performance notes for hot paths
    - _Requirements: 6.5, 7.2, 7.5_

- [ ] 20. Final validation and cleanup
  - [~] 20.1 Run complete test suite
    - Execute all unit tests
    - Execute all property-based tests (100+ iterations each)
    - Execute all integration tests
    - Verify 100% test pass rate
  
  - [~] 20.2 Compile full codebase
    - Build entire PsyMP3 with enhanced Rect
    - Verify no compilation warnings
    - Verify no linking errors
    - Test application runs correctly
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_
  
  - [~] 20.3 Final code review
    - Review all new code for style consistency
    - Verify all requirements are met
    - Verify all properties are tested
    - Ensure documentation is complete

## Notes

- Tasks marked with `*` are optional property-based and integration tests
- Each property test should run minimum 100 iterations
- Property tests use RapidCheck for random input generation
- Unit tests focus on specific examples and edge cases
- Integration tests verify compatibility with existing Widget system
- All 26 correctness properties from the design document are covered
- Backward compatibility is verified throughout implementation
- Performance validation ensures no regression in hot paths