# Implementation Plan

- [x] 1. Backup and analyze current Rect usage patterns
  - Create backup of current rect.h and rect.cpp files
  - Document all current usage patterns in the codebase to ensure compatibility
  - Identify critical paths where Rect is used most frequently
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [x] 2. Implement utility and edge access methods
  - [x] 2.1 Add edge coordinate methods (left, top, right, bottom)
    - Implement `left()` and `top()` as aliases to existing `x()` and `y()`
    - Implement `right()` returning `x() + width()`
    - Implement `bottom()` returning `y() + height()`
    - _Requirements: 2.1, 2.2_

  - [x] 2.2 Add center point calculation methods
    - Implement `centerX()` returning `x() + width() / 2`
    - Implement `centerY()` returning `y() + height() / 2`
    - Implement `center()` returning std::pair of center coordinates
    - _Requirements: 2.3_

  - [x] 2.3 Add area and validation methods
    - Implement `area()` returning `width() * height()`
    - Implement `isEmpty()` checking for zero width or height
    - Implement `isValid()` for comprehensive rectangle validation
    - _Requirements: 2.5, 2.6, 7.1_

- [x] 3. Implement geometric operation methods
  - [x] 3.1 Add point and rectangle containment methods
    - Implement `contains(int16_t x, int16_t y)` for point-in-rectangle testing
    - Implement `contains(const Rect& other)` for rectangle containment
    - Handle edge cases for empty rectangles and boundary conditions
    - _Requirements: 1.1_

  - [x] 3.2 Add rectangle intersection methods
    - Implement `intersects(const Rect& other)` for overlap detection
    - Implement `intersection(const Rect& other)` returning intersection rectangle
    - Handle non-overlapping rectangles by returning empty Rect(0, 0, 0, 0)
    - _Requirements: 1.2, 1.3_

  - [x] 3.3 Add rectangle union method
    - Implement `united(const Rect& other)` for bounding box calculation
    - Handle empty rectangles in union operations
    - Ensure proper coordinate overflow handling
    - _Requirements: 1.4_

- [x] 4. Implement expansion and contraction methods
  - [x] 4.1 Add expansion methods with margin parameters
    - Implement `expand(int16_t margin)` for uniform expansion
    - Implement `expand(int16_t dx, int16_t dy)` for directional expansion
    - Implement const versions `expanded()` that return new rectangles
    - _Requirements: 1.5_

  - [x] 4.2 Add shrinking methods with margin parameters
    - Implement `shrink(int16_t margin)` for uniform shrinking
    - Implement `shrink(int16_t dx, int16_t dy)` for directional shrinking
    - Implement const versions `shrunk()` that return new rectangles
    - Handle cases where shrinking would create negative dimensions
    - _Requirements: 1.6_

- [x] 5. Implement transformation methods
  - [x] 5.1 Add translation methods
    - Implement `translate(int16_t dx, int16_t dy)` for in-place movement
    - Implement `translated(int16_t dx, int16_t dy)` returning moved rectangle
    - Implement `moveTo(int16_t x, int16_t y)` for absolute positioning
    - Implement `movedTo(int16_t x, int16_t y)` returning repositioned rectangle
    - _Requirements: 3.1, 3.2_

  - [x] 5.2 Add resizing methods
    - Implement `resize(uint16_t width, uint16_t height)` for in-place resizing
    - Implement `resized(uint16_t width, uint16_t height)` returning resized rectangle
    - Ensure width and height parameters are properly validated
    - _Requirements: 3.3_

  - [x] 5.3 Add combined adjustment methods
    - Implement `adjust(int16_t dx, int16_t dy, int16_t dw, int16_t dh)` for combined operations
    - Implement `adjusted()` const version returning adjusted rectangle
    - Handle coordinate and dimension overflow conditions
    - _Requirements: 3.4_

- [ ] 6. Implement centering operations
  - [ ] 6.1 Add rectangle centering methods
    - Implement `centerIn(const Rect& container)` for in-place centering
    - Implement `centeredIn(const Rect& container)` returning centered rectangle
    - Handle cases where rectangle is larger than container
    - _Requirements: 3.5_

- [ ] 7. Implement modern C++ features
  - [ ] 7.1 Add comparison operators
    - Implement `operator==(const Rect& other)` comparing all member variables
    - Implement `operator!=(const Rect& other)` as negation of equality
    - Ensure operators follow standard C++ conventions
    - _Requirements: 5.2_

  - [ ] 7.2 Add string representation and debugging support
    - Implement `toString()` method returning formatted string representation
    - Format as "Rect(x, y, width, height)" for clear debugging output
    - Include validation status in debug output when rectangle is invalid
    - _Requirements: 5.3, 7.2_

- [ ] 8. Implement coordinate system validation and normalization
  - [ ] 8.1 Add coordinate precision handling
    - Document coordinate system limitations in header comments
    - Implement overflow detection for coordinate calculations
    - Add safe arithmetic methods for internal use
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [ ] 8.2 Add rectangle normalization methods
    - Implement `normalized()` method handling negative width/height
    - Implement `normalize()` for in-place normalization
    - Ensure normalized rectangles have positive dimensions
    - _Requirements: 7.4_

- [ ] 9. Update header file organization and documentation
  - [ ] 9.1 Reorganize header file structure
    - Group existing methods at top to maintain compatibility
    - Group new methods by functionality (geometric, transformation, utility)
    - Add comprehensive documentation for all new methods
    - Maintain existing inline implementations for performance
    - _Requirements: 4.1, 4.2, 4.3_

  - [ ] 9.2 Add comprehensive method documentation
    - Document coordinate system conventions and limitations
    - Add usage examples for complex geometric operations
    - Document edge cases and error handling behavior
    - Include performance notes for frequently used methods
    - _Requirements: 6.5, 7.3_

- [ ] 10. Verify backward compatibility and integration
  - [ ] 10.1 Compile existing codebase with enhanced Rect class
    - Ensure all existing Widget code compiles without changes
    - Verify Surface blitting operations work correctly
    - Check player.cpp UI positioning code functions properly
    - Validate ToastWidget and WindowWidget positioning
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [ ] 10.2 Validate critical usage patterns
    - Test Widget positioning and event handling with enhanced Rect
    - Verify LayoutWidget operations work with new methods
    - Check ApplicationWidget coordinate transformations
    - Ensure no performance regression in hot paths
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [ ] 11. Update implementation file with new method bodies
  - [ ] 11.1 Move complex method implementations to rect.cpp
    - Move non-trivial geometric operations to implementation file
    - Keep simple getters and setters as inline methods
    - Implement safe arithmetic helper functions
    - Add overflow checking for coordinate calculations
    - _Requirements: 6.2, 6.3, 6.4_

  - [ ] 11.2 Optimize performance-critical methods
    - Profile geometric operations for performance impact
    - Optimize frequently used methods like contains() and intersects()
    - Ensure memory layout remains unchanged for compatibility
    - Validate no regression in Widget system performance
    - _Requirements: 4.5, 5.4_