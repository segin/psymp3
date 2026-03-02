# Development Standards

## Code Organization

### File Formatting Requirements
- **MANDATORY**: Every source file (`.cpp`, `.h`, `.am`, etc.) MUST end with a blank line (final newline character)
- **CRITICAL**: If a closing bracket or brace appears to be missing at the end of a file, it is likely due to the lack of a final newline
- **Enforcement**: Always verify that files end with a blank line before committing
- **Rationale**: POSIX compliance, proper text file format, prevents parsing issues and compiler warnings

### Header Inclusion Policy
- **Master Header Rule**: All `.cpp` files should only include `psymp3.h` as the master header
- All other system and library includes must go in the master header (`psymp3.h`)
- This ensures consistent compilation environment and reduces header dependency issues

### Directory Structure
- **Source Code**: All implementation files (`.cpp`) go in `src/`
- **Resources**: Windows resource scripts, assets, fonts (like BitStream Vera Sans) go in `res/`
- **Tests**: All test files go in `tests/`
- **Headers**: Public headers go in `include/`

### Modular Architecture
PsyMP3 follows a modular architecture where major subsystems are organized into subdirectories with their own build systems:

#### Subsystem Organization Pattern
Each major subsystem should be organized as follows:
- **Source Directory**: `src/<subsystem>/` contains implementation files
- **Header Directory**: `include/<subsystem>/` contains public headers
- **Build System**: Each subsystem has its own `Makefile.am` that builds a convenience library
- **Convenience Libraries**: Subsystems build `lib<subsystem>.a` static libraries
- **Subdirectories**: Complex subsystems may have further subdirectories (e.g., `src/io/http/`, `src/demuxer/iso/`)
- **Namespacing**: All classes should be in appropriate namespaces (e.g., `PsyMP3::IO::`, `PsyMP3::Demuxer::ISO::`)
- **Backward Compatibility**: Use `using` declarations in `psymp3.h` to bring types into global namespace where needed

#### Established Subsystems

##### Widget System (`src/widget/`, `include/widget/`)
Organized into three layers:
- **Foundation** (`widget/foundation/`): Base widget classes (Widget, DrawableWidget, LayoutWidget, FadingWidget)
- **Windowing** (`widget/windowing/`): Window management (TitlebarWidget, WindowFrameWidget, WindowWidget, TransparentWindowWidget)
- **UI** (`widget/ui/`): UI components (ButtonWidget, SpectrumAnalyzerWidget, PlayerProgressBarWidget, ToastWidget, etc.)

**Namespace**: `PsyMP3::Widget::Foundation::`, `PsyMP3::Widget::Windowing::`, `PsyMP3::Widget::UI::`

##### Codec System (`src/codecs/`, `include/codecs/`)
Organized by codec type with shared infrastructure:
- **Core** (`codecs/`): Shared codec infrastructure (AudioCodec, CodecRegistry, CodecRegistration)
- **PCM** (`codecs/pcm/`): PCM and G.711 codecs (PCMCodec, ALawCodec, MuLawCodec)
- **FLAC** (`codecs/flac/`): Native FLAC decoder (BitstreamReader, FrameParser, NativeFLACCodec, etc.)
- **Opus** (`codecs/opus/`): Opus codec wrapper (OpusCodec)
- **Vorbis** (`codecs/vorbis/`): Vorbis codec wrapper (VorbisCodec)
- **MP3** (`codecs/mp3/`): MP3 codec wrapper (MP3Codec)

**Namespace**: `PsyMP3::Codec::`, `PsyMP3::Codec::PCM::`, `PsyMP3::Codec::FLAC::`, etc.

##### Demuxer System (`src/demuxer/`, `include/demuxer/`)
Organized by container format with shared infrastructure:
- **Core** (`demuxer/`): Shared demuxer infrastructure (Demuxer, DemuxerFactory, DemuxerRegistry, MediaFactory, ChainedStream, etc.)
- **ISO** (`demuxer/iso/`): ISO/MP4 container (ISODemuxer, ISODemuxerBoxParser, ISODemuxerSeekingEngine, etc.)
- **RIFF** (`demuxer/riff/`): RIFF/WAV container (wav.cpp)
- **Raw** (`demuxer/raw/`): Raw audio demuxer (RawAudioDemuxer)
- **FLAC** (`demuxers/flac/`): FLAC container (FLACDemuxer) - Note: uses old `demuxers` directory
- **Ogg** (`demuxers/ogg/`): Ogg container (OggDemuxer) - Note: uses old `demuxers` directory

**Namespace**: `PsyMP3::Demuxer::`, `PsyMP3::Demuxer::ISO::`, `PsyMP3::Demuxer::RIFF::`, etc.

##### I/O System (`src/io/`, `include/io/`)
Organized by I/O handler type:
- **Core** (`io/`): Base I/O infrastructure (IOHandler, TagLibIOHandlerAdapter, StreamingManager, URI)
- **File** (`io/file/`): File I/O handler (FileIOHandler)
- **HTTP** (`io/http/`): HTTP I/O handler (HTTPIOHandler, HTTPClient)

**Namespace**: `PsyMP3::IO::`, `PsyMP3::IO::File::`, `PsyMP3::IO::HTTP::`

##### Last.fm System (`src/lastfm/`, `include/lastfm/`)
Last.fm scrobbling functionality:
- **Core** (`lastfm/`): Scrobbling implementation (LastFM, Scrobble)

**Namespace**: `PsyMP3::LastFM::`

#### Modularization Guidelines
When adding new subsystems or refactoring existing code:

1. **Create Directory Structure**: Create both `src/<subsystem>/` and `include/<subsystem>/` directories
2. **Use git mv**: Always use `git mv` to move files to preserve history
3. **Create Makefile.am**: Each subsystem needs its own `Makefile.am` that builds a convenience library
4. **Update Parent Makefile**: Add subsystem to `SUBDIRS` in parent `Makefile.am`
5. **Update configure.ac**: Add subsystem Makefile to `AC_CONFIG_FILES`
6. **Link Library**: Add subsystem library to `psymp3_LDADD` in `src/Makefile.am`
7. **Update psymp3.h**: Update include paths to reference new locations
8. **Add Namespaces**: Wrap classes in appropriate namespaces
9. **Backward Compatibility**: Add `using` declarations in `psymp3.h` for commonly used types
10. **Test Build**: Verify clean build with `./generate-configure.sh && ./configure && make -j$(nproc)`

#### Convenience Library Naming
- Core subsystem: `lib<subsystem>.a` (e.g., `libio.a`, `libdemuxer.a`, `libcodecs.a`)
- Subsystem components: `lib<component><subsystem>.a` (e.g., `libfileio.a`, `libhttpio.a`, `libisodemuxer.a`)
- Codec libraries: `lib<codec>codec.a` (e.g., `libflaccodec.a`, `libopuscodec.a`)
- Demuxer libraries: `lib<format>demuxer.a` (e.g., `libflacdemuxer.a`, `liboggdemuxer.a`)

## Build System

### GNU Autotools
- Project uses GNU Autotools for build configuration
- **Rebuild toolchain**: Use `./generate-configure.sh` to regenerate build files
- `autogen.sh` exists as a symbolic link to `generate-configure.sh` for familiarity
- Always run `./generate-configure.sh` after modifying `configure.ac` or `Makefile.am` files

### Compilation Rules
- **NEVER invoke `gcc` or `g++` directly** when rebuilding changed files in the `src/` directory
- **Use Make**: Always use `make -C src File.o` to rebuild individual object files
- If already in the `src/` directory, use `make File.o` (omit the `-C src` portion)
- **Parallel Builds**: Always use `make -j$(nproc)` to utilize all available CPU cores for faster compilation
- This ensures proper dependency tracking and build consistency

### Build Output Rules - CRITICAL
- **NEVER PIPE BUILD COMMANDS**: Do NOT pipe `make`, `./configure`, or any build command output through `tail`, `head`, `grep`, or any other filter
- **Why**: Piping build output causes commands to buffer all output until completion, producing no incremental output. This causes timeouts because the system cannot detect progress
- **Correct**: `make -j$(nproc)` (let output flow naturally)
- **WRONG**: `make -j$(nproc) | tail -50` (causes timeout due to buffering)
- **WRONG**: `make 2>&1 | tail -100` (same problem)
- **Output Truncation**: If you need to limit output for context, use a timeout and let the command run naturally, then examine results afterward
- **Full Output is Essential**: Truncating output can hide critical error messages and context needed to diagnose problems

## Problem-Solving Philosophy

### Core Principle: No Shortcuts When Things Go Wrong
- **NEVER SIMPLIFY WHEN PROBLEMS OCCUR**: When encountering build failures, dependency issues, or complex technical problems, always pursue the complete and correct solution
- **Fix Root Causes**: Always address the underlying issue rather than working around it with shortcuts or simplified approaches
- **Dependency Resolution**: When dependencies are missing or broken, fix the dependency chain rather than disabling features
- **Build System Issues**: When build system problems occur, fix the build configuration rather than bypassing it
- **The Hard Path is Usually Correct**: Complex problems typically require comprehensive solutions - embrace the difficulty rather than avoiding it

### Anti-Patterns to Avoid
- Disabling features when dependencies fail to build
- Commenting out problematic code instead of fixing it
- Using workarounds instead of proper solutions
- Simplifying requirements to avoid technical challenges
- Taking shortcuts that create technical debt

## Version Control

### Git Best Practices
- **Build Artifacts**: Always put build artifacts in `.gitignore`
- **CRITICAL**: NEVER USE `git add .` EVER! Always add files explicitly
- Use specific file paths when adding to git to avoid accidentally committing unwanted files

### Git Workflow for Tasks
- **Task Completion**: Always perform a `git commit` and `git push` when done with each task or subtask
- **Include Task Files**: Make sure to include the `tasks.md` file for whatever task list is being currently worked on
- **Task Status Updates**: Include the `tasks.md` file to reflect the completed status of the specific task being worked on
- This ensures progress is tracked and shared consistently

### Build Requirements for Task Completion
- **Clean Build Requirement**: All tasks must build cleanly before being considered complete
- This requirement applies before `git commit` and updating `.gitignore`
- Use `make` to verify clean compilation before task completion

## Testing

### Current State
- **Known Issue**: No proper test harness currently exists
- **Priority**: Implementing a proper test framework should be addressed ASAP
- Individual test files exist in `tests/` but lack integration

### Test Organization
- Test files should follow naming convention: `test_<component>.cpp`
- Tests should be compilable and runnable independently where possible

## Threading Safety

### Mandatory Threading Pattern
- **Public/Private Lock Pattern**: All classes that use synchronization primitives (mutexes, spinlocks, etc.) MUST follow the public/private lock pattern
- **Public Methods**: Handle lock acquisition and call private `_unlocked` implementations
- **Private Methods**: Append `_unlocked` suffix and perform work without acquiring locks
- **Internal Calls**: Always use `_unlocked` versions when calling methods within the same class

### Threading Safety Requirements
- **Lock Documentation**: Document lock acquisition order at the class level to prevent deadlocks
- **RAII Lock Guards**: Always use RAII lock guards (`std::lock_guard`, `std::unique_lock`) instead of manual lock/unlock
- **Exception Safety**: Ensure locks are released even when exceptions occur
- **Callback Safety**: Never invoke callbacks while holding internal locks

### Code Review Checklist for Threading
When reviewing code that involves threading, verify:
- [ ] Every public method that acquires locks has a corresponding `_unlocked` private method
- [ ] Internal method calls use the `_unlocked` versions
- [ ] Lock acquisition order is documented and consistently followed
- [ ] RAII lock guards are used instead of manual lock/unlock
- [ ] Callbacks are never invoked while holding internal locks
- [ ] No public method calls other public methods that acquire the same locks
- [ ] Mutable mutexes are used for const methods that need to acquire locks
- [ ] Exception safety is maintained (locks released even on exceptions)

### Threading Test Requirements
All threading-related code changes must include:
- **Basic Thread Safety Tests**: Multiple threads calling the same public method
- **Deadlock Prevention Tests**: Scenarios that would cause deadlocks with incorrect patterns
- **Stress Tests**: High-concurrency scenarios to validate thread safety
- **Performance Tests**: Ensure lock overhead doesn't significantly impact performance

## Codec Development

### Conditional Compilation Architecture
- **Codec Support**: All codec implementations must use conditional compilation patterns
- **Factory Integration**: New codecs must integrate with the MediaFactory using conditional compilation guards
- **Build System Integration**: Codec availability should be determined at build time through configure checks
- **Preprocessor Guards**: Use appropriate `#ifdef` guards around codec-specific code to ensure clean builds when dependencies are unavailable

### Codec Directory Structure
- **Source Organization**: Codec implementations should be organized as submodules in `src/codecs/<codec_name>/`
- **Header Organization**: Codec headers should be placed in `include/codecs/<codec_name>/`
- **Build System**: Each codec subdirectory should have its own `Makefile.am` that builds a convenience library
- **Convenience Libraries**: Codec subdirectories build `lib<codec>codec.a` static libraries that are linked into the main binary
- **Conditional Building**: Use `SUBDIRS` with conditional compilation in parent `Makefile.am` to enable/disable codec builds
- **Namespacing**: All codec classes should be in `PsyMP3::Codec::<CodecName>` namespace
- **Backward Compatibility**: Use `using` declarations in `psymp3.h` to bring codec types into global namespace

### Codec Organization Examples

#### Native FLAC Codec (Complex Multi-File Codec)
The Native FLAC codec demonstrates organization for complex codecs with multiple components:

**Directory Structure:**
```
src/codecs/flac/          # Native FLAC codec implementation
  ├── Makefile.am         # Builds libnativeflac.la convenience library
  ├── BitstreamReader.cpp
  ├── CRCValidator.cpp
  ├── FrameParser.cpp
  ├── SubframeDecoder.cpp
  ├── ResidualDecoder.cpp
  └── NativeFLACCodec.cpp

include/codecs/flac/      # Native FLAC codec headers
  ├── BitstreamReader.h
  ├── CRCValidator.h
  ├── FrameParser.h
  ├── SubframeDecoder.h
  ├── ResidualDecoder.h
  └── NativeFLACCodec.h
```

**Build System Integration:**
- `src/codecs/Makefile.am` conditionally includes codec subdirectories via `SUBDIRS`
- `src/codecs/flac/Makefile.am` builds `libnativeflac.a` static library (7.5MB)
- `src/Makefile.am` links the library: `psymp3_LDADD += codecs/flac/libnativeflac.a`
- `configure.ac` includes all codec Makefiles in `AC_CONFIG_FILES`

**Namespace Pattern:**
```cpp
namespace PsyMP3 {
namespace Codec {
namespace FLAC {
    class BitstreamReader { /* ... */ };
    class CRCValidator { /* ... */ };
    class FrameParser { /* ... */ };
    class FLACCodec : public AudioCodec { /* ... */ };
}}}
```

**Include Path Pattern:**
- Headers are included with full path: `#include "codecs/flac/BitstreamReader.h"`
- Master header `psymp3.h` includes codec headers conditionally based on build flags
- Codec source files only include `psymp3.h` (master header rule)
- Using declaration in `psymp3.h`: `using PsyMP3::Codec::FLAC::FLACCodec;`

#### Opus Codec (Single-File Codec)
The Opus codec demonstrates organization for simpler single-file codecs:

**Directory Structure:**
```
src/codecs/opus/          # Opus codec implementation
  ├── Makefile.am         # Builds libopuscodec.a (6.8MB)
  └── OpusCodec.cpp

include/codecs/opus/      # Opus codec headers
  └── OpusCodec.h
```

**Namespace Pattern:**
```cpp
namespace PsyMP3 {
namespace Codec {
namespace Opus {
    class OpusCodec : public AudioCodec { /* ... */ };
    struct OpusHeader { /* ... */ };
    namespace OpusCodecSupport { /* ... */ }
}}}
```

**Using Declarations:**
```cpp
using PsyMP3::Codec::Opus::OpusCodec;
using PsyMP3::Codec::Opus::OpusHeader;
```

#### Vorbis Codec (Wrapper Codec)
The Vorbis codec demonstrates organization for library wrapper codecs:

**Directory Structure:**
```
src/codecs/vorbis/        # Vorbis codec implementation
  ├── Makefile.am         # Builds libvorbiscodec.a (890KB)
  └── VorbisCodec.cpp

include/codecs/vorbis/    # Vorbis codec headers
  └── VorbisCodec.h
```

**Namespace Pattern:**
```cpp
namespace PsyMP3 {
namespace Codec {
namespace Vorbis {
    class Vorbis : public Stream { /* ... */ };
}}}
```

### Codec Implementation Standards
- Follow the established pluggable codec architecture
- Ensure codec registration with MediaFactory is conditionally compiled
- Maintain backward compatibility when adding new codec support
- Test codec implementations with and without their dependencies available
- Complex codecs with multiple components should follow the subdirectory pattern
- Simple codec wrappers can remain in the main `src/` directory

### Format-Specific Development Guidelines

#### FLAC-Specific Guidelines
- **RFC Compliance**: All FLAC demuxer implementation must comply with RFC 9639 (available in `docs/rfc9639.txt`)
- **Reference Documentation**: When implementing or debugging FLAC features, consult `docs/RFC9639_FLAC_SUMMARY.md` for quick reference
- **Frame Boundary Detection**: FLAC frames are variable-length; always use proper sync pattern detection (0xFF followed by 0xF8-0xFF)
- **Metadata Handling**: Follow RFC specifications for metadata block parsing and validation
- **Error Recovery**: Implement robust error recovery for corrupted or incomplete FLAC streams
- **Performance**: Frame size estimation should be conservative but efficient to avoid excessive I/O operations

#### Ogg Container Guidelines
- **RFC Compliance**: All Ogg demuxer implementation must comply with RFC 3533 (available in `docs/rfc3533.txt`)
- **Reference Documentation**: When implementing or debugging Ogg features, consult `docs/RFC3533_OGG_SUMMARY.md` for quick reference
- **Page Parsing**: Always verify "OggS" capture pattern and validate CRC checksums for corruption detection
- **Packet Reconstruction**: Handle packet continuation across pages using continuation flags and segment tables
- **Stream Multiplexing**: Support multiple logical streams with unique serial numbers in single Ogg file
- **Seeking**: Implement bisection search using granule positions for efficient time-based seeking
- **Error Recovery**: Use page boundaries and sequence numbers for robust error recovery

#### Vorbis Codec Guidelines
- **Specification Compliance**: All Vorbis codec implementation must comply with the official Vorbis I specification (available in `docs/vorbis-spec.html`)
- **Reference Documentation**: When implementing or debugging Vorbis features, consult `docs/VORBIS_SPEC_SUMMARY.md` for quick reference
- **Header Processing**: Always validate identification, comment, and setup headers before processing audio packets
- **Framing Validation**: Verify framing flags in all Vorbis packets to ensure proper packet structure
- **Block Handling**: Implement proper overlap-add for variable block sizes (short/long blocks)
- **Quality Management**: Support VBR encoding and quality levels (-1 to 10 scale)
- **Error Recovery**: Handle corrupted packets gracefully with proper error concealment

#### Opus Codec Guidelines
- **RFC Compliance**: All Opus codec implementation must comply with RFC 6716 (available in `docs/rfc6716.txt`)
- **Ogg Encapsulation**: Follow RFC 7845 for Opus-specific Ogg container requirements (available in `docs/rfc7845.txt`)
- **Reference Documentation**: When implementing or debugging Opus features, consult `docs/RFC6716_OPUS_SUMMARY.md` for quick reference
- **TOC Parsing**: Properly decode Table of Contents byte for configuration, stereo flag, and frame count
- **Mode Handling**: Support SILK (speech), CELT (music), and hybrid modes based on configuration
- **Sample Rate**: Handle internal 48 kHz processing with automatic resampling for other rates
- **Pre-skip**: Implement proper pre-skip handling for encoder delay compensation
- **Packet Loss**: Implement forward error correction (FEC) and packet loss concealment algorithms