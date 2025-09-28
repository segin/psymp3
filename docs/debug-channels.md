# PsyMP3 Debug Channels Documentation

## Quick Start - How to Actually Use Debug Channels

### Most Common Usage

```bash
# Enable FLAC debugging (most useful for audio issues)
./psymp3 --debug=flac music.flac

# Enable memory debugging (for performance issues)
./psymp3 --debug=memory music.flac

# Enable MPRIS debugging (for media player interface issues)
./psymp3 --debug=mpris,dbus music.flac

# Enable multiple channels
./psymp3 --debug=flac,memory,io music.flac

# Enable ALL debug output (warning: very verbose!)
./psymp3 --debug=all music.flac

# Save debug output to file
./psymp3 --debug=flac --logfile=debug.log music.flac
```

### Runtime Controls

```bash
# Start with no debug, toggle during playback
./psymp3 music.flac
# Press 'r' during playback to toggle widget debugging
```

### Real-World Examples

```bash
# Debugging FLAC playback issues
./psymp3 --debug=flac,flac_codec problem.flac

# Debugging memory leaks or performance
./psymp3 --debug=memory,io --logfile=memory_debug.log *.flac

# Debugging Ogg/Vorbis files
./psymp3 --debug=ogg,vorbis problem.ogg

# Debugging MPRIS interface issues
./psymp3 --debug=mpris,dbus music.flac

# Full debugging session (save to file - console will be flooded!)
./psymp3 --debug=all --logfile=full_debug.log problem_file.flac
```

## Available Debug Channels - Complete List

### Core System Channels
- **`memory`** - Memory management, allocation tracking, pool operations
- **`io`** - I/O operations, positioning, state management  
- **`error`** - Error handling, propagation, and recovery
- **`resource`** - System resource management and exhaustion handling
- **`system`** - System-level operations and initialization

### Container Format Channels
- **`flac`** - FLAC container demuxing and frame processing
- **`ogg`** - Ogg container demuxing and stream management
- **`iso`** - ISO container format operations (MP4, etc.)
- **`demux`** - General demuxer operations
- **`demuxer`** - Demuxer factory and management

### Audio Codec Channels
- **`audio`** - General audio processing operations
- **`flac_codec`** - FLAC codec operations and audio decoding
- **`mp3`** - MP3 codec operations and decoding
- **`vorbis`** - Vorbis codec operations
- **`opus`** - Opus codec operations
- **`codec`** - General codec operations and management
- **`chunk`** - Audio chunk processing and management
- **`stream`** - Audio stream operations
- **`streaming`** - Streaming operations and buffering

### Network and I/O Channels
- **`http`** - HTTP client operations and network I/O
- **`HTTPIOHandler`** - HTTP-specific I/O handler operations

### Application Channels
- **`player`** - Main player operations and state management
- **`loader`** - File loading and track management
- **`playlist`** - Playlist operations and management
- **`plugin`** - Plugin system operations
- **`lastfm`** - Last.fm scrobbling and integration
- **`lyrics`** - Lyrics processing and display
- **`spectrum`** - Spectrum analyzer and visualization
- **`font`** - Font rendering and text operations
- **`widget`** - UI widget debugging (runtime toggle with 'r' key)
- **`mpris`** - MPRIS media player interface operations
- **`dbus`** - D-Bus connection and message handling

### Infrastructure Channels  
- **`raii`** - RAII resource management and lifecycle tracking
- **`timer`** - Timer operations and scheduling

### Testing and Performance Channels
- **`test`** - Test execution and validation
- **`flac_benchmark`** - FLAC performance benchmarking and testing
- **`performance`** - Performance monitoring and benchmarking
- **`benchmark`** - Benchmarking operations

### Special Channels
- **`all`** - Enables ALL debug channels (very verbose!)

## Overview

PsyMP3 uses a centralized debug logging system that allows selective enabling of debug output through named channels. This document provides a comprehensive reference of all available debug channels and their usage patterns.

## Debug System Architecture

The debug system is implemented in the `Debug` class (`include/debug.h`, `src/debug.cpp`) and provides:

- **Channel-based filtering**: Enable/disable specific debug categories
- **Thread-safe logging**: All debug output is thread-safe
- **Flexible output**: Console or file-based logging
- **Template-based API**: Efficient variadic argument handling
- **Timestamp precision**: Microsecond-precision timestamps

### Basic Usage

```cpp
// Initialize debug system with channels
std::vector<std::string> channels = {"flac", "ogg", "memory"};
Debug::init("debug.log", channels);

// Log to specific channel
Debug::log("flac", "Frame parsed successfully, size: ", frame_size);

// Enable all channels
std::vector<std::string> all_channels = {"all"};
Debug::init("", all_channels); // Empty string = console output
```

### Special Channels

- **`all`**: Enables all debug channels globally
- **Empty channel list**: Disables all debug output

## Core System Channels

### `memory`
**Purpose**: Memory management, allocation tracking, and optimization

**Components**:
- `MemoryPoolManager` - Pool allocation and management
- `MemoryTracker` - Memory usage tracking and pressure monitoring
- `IOHandler` - Memory limit enforcement and optimization
- `MemoryOptimizer` - Memory pressure and optimization decisions

**Example Output**:
```
14:32:15.123456 [memory]: MemoryPoolManager::allocateBuffer_unlocked() - Allocated 4096 bytes from pool for flac_decoder
14:32:15.124001 [memory]: IOHandler::checkMemoryLimits_unlocked() - Memory usage: 85.2% (87654321 / 102857600 bytes)
```

**Key Events**:
- Buffer pool allocation/deallocation
- Memory limit warnings and enforcement
- Memory pressure level changes
- Optimization operations and results

### `io`
**Purpose**: I/O operations, positioning, and state management

**Components**:
- `IOHandler` - Base I/O operations and state tracking
- `FileIOHandler` - File-specific I/O operations
- `HTTPIOHandler` - Network I/O operations

**Example Output**:
```
14:32:15.125678 [io]: IOHandler::updatePosition() - Position updated to: 1048576
14:32:15.126234 [io]: IOHandler::updateEofState() - EOF state updated to: false
```

**Key Events**:
- Position updates and seeking operations
- EOF and closed state changes
- I/O operation results and errors

### `error`
**Purpose**: Error handling, propagation, and recovery

**Components**:
- `IOHandler` - Error state management and safe propagation
- All components - Error reporting and recovery operations

**Example Output**:
```
14:32:15.127890 [error]: IOHandler::safeErrorPropagation() - Propagating error 12: Cannot allocate memory
14:32:15.128456 [error]: IOHandler::safeErrorPropagation() - Fatal error, marking as closed
```

**Key Events**:
- Error detection and classification
- Error propagation and cleanup operations
- Recovery attempts and results

### `resource`
**Purpose**: System resource management and exhaustion handling

**Components**:
- `IOHandler` - Resource limit detection and recovery

**Example Output**:
```
14:32:15.129012 [resource]: IOHandler::handleResourceExhaustion() - Resource exhausted: file_descriptors in context: file_open
14:32:15.129567 [resource]: IOHandler::handleResourceExhaustion() - Attempting to free file descriptors
```

**Key Events**:
- Resource exhaustion detection
- Resource cleanup and recovery attempts
- Resource limit warnings

### `system`
**Purpose**: System-level operations and initialization

**Components**:
- System initialization and startup operations
- Platform-specific system calls and operations

**Key Events**:
- System initialization and shutdown
- Platform-specific operations
- System-level error handling

## Container Format Channels

### `flac`
**Purpose**: FLAC container demuxing and frame processing

**Components**:
- `FLACDemuxer` - Container parsing and frame extraction
- FLAC frame indexing and seeking operations

**Example Output**:
```
14:32:15.130123 [flac]: [calculateFrameSize] Fixed block size, using min: 4096 bytes
14:32:15.130678 [flac]: [addFrameToIndex] Added frame to index: sample 44100 at offset 2048 (1024 samples)
```

**Key Events**:
- Stream marker validation
- Metadata block parsing
- Frame boundary detection and indexing
- Seeking operations and frame recovery

**Message Format**: Uses `[methodName]` prefixes for method identification

### `ogg`
**Purpose**: Ogg container demuxing and stream management

**Components**:
- `OggDemuxer` - Ogg page parsing and packet extraction
- Codec-specific header parsing within Ogg streams

**Example Output**:
```
14:32:15.132345 [ogg]: YourCodec: Parsing identification header, size=64
14:32:15.132901 [ogg]: YourCodec: Sample rate=44100, channels=2
```

**Key Events**:
- Ogg page validation and parsing
- Packet boundary detection
- Codec header parsing
- Stream multiplexing operations

### `iso`
**Purpose**: ISO container format operations (MP4, MOV, etc.)

**Components**:
- ISO container demuxers
- MP4/MOV format parsing

**Key Events**:
- ISO container parsing
- Atom/box structure analysis
- Track and metadata extraction

### `demux`
**Purpose**: General demuxer operations

**Components**:
- Generic demuxer functionality
- Cross-format demuxing operations

**Key Events**:
- Demuxer selection and initialization
- Generic container operations
- Format detection and validation

### `demuxer`
**Purpose**: Demuxer factory and management

**Components**:
- `MediaFactory` - Demuxer creation and selection
- Demuxer lifecycle management

**Key Events**:
- Demuxer factory operations
- Format detection and demuxer selection
- Demuxer registration and management

## Audio Codec Channels

### `audio`
**Purpose**: General audio processing operations

**Components**:
- Generic audio processing
- Cross-codec audio operations

**Key Events**:
- Audio format conversion
- General audio processing operations
- Audio pipeline management

### `flac_codec`
**Purpose**: FLAC codec operations and audio decoding

**Components**:
- `FLACCodec` - Audio frame decoding and error recovery

**Example Output**:
```
14:32:15.131234 [flac_codec]: Frame decode failed, attempting recovery
14:32:15.131789 [flac_codec]: Decoder recovery successful
```

**Key Events**:
- Codec initialization and configuration
- Frame decoding operations
- Error detection and recovery
- Performance threshold monitoring

### `mp3`
**Purpose**: MP3 codec operations and decoding

**Components**:
- `MP3Codec` - MP3 audio decoding
- MP3 frame processing and error recovery

**Key Events**:
- MP3 decoder initialization
- Frame decoding and synchronization
- Bitrate and quality management
- Error recovery and concealment

### `vorbis`
**Purpose**: Vorbis codec operations

**Components**:
- `VorbisCodec` - Vorbis audio decoding
- Vorbis header processing and setup

**Key Events**:
- Vorbis decoder initialization
- Header parsing (identification, comment, setup)
- Audio packet decoding
- Quality and bitrate management

### `opus`
**Purpose**: Opus codec operations

**Components**:
- `OpusCodec` - Opus audio decoding
- Opus packet processing

**Key Events**:
- Opus decoder initialization
- Packet decoding and error correction
- Mode switching (SILK/CELT/Hybrid)
- Bandwidth and complexity management

### `codec`
**Purpose**: General codec operations and management

**Components**:
- Generic codec functionality
- Codec factory and selection

**Key Events**:
- Codec registration and selection
- Generic codec operations
- Cross-codec functionality

### `chunk`
**Purpose**: Audio chunk processing and management

**Components**:
- Audio chunk allocation and processing
- Buffer management for audio data

**Key Events**:
- Audio chunk creation and destruction
- Chunk size optimization
- Buffer pool operations

### `stream`
**Purpose**: Audio stream operations

**Components**:
- Audio stream management
- Stream state and metadata

**Key Events**:
- Stream initialization and configuration
- Stream state changes
- Metadata processing

### `streaming`
**Purpose**: Streaming operations and buffering

**Components**:
- Network streaming operations
- Buffer management for streaming

**Key Events**:
- Stream buffering operations
- Network streaming state
- Buffer underrun/overrun handling

## Network and I/O Channels

### `http`
**Purpose**: HTTP client operations and network I/O

**Components**:
- `HTTPClient` - HTTP request/response handling
- HTTP streaming operations

**Key Events**:
- HTTP request initiation and completion
- Response processing and error handling
- Connection management and timeouts
- Range request operations

### `HTTPIOHandler`
**Purpose**: HTTP-specific I/O handler operations

**Components**:
- `HTTPIOHandler` - HTTP-based streaming I/O

**Key Events**:
- HTTP stream initialization
- Chunk-based reading operations
- Connection state management
- HTTP-specific error handling

## Application Channels

### `player`
**Purpose**: Main player operations and state management

**Components**:
- `Player` - Main application controller
- Playback state management

**Example Output**:
```
14:32:15.140123 [player]: PsyMP3 version 2.1.0.
```

**Key Events**:
- Player initialization and shutdown
- Playback state changes (play, pause, stop)
- Track loading and switching
- User interface events

### `loader`
**Purpose**: File loading and track management

**Components**:
- Track loading operations
- File format detection and validation

**Key Events**:
- File loading initiation and completion
- Format detection and validation
- Metadata extraction
- Loading error handling

### `playlist`
**Purpose**: Playlist operations and management

**Components**:
- Playlist creation and modification
- Track ordering and selection

**Key Events**:
- Playlist loading and saving
- Track addition and removal
- Shuffle and repeat operations
- Playlist navigation

### `plugin`
**Purpose**: Plugin system operations

**Components**:
- Plugin loading and management
- Plugin API operations

**Key Events**:
- Plugin discovery and loading
- Plugin initialization and cleanup
- Plugin API calls and responses
- Plugin error handling

### `lastfm`
**Purpose**: Last.fm scrobbling and integration

**Components**:
- Last.fm API client
- Scrobbling operations

**Key Events**:
- Last.fm authentication
- Track scrobbling operations
- API request/response handling
- Offline scrobble caching

### `lyrics`
**Purpose**: Lyrics processing and display

**Components**:
- Lyrics fetching and parsing
- Lyrics display management

**Key Events**:
- Lyrics source detection
- Lyrics parsing and formatting
- Display synchronization
- Lyrics caching operations

### `spectrum`
**Purpose**: Spectrum analyzer and visualization

**Components**:
- FFT processing for spectrum analysis
- Visualization rendering

**Key Events**:
- FFT computation and analysis
- Spectrum data processing
- Visualization updates
- Performance optimization

### `font`
**Purpose**: Font rendering and text operations

**Components**:
- Font loading and management
- Text rendering operations

**Key Events**:
- Font loading and caching
- Text measurement and rendering
- Font fallback operations
- Glyph caching

### `widget`
**Purpose**: UI widget debugging (runtime toggle with 'r' key)

**Components**:
- UI widget system
- Widget rendering and event handling

**Key Events**:
- Widget creation and destruction
- Rendering operations and blitting
- Event handling and propagation
- Layout calculations

### `mpris`
**Purpose**: MPRIS media player interface operations

**Components**:
- `MPRISManager` - Main MPRIS interface management
- `PropertyManager` - MPRIS property handling and updates
- `MethodHandler` - MPRIS method call processing
- `SignalEmitter` - MPRIS signal emission and event handling

**Example Output**:
```
14:32:15.140123 [mpris]: [MPRISManager] MPRIS interface initialized successfully
14:32:15.140678 [mpris]: [PropertyManager] Property updated: PlaybackStatus = Playing
14:32:15.141234 [mpris]: [MethodHandler] Method call received: Play()
14:32:15.141789 [mpris]: [SignalEmitter] Emitting PropertiesChanged signal
```

**Key Events**:
- MPRIS interface initialization and shutdown
- Property updates (PlaybackStatus, Metadata, Position, etc.)
- Method call processing (Play, Pause, Stop, Seek, etc.)
- Signal emission for property changes
- Error handling and recovery operations

### `dbus`
**Purpose**: D-Bus connection and message handling

**Components**:
- `DBusConnectionManager` - D-Bus connection lifecycle management
- D-Bus message tracing and debugging

**Example Output**:
```
14:32:15.142345 [dbus]: [DBusConnectionManager] D-Bus connection established successfully
14:32:15.142901 [dbus]: [DBusConnectionManager] Connection state changed: Connected
14:32:15.143456 [dbus]: OUTGOING D-Bus message (method call)
14:32:15.144012 [dbus]: INCOMING D-Bus message (signal)
```

**Key Events**:
- D-Bus connection establishment and teardown
- Connection state changes and monitoring
- D-Bus message tracing (incoming/outgoing)
- Connection error handling and recovery
- Service registration and name acquisition

## Infrastructure Channels

### `raii`
**Purpose**: RAII resource management and lifecycle tracking

**Components**:
- `RAIIFileHandle` - File handle lifecycle management

**Example Output**:
```
14:32:15.133456 [raii]: RAIIFileHandle::RAIIFileHandle() - Constructor with file handle: valid, ownership: yes
14:32:15.134012 [raii]: RAIIFileHandle::close() - Closing owned file handle
```

**Key Events**:
- Resource acquisition and release
- Ownership transfer operations
- Exception safety during cleanup
- Resource lifecycle validation

### `timer`
**Purpose**: Timer operations and scheduling

**Components**:
- Timer management and scheduling
- Periodic operations and callbacks

**Example Output**:
```
14:32:15.135123 [timer]: skipped
```

**Key Events**:
- Timer creation and destruction
- Timer callback execution
- Scheduling operations
- Timer performance monitoring

## Testing and Performance Channels

### `test`
**Purpose**: Test execution and validation

**Components**:
- All test files - Test progress and validation

**Example Output**:
```
14:32:15.136123 [test]: test_enhanced_buffer_pool() - Starting test
14:32:15.136678 [test]: test_enhanced_buffer_pool() - Test completed successfully
```

**Key Events**:
- Test initialization and completion
- Test validation steps
- Performance measurement results
- Error condition testing

### `flac_benchmark`
**Purpose**: FLAC performance benchmarking and testing

**Components**:
- FLAC performance testing
- Benchmarking operations specific to FLAC

**Key Events**:
- FLAC performance measurements
- Benchmark test execution
- Performance regression testing
- Optimization validation

### `performance`
**Purpose**: Performance monitoring and benchmarking

**Components**:
- General performance monitoring
- Cross-component performance analysis

**Example Output**:
```
14:32:15.137456 [performance]: Container parsing took 125 ms
```

**Key Events**:
- Performance measurement collection
- Timing analysis and reporting
- Performance threshold monitoring
- Optimization impact assessment

### `benchmark`
**Purpose**: Benchmarking operations

**Components**:
- Benchmarking framework
- Performance comparison operations

**Key Events**:
- Benchmark execution and timing
- Performance comparison analysis
- Regression detection
- Optimization validation
```
14:32:15.134567 [file]: FileIOHandler operation details
```

**Key Events**:
- File opening and closing
- File seeking and reading operations
- File-specific error handling

### `http`
**Purpose**: HTTP client operations and network I/O

**Components**:
- `HTTPClient` - HTTP request/response handling
- `HTTPIOHandler` - HTTP-based streaming

**Usage**: Reserved for HTTP streaming operations

### `test`
**Purpose**: Test execution and validation

**Components**:
- All test files - Test progress and validation

**Example Output**:
```
14:32:15.135123 [test]: test_enhanced_buffer_pool() - Starting test
14:32:15.135678 [test]: test_enhanced_buffer_pool() - Test completed successfully
```

**Key Events**:
- Test initialization and completion
- Test validation steps
- Performance measurement results
- Error condition testing

## Runtime Debug Control

### Actual Command Line Options (from main.cpp)

```bash
# The REAL command line syntax:
./psymp3 --debug=CHANNELS [--logfile=FILE] music_files...

# Examples:
./psymp3 --debug=flac music.flac                    # Single channel
./psymp3 --debug=flac,memory music.flac             # Multiple channels  
./psymp3 --debug=all music.flac                     # All channels
./psymp3 --debug=flac --logfile=debug.log music.flac # With log file

# Runtime toggle (press 'r' key during playback)
# Currently only toggles widget debugging - more channels planned
```

### Programmatic Control

```cpp
// Initialize with specific channels
std::vector<std::string> channels = {"flac", "ogg", "memory"};
Debug::init("psymp3_debug.log", channels);

// Enable all channels
std::vector<std::string> all_channels = {"all"};
Debug::init("", all_channels);

// Disable all debug output
std::vector<std::string> no_channels = {};
Debug::init("", no_channels);
```

## Performance Considerations

### Efficient Logging

The debug system uses template-based variadic arguments for efficient message construction:

```cpp
// Efficient - arguments only evaluated if channel is enabled
Debug::log("flac", "Frame size: ", calculateFrameSize(), " bytes");

// Less efficient - always constructs string
std::string msg = "Frame size: " + std::to_string(calculateFrameSize()) + " bytes";
Debug::log("flac", msg);
```

### Channel Filtering

- Channel filtering occurs before message construction
- Disabled channels have minimal performance impact
- Use specific channels rather than `all` in production

### Memory Usage

- File logging buffers output for performance
- Console logging is unbuffered for immediate visibility
- Log rotation is not implemented - manage log file size externally

## Best Practices

### Channel Selection

1. **Development**: Enable relevant channels for the component being worked on
2. **Debugging**: Use `all` channel for comprehensive logging
3. **Performance Testing**: Disable all channels or use specific performance-related channels
4. **Production**: Disable debug logging entirely

### Message Format Guidelines

1. **Include method name**: Use `[methodName]` prefix for method identification
2. **Include context**: Add relevant parameters and state information
3. **Use consistent terminology**: Follow established naming conventions
4. **Avoid sensitive data**: Don't log user data or credentials

### Error Handling

1. **Log before throwing**: Always log errors before throwing exceptions
2. **Include error codes**: Log system error codes and messages
3. **Log recovery attempts**: Document error recovery operations
4. **Use appropriate channels**: Use `error` channel for error conditions

## Integration with Development Tools

### GDB Integration

Debug channels can be controlled from GDB:

```gdb
# Enable specific channels during debugging
call Debug::init("", std::vector<std::string>{"flac", "memory"})

# Enable all channels
call Debug::init("", std::vector<std::string>{"all"})
```

### Test Integration

Tests automatically enable the `test` channel:

```cpp
int main() {
    std::vector<std::string> channels = {"test", "memory"};
    Debug::init("", channels);
    
    // Run tests with debug output
    return run_all_tests();
}
```

### Build System Integration

Configure debug channels at build time:

```bash
# Enable debug build with specific channels
./configure --enable-debug --with-debug-channels="flac,ogg,memory"

# Enable all debug channels
./configure --enable-debug --with-debug-channels="all"
```

## Troubleshooting Real Problems

### "My FLAC file won't play"

```bash
# Step 1: Enable FLAC debugging
./psymp3 --debug=flac problem.flac

# Look for these messages:
# ✓ Good: "[calculateFrameSize] Fixed block size, using min: 4096 bytes"
# ✗ Bad: "Invalid FLAC stream marker"
# ✗ Bad: "Container parsing exception"

# Step 2: If codec issues, add flac_codec channel
./psymp3 --debug=flac,flac_codec problem.flac

# Look for:
# ✓ Good: "Decoder recovery successful"  
# ✗ Bad: "Frame decode failed, attempting recovery"
```

### "Memory usage is too high"

```bash
# Enable memory debugging
./psymp3 --debug=memory --logfile=memory.log music_collection/*.flac

# Look for these warning patterns:
# ✗ "Memory allocation of X bytes not safe according to MemoryPoolManager"
# ✗ "Per-handler limit exceeded: X > Y"
# ✗ "Total memory limit exceeded: X > Y"
# ✓ "Optimization complete: 85.2% -> 45.1% (saved 12345678 bytes)"
```

### "Ogg files have playback issues"

```bash
# Enable Ogg container debugging
./psymp3 --debug=ogg problem.ogg

# Look for:
# ✓ Good: "YourCodec: Sample rate=44100, channels=2"
# ✗ Bad: "Exception parsing YourCodec headers"
# ✗ Bad: "Malformed YourCodec header data"
```

### "Application crashes or hangs"

```bash
# Full debugging (save to file - console will be overwhelmed!)
./psymp3 --debug=all --logfile=crash_debug.log problem_file

# Key channels for crashes:
./psymp3 --debug=error,memory,raii --logfile=crash.log problem_file

# Look for:
# ✗ "Fatal error, marking as closed"
# ✗ "Exception during file close"
# ✗ "Failed to allocate X bytes"
```

### "MPRIS interface not working"

```bash
# Enable MPRIS debugging
./psymp3 --debug=mpris,dbus music.flac

# Look for these messages:
# ✓ Good: "[MPRISManager] MPRIS interface initialized successfully"
# ✓ Good: "[DBusConnectionManager] D-Bus connection established successfully"
# ✗ Bad: "[DBusConnectionManager] ERROR: Failed to connect to D-Bus"
# ✗ Bad: "[MPRISManager] ERROR: Failed to register MPRIS service"

# Step 2: If D-Bus connection issues, check system D-Bus
./psymp3 --debug=dbus --logfile=dbus_debug.log music.flac

# Look for:
# ✓ Good: "Connection state changed: Connected"
# ✗ Bad: "Connection state changed: Disconnected"
# ✗ Bad: "D-Bus service registration failed"
```

### Common Issues

1. **No debug output**: Check you're using `--debug=channel` not `--debug-channel`
2. **Too much output**: Use specific channels instead of `--debug=all`
3. **Log file not created**: Check directory permissions and disk space
4. **Performance impact**: Debug logging does impact performance - disable for production use

### Debug Channel Verification

```cpp
// Verify channel is enabled
if (Debug::isChannelEnabled("flac")) {
    std::cout << "FLAC debug channel is enabled" << std::endl;
}
```

### Practical Debug Workflow

#### 1. Start Simple
```bash
# Always start with the most relevant channel
./psymp3 --debug=flac problem.flac        # For FLAC issues
./psymp3 --debug=ogg problem.ogg          # For Ogg issues  
./psymp3 --debug=memory slow_performance   # For performance issues
./psymp3 --debug=mpris music.flac          # For MPRIS interface issues
```

#### 2. Add Related Channels
```bash
# If FLAC container parsing works but audio doesn't
./psymp3 --debug=flac,flac_codec problem.flac

# If memory issues, add I/O debugging
./psymp3 --debug=memory,io --logfile=debug.log problem.flac

# If MPRIS interface issues, add D-Bus debugging
./psymp3 --debug=mpris,dbus --logfile=mpris_debug.log music.flac
```

#### 3. Full Debug (Last Resort)
```bash
# Only when you need everything (VERY verbose!)
./psymp3 --debug=all --logfile=full_debug.log problem.flac
# Then grep the log file for specific patterns
grep -i error full_debug.log
grep -i exception full_debug.log
grep -i failed full_debug.log
```

### Log Analysis

Debug output includes microsecond timestamps for performance analysis:

```
14:32:15.123456 [flac]: Frame parsing started
14:32:15.125789 [flac]: Frame parsing completed (2.333ms)
```

**Pro tip**: Use `grep` to filter large debug logs:
```bash
# Find all errors
grep "\[error\]" debug.log

# Find memory warnings  
grep "limit exceeded" debug.log

# Find FLAC-specific issues
grep "\[flac\].*failed\|error\|exception" debug.log

# Find MPRIS-specific issues
grep "\[mpris\].*ERROR\|FATAL\|failed" debug.log

# Find D-Bus connection issues
grep "\[dbus\].*ERROR\|failed\|Disconnected" debug.log
```

## Future Enhancements

### Planned Features

1. **Log rotation**: Automatic log file rotation based on size/time
2. **Remote logging**: Network-based debug output for embedded systems
3. **Filtering by severity**: Debug levels (VERBOSE, INFO, WARNING, ERROR)
4. **Performance metrics**: Built-in timing and performance measurement
5. **Structured logging**: JSON or structured output format options

### Channel Expansion

As new components are added, corresponding debug channels will be created following the established naming conventions and patterns documented here.