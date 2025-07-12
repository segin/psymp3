# CLAUDE.md - Project Memory and Development Context

This file contains comprehensive project context for AI assistant continuity. **Always update this file when making significant changes!**

## Project Overview

**PsyMP3** is a cross-platform audio player written in C++ with FFT visualization. Originally a FreeBASIC project (1.x), version 2.x is a complete rewrite focused on portability and modern C++ practices.

- **Repository**: https://github.com/segin/psymp3
- **Version**: 2-CURRENT (development branch)
- **License**: ISC License
- **Maintainer**: Kirn Gill <segin2005@gmail.com>

## Recent Major Work (Current Session Context)

### Windows Compilation Compatibility (Build 657-669)
**Critical Issue**: PsyMP3 had numerous Windows compilation errors preventing builds on Windows/MSYS2.

**Key Fixes Implemented**:

1. **Socket Compatibility Layer** (Build 657-661)
   - Replaced problematic `#define close(s) closesocket(s)` with `closeSocket()` helper function
   - Added cross-platform socket error handling: `getSocketError()`, `isSocketInProgress()`
   - Fixed Unicode/ANSI API mismatches: `GetModuleHandle` → `GetModuleHandleW`, etc.
   - Added Windows socket library linking: `-lws2_32 -lmswsock`

2. **Build System Fixes** (Build 662-666)
   - Fixed automatic Windows resource file compilation (`psymp3.rc` → `psymp3.res`)
   - Corrected build order: `SUBDIRS = res src` (res must build before src)
   - Added proper windres detection and conditional compilation

3. **Critical Bug Fixes** (Build 668-669)
   - **Stream class initialization**: Fixed uninitialized member variables causing garbage audio parameters
   - **Vorbis decoder safety**: Added null pointer checks and proper structure initialization
   - Fixed audio showing "3670064Hz, channels: 3145766" nonsense values

### Last.fm Integration Improvements
- Comprehensive debug output system with runtime toggle capability
- Offline scrobble caching with automatic retry
- Multi-endpoint redundancy for reliability
- Created detailed setup documentation (`LASTFM_SETUP.md`)

### VLC-Style Bitstream Buffering Implementation (Build 670+)
**Critical Issue**: Opus tracks stopping at exactly 33.4 seconds instead of playing full duration due to excessive decoded audio buffering causing premature EOF.

**Root Cause**: PsyMP3 was buffering 2.5+ minutes of decoded audio (opposite of VLC's approach), causing memory issues and EOF detection problems.

**Key Changes**:
1. **DemuxedStream Architecture**: Replaced decoded audio frame buffering with compressed bitstream chunk buffering
2. **On-Demand Decoding**: Audio frames are now decoded just-in-time from buffered compressed chunks
3. **OpusTags Debug Fix**: Fixed `METADATA_BLOCK_PICTURE` logging to show byte count instead of dumping large binary data
4. **Header Packet Handling**: Fixed immediate EOF from header packets by continuing loop when empty frames are received but more chunks are available
5. **Memory Efficiency**: Reduced memory usage by ~90% by buffering 10 compressed chunks instead of minutes of decoded audio

**Current Status**: VLC-style bitstream buffering is implemented and audio playback starts successfully, but the 33-second cutoff issue persists. Root cause identified: 

**CRITICAL BUG FOUND**: 
- **Actual track duration**: 189.18 seconds (3:09) - confirmed by ffprobe
- **PsyMP3 reported duration**: 300 seconds (5:00) - incorrect fallback default  
- **Stops at**: 33 seconds = 17.5% of actual duration
- **Issue**: Correct duration calculation (189s) is being reset to 0, triggering 5-minute fallback
- **Vorbis worse**: Stops after 1-2 seconds and shows no metadata, suggesting demuxer/metadata system issues

## Architecture Overview

### Core Components
- **Stream**: Base class for all audio decoders (MP3, Vorbis, Opus, FLAC, WAV)
- **Audio**: Audio system management and threading
- **Player**: Main playback controller and event loop
- **Widget System**: Hierarchical UI with transparency and mouse event handling
- **ApplicationWidget**: Singleton desktop container managing all windows
- **FFT/Visualization**: Real-time spectrum analysis and rendering

### Audio Pipeline
```
File → IOHandler → Stream (codec-specific) → Audio → SDL → Output
                     ↓
                 FFT Analysis → Spectrum Widget
```

### Supported Formats
- **MP3**: libmpg123 wrapper
- **Vorbis**: libvorbisfile wrapper  
- **Opus**: libopusfile wrapper
- **FLAC**: libFLAC++ wrapper
- **WAV**: Built-in PCM decoder via ChunkDemuxer

## Build System

### Dependencies
```bash
# Core dependencies
SDL 1.2+, libmpg123 1.8+, taglib 1.6+, libvorbisfile 1.3+
libopusfile 0.9+, libFLAC++ 1.2+, FreeType 2.4+, OpenSSL 1.0+

# Optional
D-Bus 1.0+ (for MPRIS media interface)

# Windows-specific  
ws2_32, mswsock (automatically linked on Windows builds)
```

### Build Process
```bash
# Standard autotools workflow
autoreconf -fiv
./configure
make -j8

# Windows: Automatic resource compilation via windres
# Build order: res/ directory first, then src/
```

## File Structure & Key Locations

### Configuration & Data
- **Linux**: `~/.config/psymp3/`
- **Windows**: `%APPDATA%\PsyMP3\`
- **Last.fm config**: `lastfm.conf` in config directory
- **Scrobble cache**: `scrobble_cache.xml` in config directory

### Important Files
- `src/stream.cpp`: Base audio decoder class (**Recently fixed: initialization**)
- `src/vorbis.cpp`: Vorbis decoder (**Recently fixed: null safety**)
- `src/HTTPClient.cpp`: Network layer (**Recently fixed: Windows sockets**)
- `src/system.cpp`: OS abstraction (**Recently fixed: Unicode APIs**)
- `include/psymp3.h`: Main header with platform compatibility (**Recently overhauled**)
- `res/psymp3.rc`: Windows resources (**Auto-compiled now**)

## Known Issues & Technical Debt

### Current Status (Post-Fixes)
- ✅ **Windows compilation**: Now works fully on MSYS2/MinGW
- ✅ **Audio initialization**: Fixed garbage values in Stream classes
- ✅ **Resource compilation**: Automatic windres integration
- ✅ **Socket layer**: Cross-platform compatibility implemented

### Remaining Areas for Improvement
- **Threading safety**: Some components lack explicit thread safety
- **Error handling**: Inconsistent error reporting across decoders  
- **Memory management**: Some manual resource management could use RAII
- **Unicode support**: File paths with non-ASCII characters need more testing
- **UI system**: Needs more comprehensive widget implementation

## Debug & Development

### Debug Modes
```bash
# Widget blitting debug
./psymp3 --debug-widgets files...

# Runtime debug (includes Last.fm)
./psymp3 --debug-runtime files...

# In-app: Press 'r' to toggle runtime debug during playback
```

### Build Versions
- **Build number**: Auto-increments on Windows (in `res/psymp3.rc`)
- **Current**: Build 669 (as of latest fixes)
- **Git tags**: Use for release milestones

## Network & External Services

### Last.fm Integration
- **API endpoints**: post.audioscrobbler.com, post2.audioscrobbler.com, submissions.last.fm
- **Authentication**: Username/password → session key (cached)
- **Scrobble rules**: >30s tracks, >50% listened OR >4min
- **Offline support**: XML cache with automatic retry

### HTTP Client Features
- **Connection pooling**: Keep-alive support for efficiency
- **Cross-platform sockets**: Unified Windows/Unix socket layer
- **SSL/TLS**: OpenSSL integration for HTTPS
- **Timeouts**: Configurable request timeouts
- **Error handling**: Comprehensive retry logic

## Platform-Specific Notes

### Windows (MSYS2/MinGW)
- **Socket libraries**: ws2_32, mswsock (auto-linked)
- **Resource compilation**: windres (auto-detected)
- **Unicode APIs**: All Windows API calls use wide character versions
- **File paths**: Unicode-aware file handling implemented

### Linux/Unix
- **Package managers**: Dependencies available via standard repos
- **MPRIS**: D-Bus integration for media key support
- **Config location**: XDG-compliant (`~/.config/psymp3/`)

## Testing & Quality Assurance

### Manual Testing Checklist
- [ ] Audio playback across all supported formats
- [ ] Last.fm scrobbling (online/offline scenarios)
- [ ] Windows Unicode file paths
- [ ] Widget system responsiveness
- [ ] Memory leak verification
- [ ] Cross-platform build verification

### Automated Testing
- **Build system**: Autotools configure checks
- **Compiler warnings**: `-Wall -Werror` enforced
- **Static analysis**: Consider adding clang-static-analyzer

## Documentation

### User Documentation
- `README`: Basic usage and dependencies
- `LASTFM_SETUP.md`: Comprehensive Last.fm setup guide
- In-code: Extensive Doxygen-style comments

### Developer Documentation
- Header files: Well-documented public APIs
- Implementation files: Architecture explanations
- This file: Comprehensive project context

## IMPORTANT: Maintenance Directives

### Self-Updating File Directives:
1. **When fixing bugs**: Document the root cause, symptoms, and fix in the "Recent Major Work" section
2. **When adding features**: Update the "Architecture Overview" and add to "Current Status" 
3. **When changing dependencies**: Update both the "Build System" section and mention compatibility impacts
4. **When discovering performance issues**: Add to "Known Issues & Technical Debt" with specific metrics
5. **When making architectural changes**: Update relevant sections and add migration notes for future developers

### When Making Changes, Always Update:
1. **This file (`CLAUDE.md`)**: Add new context, mark completed items
2. **Build number**: Automatically increments on Windows commits  
3. **README**: If dependencies or features change
4. **Comments**: Keep code documentation current

### When Adding New Features:
1. **Follow existing patterns**: Study similar implementations first
2. **Cross-platform consideration**: Test on Windows and Linux
3. **Error handling**: Use exceptions consistently with descriptive messages
4. **Resource management**: Prefer RAII and smart pointers
5. **Thread safety**: Document threading requirements

### When Fixing Bugs:
1. **Root cause analysis**: Don't just patch symptoms
2. **Initialization**: Check for uninitialized variables (common issue)
3. **Platform differences**: Consider Windows vs Unix behavior
4. **Test thoroughly**: Verify fix doesn't break other functionality

### Code Style Guidelines:
- **Modern C++**: Use C++17 features appropriately
- **RAII**: Prefer automatic resource management
- **const correctness**: Mark methods const where appropriate
- **Descriptive names**: Self-documenting code preferred
- **Error handling**: Throw exceptions with context, don't return error codes

## Emergency Recovery Information

### If Build System Breaks:
```bash
# Clean and regenerate everything
make distclean
autoreconf -fiv
./configure
make
```

### If Windows Build Fails:
1. Check windres is available: `which windres`
2. Verify socket libraries linked: Look for `-lws2_32 -lmswsock` in build output
3. Ensure resource file built: Check `res/psymp3.res` exists
4. Verify build order: `res` directory should build before `src`

### If Audio Issues Occur:
1. Check Stream constructor initialization
2. Verify codec-specific structure initialization (especially Vorbis)
3. Look for null pointer dereferences in codec implementations
4. Check debug output for garbage values in sample rate/channels

---

**Last Updated**: January 9, 2025 (Build 669)
**Next Reviewer**: Update this section when continuing development