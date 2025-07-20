---
inclusion: manual
---

# Known Incomplete Areas

This document tracks areas of the project that are known to be incomplete or need attention.

## Testing Infrastructure
- **Status**: Incomplete
- **Issue**: No proper test harness exists
- **Priority**: High - should be addressed ASAP
- **Current State**: Individual test files exist in `tests/` but lack integration
- **Impact**: Makes regression testing and validation difficult

## Areas Requiring Documentation
- Build process beyond basic autotools usage
- Widget system usage patterns and best practices
- Codec integration guidelines
- I/O handler implementation patterns

## Technical Debt
- Test framework implementation
- Potential header dependency cleanup
- Documentation of existing architecture patterns

## Notes
This document should be updated as incomplete areas are identified or resolved.