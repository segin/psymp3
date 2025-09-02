# Development Standards

## Code Organization

### Header Inclusion Policy
- **Master Header Rule**: All `.cpp` files should only include `psymp3.h` as the master header
- All other system and library includes must go in the master header (`psymp3.h`)
- This ensures consistent compilation environment and reduces header dependency issues

### Directory Structure
- **Source Code**: All implementation files (`.cpp`) go in `src/`
- **Resources**: Windows resource scripts, assets, fonts (like BitStream Vera Sans) go in `res/`
- **Tests**: All test files go in `tests/`
- **Headers**: Public headers go in `include/`

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

### Codec Implementation Standards
- Follow the established pluggable codec architecture
- Ensure codec registration with MediaFactory is conditionally compiled
- Maintain backward compatibility when adding new codec support
- Test codec implementations with and without their dependencies available

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