# FLAC Frame Indexing Guide

## Overview

The FLAC Frame Indexing system provides efficient seeking in FLAC streams by building and maintaining an index of discovered frame positions. This addresses the fundamental limitations of binary search seeking on compressed audio streams.

## Architecture

### Problem Statement

Traditional seeking methods have limitations with compressed FLAC streams:

1. **Binary Search Limitations**: Cannot predict frame positions in variable-length compressed data
2. **SEEKTABLE Dependency**: Not all FLAC files have SEEKTABLE metadata blocks
3. **Linear Search Performance**: Slow for long-distance seeks

### Solution: Frame Indexing

The frame indexing system builds a cache of discovered frame positions during:
- Initial parsing (limited indexing to prevent delays)
- Playback (continuous indexing as frames are read)

## Key Components

### FLACFrameIndexEntry

Stores information about a discovered frame:
```cpp
struct FLACFrameIndexEntry {
    uint64_t sample_offset;    // Sample position in stream
    uint64_t file_offset;      // File position of frame start
    uint32_t block_size;       // Number of samples in frame
    uint32_t frame_size;       // Actual frame size in bytes
};
```

### FLACFrameIndex

Manages the collection of frame index entries:
- Maintains sorted list of entries by sample offset
- Enforces memory limits and granularity constraints
- Provides efficient lookup methods

### Integration with FLACDemuxer

Frame indexing is integrated into the demuxer's seeking strategy:

1. **Priority 1**: Frame index seeking (most accurate)
2. **Priority 2**: SEEKTABLE seeking (fast but less accurate)
3. **Priority 3**: Binary search (limited effectiveness)
4. **Priority 4**: Linear search (reliable fallback)

## Configuration

### Memory Limits

```cpp
static constexpr size_t MAX_INDEX_ENTRIES = 50000;        // Maximum entries
static constexpr size_t MEMORY_LIMIT_BYTES = 8 * 1024 * 1024; // 8MB limit
```

### Granularity Control

```cpp
static constexpr size_t INDEX_GRANULARITY_SAMPLES = 44100; // 1 second at 44.1kHz
```

Entries closer than the granularity threshold are rejected to prevent excessive memory usage.

## API Usage

### Enabling/Disabling Frame Indexing

```cpp
FLACDemuxer demuxer(std::move(handler));

// Check if enabled (default: true)
bool enabled = demuxer.isFrameIndexingEnabled();

// Disable frame indexing
demuxer.setFrameIndexingEnabled(false);

// Re-enable frame indexing
demuxer.setFrameIndexingEnabled(true);
```

### Manual Index Building

```cpp
// Trigger initial frame indexing
bool success = demuxer.buildFrameIndex();
```

### Getting Index Statistics

```cpp
auto stats = demuxer.getFrameIndexStats();
std::cout << "Index entries: " << stats.entry_count << std::endl;
std::cout << "Memory usage: " << stats.memory_usage << " bytes" << std::endl;
std::cout << "Coverage: " << stats.coverage_percentage << "%" << std::endl;
```

## Performance Characteristics

### Initial Indexing

- Limited to 1000 frames or 5 minutes of audio
- Performed during `parseContainer()` if enabled
- Prevents long delays during file opening

### Playback Indexing

- Frames added to index during normal playback
- Continuous improvement of seeking accuracy
- Respects granularity and memory limits

### Seeking Performance

With frame indexing:
- **Sample-accurate seeking** for indexed regions
- **Millisecond-level seek times** for cached positions
- **Graceful fallback** to other methods when index insufficient

Without frame indexing:
- Relies on SEEKTABLE or binary search
- May have accuracy limitations with compressed streams

## Memory Usage

### Typical Usage

For a 1-hour FLAC file at 44.1kHz:
- Approximately 3600 index entries (1 per second)
- Memory usage: ~115KB (32 bytes per entry)
- Coverage: Depends on playback patterns

### Memory Optimization

The system automatically:
- Enforces maximum entry limits
- Respects total memory limits
- Uses granularity to prevent excessive entries
- Provides memory usage reporting

## Integration Examples

### Basic Usage

```cpp
// Create demuxer with frame indexing enabled
auto handler = std::make_unique<FileIOHandler>("music.flac");
FLACDemuxer demuxer(std::move(handler));

// Parse container (includes initial indexing)
if (!demuxer.parseContainer()) {
    // Handle error
}

// Seeking will automatically use frame index when available
demuxer.seekTo(120000); // Seek to 2 minutes
```

### Advanced Configuration

```cpp
// Disable frame indexing for memory-constrained environments
demuxer.setFrameIndexingEnabled(false);

// Or monitor memory usage
auto stats = demuxer.getFrameIndexStats();
if (stats.memory_usage > threshold) {
    demuxer.setFrameIndexingEnabled(false);
}
```

## Thread Safety

The frame indexing system is fully thread-safe:
- Uses mutex protection for index access
- Atomic operations for performance-critical paths
- Follows public/private lock pattern to prevent deadlocks

## Future Enhancements

### Persistent Indexing

Future versions could implement:
- Index caching to disk
- Faster subsequent file opens
- Shared index databases

### Advanced Algorithms

Potential improvements:
- Adaptive granularity based on file characteristics
- Compression-aware indexing strategies
- Predictive frame position algorithms

## Troubleshooting

### Common Issues

1. **High Memory Usage**
   - Check `getFrameIndexStats().memory_usage`
   - Consider disabling for very long files
   - Adjust granularity if needed

2. **Slow Seeking**
   - Verify frame indexing is enabled
   - Check index coverage with `getFrameIndexStats()`
   - Ensure sufficient playback for index building

3. **Seeking Accuracy**
   - Frame indexing provides sample-accurate seeking
   - Other methods may have approximation errors
   - Linear search fallback ensures reliability

### Debug Information

Enable FLAC debug logging to see:
- Frame indexing operations
- Seeking strategy selection
- Index statistics and coverage

```cpp
Debug::setLevel("flac", Debug::VERBOSE);
```

## Conclusion

The FLAC Frame Indexing system provides a robust solution for efficient seeking in compressed FLAC streams. By building an index of discovered frame positions, it enables sample-accurate seeking while maintaining reasonable memory usage and providing graceful fallbacks for edge cases.