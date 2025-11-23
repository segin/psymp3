# Design Document

## Overview

This design document outlines the systematic approach to completing namespace migration for the PsyMP3 codebase. The migration will be executed in phases to ensure incremental verification and minimize risk. The design covers namespace structure, file organization, class renaming, build system updates, and verification strategies.

## Architecture

### Namespace Hierarchy

The PsyMP3 namespace hierarchy follows a three-level structure:

```
PsyMP3                          // Top-level namespace
├── IO                          // I/O subsystem
│   ├── File                    // File I/O handlers
│   └── HTTP                    // HTTP I/O handlers
├── Demuxer                     // Container demuxer subsystem
│   ├── ISO                     // ISO/MP4 demuxer
│   ├── RIFF                    // RIFF/WAV demuxer
│   ├── Raw                     // Raw audio demuxer
│   ├── FLAC                    // FLAC demuxer
│   └── Ogg                     // Ogg demuxer
├── Codec                       // Audio codec subsystem
│   ├── PCM                     // PCM and G.711 codecs
│   ├── FLAC                    // FLAC codec
│   ├── Opus                    // Opus codec
│   ├── Vorbis                  // Vorbis codec
│   └── MP3                     // MP3 codec
├── Widget                      // UI widget subsystem
│   ├── Foundation              // Base widget classes
│   ├── Windowing               // Window management
│   └── UI                      // UI components
├── LastFM                      // Last.fm integration
├── MPRIS                       // MPRIS D-Bus integration (new)
└── Util                        // Utility functions (new)
```

### Directory Structure

The physical directory structure mirrors the namespace hierarchy:

```
src/
├── io/                         // PsyMP3::IO
│   ├── file/                   // PsyMP3::IO::File
│   ├── http/                   // PsyMP3::IO::HTTP
│   ├── IOHandler.cpp
│   ├── MemoryPoolManager.cpp   // Moved from src/
│   ├── MemoryOptimizer.cpp     // Moved from src/
│   ├── MemoryTracker.cpp       // Moved from src/
│   ├── BufferPool.cpp          // Moved from src/
│   ├── RAIIFileHandle.cpp      // Moved from src/
│   └── ...
├── demuxer/                    // PsyMP3::Demuxer
│   ├── iso/                    // PsyMP3::Demuxer::ISO
│   ├── riff/                   // PsyMP3::Demuxer::RIFF
│   ├── raw/                    // PsyMP3::Demuxer::Raw
│   ├── flac/                   // PsyMP3::Demuxer::FLAC (moved from demuxers/)
│   ├── ogg/                    // PsyMP3::Demuxer::Ogg (moved from demuxers/)
│   └── ...
├── codecs/                     // PsyMP3::Codec
│   ├── pcm/                    // PsyMP3::Codec::PCM
│   ├── flac/                   // PsyMP3::Codec::FLAC
│   ├── opus/                   // PsyMP3::Codec::Opus
│   ├── vorbis/                 // PsyMP3::Codec::Vorbis
│   ├── mp3/                    // PsyMP3::Codec::MP3
│   └── ...
├── widget/                     // PsyMP3::Widget
│   ├── foundation/             // PsyMP3::Widget::Foundation
│   ├── windowing/              // PsyMP3::Widget::Windowing
│   ├── ui/                     // PsyMP3::Widget::UI
│   └── ...
├── lastfm/                     // PsyMP3::LastFM
├── mpris/                      // PsyMP3::MPRIS (new)
│   ├── MPRISManager.cpp        // Moved from src/
│   ├── DBusConnectionManager.cpp
│   ├── MPRISTypes.cpp
│   ├── MethodHandler.cpp
│   ├── PropertyManager.cpp
│   ├── SignalEmitter.cpp
│   └── Makefile.am
├── util/                       // PsyMP3::Util (new)
│   ├── XMLUtil.cpp             // Moved from src/
│   └── Makefile.am
└── [core files remain in root]
```

## Components and Interfaces

### Phase 1: Namespace Existing Subsystem Files

**Objective:** Add proper namespaces to files already in subsystem directories without moving them.

**Components:**
1. **Codec Subsystem Files** (~12 files)
   - Files in `src/codecs/pcm/`, `src/codecs/opus/`, `src/codecs/vorbis/`, `src/codecs/mp3/`
   - Add appropriate `PsyMP3::Codec::<Component>` namespaces

2. **Demuxer Subsystem Files** (~12 files)
   - Files in `src/demuxer/iso/`, `src/demuxer/riff/`, `src/demuxer/raw/`
   - Add appropriate `PsyMP3::Demuxer::<Component>` namespaces

3. **Widget Subsystem Files** (~15 files)
   - Files in `src/widget/foundation/`, `src/widget/ui/`, `src/widget/windowing/`
   - Add appropriate `PsyMP3::Widget::<Component>` namespaces

4. **IO Subsystem Files** (~2 files)
   - Files in `src/io/http/` (HTTPIOHandler.cpp)
   - Add `PsyMP3::IO::HTTP` namespace

**Interface Pattern:**
```cpp
// File: src/codecs/opus/OpusCodec.cpp
#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace Opus {

// Implementation here

} // namespace Opus
} // namespace Codec
} // namespace PsyMP3
```

### Phase 2: Fold demuxers/ into demuxer/

**Objective:** Consolidate demuxer code into a single directory structure.

**Steps:**
1. Use `git mv` to move `src/demuxers/flac/` to `src/demuxer/flac/`
2. Use `git mv` to move `src/demuxers/ogg/` to `src/demuxer/ogg/`
3. Verify namespaces are already correct (`PsyMP3::Demuxer::FLAC` and `PsyMP3::Demuxer::Ogg`)
4. Update `src/demuxer/Makefile.am` to include new subdirectories
5. Remove `src/demuxers/` directory and its Makefile.am
6. Update `configure.ac` to remove demuxers references

**Build System Changes:**
```makefile
# src/demuxer/Makefile.am
SUBDIRS = iso riff raw flac ogg

# Add to library list
psymp3_LDADD += \
    demuxer/flac/libflacdemuxer.a \
    demuxer/ogg/liboggdemuxer.a
```

### Phase 3: Create New Subsystems

**Objective:** Create MPRIS and Util subsystems with proper structure.

**MPRIS Subsystem:**
1. Create `src/mpris/` directory
2. Create `src/mpris/Makefile.am` for `libmpris.a`
3. Move MPRIS files using `git mv`:
   - `src/MPRISManager.cpp` → `src/mpris/MPRISManager.cpp`
   - `src/DBusConnectionManager.cpp` → `src/mpris/DBusConnectionManager.cpp`
   - `src/MPRISTypes.cpp` → `src/mpris/MPRISTypes.cpp`
   - `src/MethodHandler.cpp` → `src/mpris/MethodHandler.cpp`
   - `src/PropertyManager.cpp` → `src/mpris/PropertyManager.cpp`
   - `src/SignalEmitter.cpp` → `src/mpris/SignalEmitter.cpp`
4. Verify/add `PsyMP3::MPRIS` namespace to all files
5. Update `configure.ac` to add `src/mpris/Makefile`
6. Update `src/Makefile.am` to link `mpris/libmpris.a`

**Util Subsystem:**
1. Create `src/util/` directory
2. Create `src/util/Makefile.am` for `libutil.a`
3. Move utility files using `git mv`:
   - `src/XMLUtil.cpp` → `src/util/XMLUtil.cpp`
4. Add `PsyMP3::Util` namespace
5. Update `configure.ac` to add `src/util/Makefile`
6. Update `src/Makefile.am` to link `util/libutil.a`

**Makefile.am Template:**
```makefile
# src/mpris/Makefile.am
noinst_LIBRARIES = libmpris.a

libmpris_a_SOURCES = \
    MPRISManager.cpp \
    DBusConnectionManager.cpp \
    MPRISTypes.cpp \
    MethodHandler.cpp \
    PropertyManager.cpp \
    SignalEmitter.cpp

libmpris_a_CXXFLAGS = $(AM_CXXFLAGS)
```

### Phase 4: Move IO-Related Files

**Objective:** Centralize all I/O functionality in the IO subsystem.

**Files to Move:**
1. Memory management files:
   - `src/MemoryPoolManager.cpp` → `src/io/MemoryPoolManager.cpp`
   - `src/MemoryOptimizer.cpp` → `src/io/MemoryOptimizer.cpp`
   - `src/MemoryTracker.cpp` → `src/io/MemoryTracker.cpp`

2. Buffer pool files:
   - `src/BufferPool.cpp` → `src/io/BufferPool.cpp`
   - `src/EnhancedBufferPool.cpp` → `src/io/EnhancedBufferPool.cpp`
   - `src/EnhancedAudioBufferPool.cpp` → `src/io/EnhancedAudioBufferPool.cpp`
   - `src/BoundedBuffer.cpp` → `src/io/BoundedBuffer.cpp`

3. RAII wrapper:
   - `src/RAIIFileHandle.cpp` → `src/io/RAIIFileHandle.cpp`

**Process:**
1. Use `git mv` for each file
2. Add `PsyMP3::IO` namespace to each file
3. Update `src/io/Makefile.am` to include new files
4. Update `src/Makefile.am` to remove old file references

### Phase 5: Remove Redundant Class Prefixes

**Objective:** Clean up ISO demuxer helper class names.

**Classes to Rename:**
- `ISODemuxer` → Keep as `ISODemuxer` (main class)
- `ISODemuxerBoxParser` → `BoxParser`
- `ISODemuxerComplianceValidator` → `ComplianceValidator`
- `ISODemuxerErrorRecovery` → `ErrorRecovery`
- `ISODemuxerFragmentHandler` → `FragmentHandler`
- `ISODemuxerMetadataExtractor` → `MetadataExtractor`
- `ISODemuxerSampleTableManager` → `SampleTableManager`
- `ISODemuxerSeekingEngine` → `SeekingEngine`
- `ISODemuxerStreamManager` → `StreamManager`

**Renaming Process:**
1. Update class declaration in header file
2. Update class implementation in source file
3. Update all references in other ISO demuxer files
4. Update forward declarations
5. Verify build succeeds
6. Repeat for next class

**Example:**
```cpp
// Before
namespace PsyMP3 {
namespace Demuxer {
namespace ISO {
class ISODemuxerBoxParser { ... };
}}}

// After
namespace PsyMP3 {
namespace Demuxer {
namespace ISO {
class BoxParser { ... };
}}}
```

## Data Models

### File Migration Tracking

Track migration progress using a structured approach:

```
Phase 1: Namespace Existing Files
├── Codec Subsystem
│   ├── pcm/ (3 files)
│   ├── opus/ (1 file)
│   ├── vorbis/ (1 file)
│   └── mp3/ (1 file)
├── Demuxer Subsystem
│   ├── iso/ (9 files)
│   ├── riff/ (1 file)
│   └── raw/ (1 file)
├── Widget Subsystem
│   ├── foundation/ (~5 files)
│   ├── windowing/ (~5 files)
│   └── ui/ (~10 files)
└── IO Subsystem
    └── http/ (1 file)

Phase 2: Fold demuxers/
├── Move flac/ (1 file)
├── Move ogg/ (1 file)
└── Update build system

Phase 3: Create New Subsystems
├── MPRIS (6 files)
└── Util (1 file)

Phase 4: Move IO Files
└── 8 files to src/io/

Phase 5: Rename Classes
└── 8 ISO helper classes
```

### Namespace Declaration Format

All namespace declarations follow this format:

```cpp
/*
 * File header comment
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Subsystem {
namespace Component {  // Optional third level

// Implementation code (no indentation)

} // namespace Component
} // namespace Subsystem
} // namespace PsyMP3
```

**Key Points:**
- Opening brace on same line as namespace keyword
- No indentation within namespace blocks
- Closing comments indicate which namespace is closing
- File must end with exactly one newline after final closing brace

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Namespace Consistency
*For any* source file in a subsystem directory, the namespace declaration must match the directory structure.

**Validates: Requirements 1.1-1.5, 2.1-2.15**

**Test Approach:**
- Parse all .cpp files in subsystem directories
- Extract namespace declarations
- Verify namespace matches expected pattern based on file path
- Example: `src/codecs/opus/OpusCodec.cpp` must have `PsyMP3::Codec::Opus`

### Property 2: Master Header Inclusion
*For any* source file in a subsystem directory, the first include must be `#include "psymp3.h"`.

**Validates: Requirements 3.1-3.4**

**Test Approach:**
- Parse all .cpp files in subsystem directories
- Find first `#include` directive
- Verify it is exactly `#include "psymp3.h"`

### Property 3: File Ending Compliance
*For any* modified source file, the file must end with exactly one newline character.

**Validates: Requirements 6.6-6.7**

**Test Approach:**
- Read last byte of each modified file
- Verify it is `\n` (0x0A)
- Verify there are no additional trailing newlines

### Property 4: Build System Consistency
*For any* subsystem directory with source files, a corresponding Makefile.am must exist and be referenced in parent Makefile.am.

**Validates: Requirements 7.1-7.5**

**Test Approach:**
- List all subsystem directories containing .cpp files
- Verify each has a Makefile.am
- Verify parent Makefile.am includes subdirectory in SUBDIRS
- Verify configure.ac includes subsystem Makefile

### Property 5: Namespace Closure Completeness
*For any* namespace opening in a file, there must be a corresponding closing with comment.

**Validates: Requirements 6.1-6.5**

**Test Approach:**
- Parse namespace declarations in each file
- Count opening braces
- Count closing braces with comments
- Verify counts match and comments are correct

### Property 6: Class Prefix Removal Correctness
*For any* ISO helper class, the class name must not contain the `ISODemuxer` prefix.

**Validates: Requirements 12.1-12.7**

**Test Approach:**
- Parse class declarations in `src/demuxer/iso/` files
- Verify main class `ISODemuxer` retains its name
- Verify helper classes do not have `ISODemuxer` prefix
- Verify all references are updated

### Property 7: Git History Preservation
*For any* file moved between directories, the git history must be preserved.

**Validates: Requirements 8.2, 9.4, 11.1-11.2**

**Test Approach:**
- After file moves, run `git log --follow <file>`
- Verify history shows commits before the move
- Verify `git mv` was used (not copy+delete)

## Error Handling

### Compilation Errors

**Strategy:** Stop immediately on compilation errors and fix before proceeding.

**Process:**
1. After modifying a file, rebuild only that file: `make -C src FileName.o`
2. If compilation fails:
   - Read error message carefully
   - Check for missing namespace closures
   - Check for incorrect namespace nesting
   - Check for missing includes
   - Fix error and retry
3. Do not proceed to next file until current file compiles

### Missing References

**Strategy:** Update all references when renaming classes.

**Process:**
1. Before renaming a class, search for all references:
   ```bash
   grep -r "OldClassName" include/ src/
   ```
2. Update header file first
3. Update source file
4. Update all referencing files
5. Verify build succeeds

### Build System Errors

**Strategy:** Verify build system changes incrementally.

**Process:**
1. After modifying Makefile.am, regenerate build system:
   ```bash
   ./generate-configure.sh
   ./configure
   ```
2. Attempt build: `make -j$(nproc)`
3. If build fails:
   - Check SUBDIRS are correct
   - Check library names match
   - Check source file lists are complete
   - Fix and retry

### Git History Issues

**Strategy:** Always use `git mv` to preserve history.

**Process:**
1. Never use `mv` command directly
2. Always use: `git mv src/old/path.cpp src/new/path.cpp`
3. Verify move was recorded: `git status`
4. Commit move separately from content changes

## Testing Strategy

### Unit Testing

**Namespace Verification Tests:**
- Create a test script that parses all .cpp files
- Extract namespace declarations
- Verify against expected patterns
- Run after each phase completion

**Build Verification Tests:**
- Clean build: `make clean && make -j$(nproc)`
- Verify no warnings or errors
- Run after each subsystem is complete

**File Format Tests:**
- Check all files end with newline
- Check no files have trailing whitespace
- Run before committing changes

### Property-Based Testing

**Property 1 Test: Namespace Consistency**
```python
def test_namespace_consistency():
    for file in find_cpp_files("src/"):
        if is_subsystem_file(file):
            expected_ns = derive_namespace_from_path(file)
            actual_ns = extract_namespace(file)
            assert actual_ns == expected_ns, f"{file}: expected {expected_ns}, got {actual_ns}"
```

**Property 2 Test: Master Header Inclusion**
```python
def test_master_header_inclusion():
    for file in find_cpp_files("src/"):
        if is_subsystem_file(file):
            first_include = extract_first_include(file)
            assert first_include == '#include "psymp3.h"', f"{file}: wrong first include"
```

**Property 3 Test: File Ending Compliance**
```python
def test_file_endings():
    for file in find_modified_files():
        with open(file, 'rb') as f:
            content = f.read()
            assert content[-1:] == b'\n', f"{file}: missing final newline"
            assert content[-2:-1] != b'\n', f"{file}: extra trailing newlines"
```

### Integration Testing

**Full Build Test:**
```bash
# Clean build from scratch
make distclean
./generate-configure.sh
./configure
make -j$(nproc)

# Verify success
echo $?  # Should be 0
```

**Subsystem Build Test:**
```bash
# Test individual subsystem builds
make -C src/io clean && make -C src/io
make -C src/demuxer clean && make -C src/demuxer
make -C src/codecs clean && make -C src/codecs
make -C src/widget clean && make -C src/widget
make -C src/lastfm clean && make -C src/lastfm
make -C src/mpris clean && make -C src/mpris
make -C src/util clean && make -C src/util
```

## Implementation Phases

### Phase 1: Namespace Existing Files (No Moves)
**Duration:** ~40 files
**Risk:** Low - no file moves, only namespace additions

**Execution Order:**
1. Codec subsystem (smallest, ~6 files)
2. IO subsystem (1 file)
3. Demuxer subsystem (11 files)
4. Widget subsystem (largest, ~20 files)

**Verification:** Build each subsystem after completion

### Phase 2: Fold demuxers/ into demuxer/
**Duration:** 2 files + build system
**Risk:** Medium - involves file moves and build changes

**Execution Order:**
1. Move flac/ subdirectory
2. Move ogg/ subdirectory
3. Update demuxer/Makefile.am
4. Update configure.ac
5. Remove demuxers/ directory
6. Full build verification

### Phase 3: Create New Subsystems
**Duration:** 7 files + build system
**Risk:** Medium - new subsystems and file moves

**Execution Order:**
1. Create MPRIS subsystem (6 files)
2. Create Util subsystem (1 file)
3. Update configure.ac
4. Update src/Makefile.am
5. Full build verification

### Phase 4: Move IO Files
**Duration:** 8 files
**Risk:** Low - files already have namespaces

**Execution Order:**
1. Move memory management files (3 files)
2. Move buffer pool files (4 files)
3. Move RAII file handle (1 file)
4. Update io/Makefile.am
5. Update src/Makefile.am
6. Full build verification

### Phase 5: Rename ISO Classes
**Duration:** 8 classes
**Risk:** Medium - requires updating many references

**Execution Order:**
1. Rename one helper class at a time
2. Update all references
3. Verify build after each rename
4. Commit each rename separately

## Backward Compatibility

### Using Declarations

Add `using` declarations in `psymp3.h` for commonly-used types:

```cpp
// Backward compatibility - bring namespaced types into global scope
namespace PsyMP3 {
    namespace IO {
        class IOHandler;
        class FileIOHandler;
        class HTTPClient;
    }
    namespace Demuxer {
        class Demuxer;
        class ISODemuxer;
        class FLACDemuxer;
        class OggDemuxer;
    }
    namespace Codec {
        class AudioCodec;
        class PCMCodec;
    }
    namespace Widget {
        namespace Foundation {
            class Widget;
            class DrawableWidget;
        }
    }
}

// Bring commonly-used types into global namespace for backward compatibility
using PsyMP3::IO::IOHandler;
using PsyMP3::Demuxer::Demuxer;
using PsyMP3::Codec::AudioCodec;
using PsyMP3::Widget::Foundation::Widget;
using PsyMP3::Widget::Foundation::DrawableWidget;
// Add more as needed based on compilation errors
```

### Verification Strategy

After each phase:
1. Verify existing code compiles without modification
2. If compilation errors occur, add necessary `using` declarations
3. Document which types need global scope access

## Performance Considerations

### Build Time Impact

**Mitigation:**
- Use parallel builds: `make -j$(nproc)`
- Build incrementally (one subsystem at a time)
- Use ccache if available

**Expected Impact:**
- Minimal - namespace changes don't affect compilation time
- File moves may trigger rebuilds of dependent files

### Runtime Impact

**Expected Impact:**
- None - namespaces are compile-time only
- No runtime overhead from namespace organization

## Summary

This design provides a systematic, phased approach to completing namespace migration for PsyMP3. The five-phase execution plan minimizes risk through incremental verification, while the property-based testing strategy ensures correctness at each step. The design maintains backward compatibility through using declarations and preserves git history through proper use of `git mv`.

Key success factors:
1. Incremental execution with verification after each phase
2. Proper use of `git mv` to preserve history
3. Consistent namespace formatting
4. Build system updates synchronized with file moves
5. Comprehensive testing at each phase
