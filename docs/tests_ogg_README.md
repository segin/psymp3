# OGG Demuxer Tests

This directory contains tests for the OggDemuxer implementation following the ogg-demuxer-fix spec.

## Test Categories

### Property-Based Tests (RapidCheck)
Property-based tests verify universal properties that should hold across all inputs:

1. **Property 1**: OggS Capture Pattern Validation
2. **Property 2**: Page Version Validation
3. **Property 3**: Page Size Bounds
4. **Property 4**: Lacing Value Interpretation
5. **Property 5**: Codec Signature Detection
6. **Property 6**: FLAC-in-Ogg Header Structure
7. **Property 7**: Page Sequence Tracking
8. **Property 8**: Grouped Stream Ordering
9. **Property 9**: Chained Stream Detection
10. **Property 10**: Granule Position Arithmetic Safety
11. **Property 11**: Invalid Granule Handling
12. **Property 12**: Multi-Page Packet Reconstruction
13. **Property 13**: Seeking Accuracy
14. **Property 14**: Duration Calculation Consistency
15. **Property 15**: Bounded Queue Memory
16. **Property 16**: Thread Safety
17. **Property 17**: Position Reporting Consistency

### Unit Tests
Unit tests verify specific examples, edge cases, and error conditions.

## Running Tests

```bash
# Run all OGG demuxer tests
make check

# Run specific property test
./tests/test_ogg_property_<N>

# Run with verbose output
./tests/test_ogg_property_<N> --verbose
```

## Test Data

Test data files should be placed in `tests/data/ogg/` directory.

## References

- RFC 3533: The Ogg Encapsulation Format Version 0
- RFC 7845: Ogg Encapsulation for the Opus Audio Codec
- RFC 9639 Section 10.1: FLAC-in-Ogg Encapsulation
- libvorbisfile reference implementation
- libopusfile reference implementation
