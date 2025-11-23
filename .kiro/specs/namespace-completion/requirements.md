# Requirements Document

## Introduction

This specification addresses the systematic completion of namespace migration for the PsyMP3 codebase. The project has adopted a modular architecture with proper C++ namespacing (`PsyMP3::<Subsystem>::<Component>`), and significant work has already been completed (~30 files migrated). This spec will:

1. Add proper namespaces to ~40 files already in subsystem directories
2. Create new subsystems (MPRIS, Util) and move ~15 files into them
3. Ensure all ~105 source files follow consistent namespace patterns
4. Maintain backward compatibility throughout the migration

Analysis shows that core application files (player, audio, playlist, etc.) should remain in src/ root without subsystem namespaces, while subsystem-specific files need proper namespace organization.

## Glossary

- **Namespace**: A C++ language feature that provides scope for identifiers to prevent name collisions
- **Subsystem**: A major functional area of PsyMP3 (e.g., IO, Demuxer, Codec, Widget, LastFM)
- **Component**: A specific implementation within a subsystem (e.g., File, HTTP, ISO, FLAC)
- **Source File**: A `.cpp` implementation file in the PsyMP3 codebase
- **Namespace Declaration**: The `namespace PsyMP3 { namespace Subsystem { ... } }` structure wrapping implementation code
- **Using Declaration**: A `using` statement in `psymp3.h` that brings namespaced types into global scope for backward compatibility

## Requirements

### Requirement 1

**User Story:** As a developer, I want all source files in subsystem directories to use proper namespaces, so that the codebase has consistent organization and prevents name collisions.

#### Acceptance Criteria

1. WHEN a source file exists in `src/codecs/` or its subdirectories, THEN the system SHALL wrap all implementations in `namespace PsyMP3 { namespace Codec { ... } }`
2. WHEN a source file exists in `src/demuxer/` or its subdirectories, THEN the system SHALL wrap all implementations in `namespace PsyMP3 { namespace Demuxer { ... } }`
3. WHEN a source file exists in `src/io/` or its subdirectories, THEN the system SHALL wrap all implementations in `namespace PsyMP3 { namespace IO { ... } }`
4. WHEN a source file exists in `src/widget/` or its subdirectories, THEN the system SHALL wrap all implementations in `namespace PsyMP3 { namespace Widget { ... } }`
5. WHEN a source file exists in `src/lastfm/`, THEN the system SHALL wrap all implementations in `namespace PsyMP3 { namespace LastFM { ... } }`

### Requirement 2

**User Story:** As a developer, I want component-specific subdirectories to have additional namespace layers, so that related code is properly grouped and organized.

#### Acceptance Criteria

1. WHEN a source file exists in `src/codecs/flac/`, THEN the system SHALL use `namespace PsyMP3 { namespace Codec { namespace FLAC { ... } } }`
2. WHEN a source file exists in `src/codecs/opus/`, THEN the system SHALL use `namespace PsyMP3 { namespace Codec { namespace Opus { ... } } }`
3. WHEN a source file exists in `src/codecs/vorbis/`, THEN the system SHALL use `namespace PsyMP3 { namespace Codec { namespace Vorbis { ... } } }`
4. WHEN a source file exists in `src/codecs/pcm/`, THEN the system SHALL use `namespace PsyMP3 { namespace Codec { namespace PCM { ... } } }`
5. WHEN a source file exists in `src/codecs/mp3/`, THEN the system SHALL use `namespace PsyMP3 { namespace Codec { namespace MP3 { ... } } }`
6. WHEN a source file exists in `src/demuxer/iso/`, THEN the system SHALL use `namespace PsyMP3 { namespace Demuxer { namespace ISO { ... } } }`
7. WHEN a source file exists in `src/demuxer/riff/`, THEN the system SHALL use `namespace PsyMP3 { namespace Demuxer { namespace RIFF { ... } } }`
8. WHEN a source file exists in `src/demuxer/raw/`, THEN the system SHALL use `namespace PsyMP3 { namespace Demuxer { namespace Raw { ... } } }`
9. WHEN a source file exists in `src/demuxers/flac/`, THEN the system SHALL use `namespace PsyMP3 { namespace Demuxer { namespace FLAC { ... } } }`
10. WHEN a source file exists in `src/demuxers/ogg/`, THEN the system SHALL use `namespace PsyMP3 { namespace Demuxer { namespace Ogg { ... } } }`
11. WHEN a source file exists in `src/io/file/`, THEN the system SHALL use `namespace PsyMP3 { namespace IO { namespace File { ... } } }`
12. WHEN a source file exists in `src/io/http/`, THEN the system SHALL use `namespace PsyMP3 { namespace IO { namespace HTTP { ... } } }`
13. WHEN a source file exists in `src/widget/foundation/`, THEN the system SHALL use `namespace PsyMP3 { namespace Widget { namespace Foundation { ... } } }`
14. WHEN a source file exists in `src/widget/windowing/`, THEN the system SHALL use `namespace PsyMP3 { namespace Widget { namespace Windowing { ... } } }`
15. WHEN a source file exists in `src/widget/ui/`, THEN the system SHALL use `namespace PsyMP3 { namespace Widget { namespace UI { ... } } }`

### Requirement 3

**User Story:** As a developer, I want all source files to follow the master header pattern, so that compilation is consistent and header dependencies are properly managed.

#### Acceptance Criteria

1. WHEN a source file in a subsystem directory is modified for namespacing, THEN the system SHALL ensure it includes only `psymp3.h` as the master header
2. WHEN a source file includes system or library headers directly, THEN the system SHALL move those includes to `psymp3.h`
3. WHEN a source file is in a subdirectory, THEN the system SHALL NOT include individual component headers directly
4. WHEN reviewing a namespaced source file, THEN the system SHALL verify the first include is `#include "psymp3.h"`

### Requirement 4

**User Story:** As a developer, I want the codebase to maintain backward compatibility, so that existing code continues to work without modification.

#### Acceptance Criteria

1. WHEN a class is moved into a namespace, THEN the system SHALL add appropriate `using` declarations in `psymp3.h`
2. WHEN a commonly-used type is namespaced, THEN the system SHALL bring it into global scope via `using` declaration
3. WHEN backward compatibility is needed, THEN the system SHALL document the `using` declarations with comments explaining their purpose
4. WHEN all files in a subsystem are namespaced, THEN the system SHALL verify existing code still compiles without modification

### Requirement 5

**User Story:** As a developer, I want to track which files have been migrated, so that I can ensure complete coverage and avoid duplicate work.

#### Acceptance Criteria

1. WHEN beginning namespace migration, THEN the system SHALL identify all source files in subsystem directories
2. WHEN a file is successfully migrated, THEN the system SHALL verify it compiles cleanly
3. WHEN all files in a subdirectory are migrated, THEN the system SHALL verify the subsystem builds successfully
4. WHEN migration is complete, THEN the system SHALL verify the entire project builds without errors
5. WHEN tracking progress, THEN the system SHALL maintain a list of completed files organized by subsystem

### Requirement 6

**User Story:** As a developer, I want namespace declarations to follow consistent formatting, so that the codebase is readable and maintainable.

#### Acceptance Criteria

1. WHEN adding namespace declarations, THEN the system SHALL place opening braces on the same line as the namespace keyword
2. WHEN closing namespaces, THEN the system SHALL add comments indicating which namespace is being closed
3. WHEN nesting namespaces, THEN the system SHALL use separate namespace declarations (not C++17 nested syntax)
4. WHEN formatting namespace content, THEN the system SHALL NOT indent code within namespace blocks
5. WHEN a file ends, THEN the system SHALL ensure proper closing of all namespace blocks before the final newline
6. WHEN any source file is modified, THEN the system SHALL ensure the file ends with exactly one newline character (`\n`)
7. WHEN verifying file endings, THEN the system SHALL NOT allow files to end with any byte other than a newline

### Requirement 7

**User Story:** As a developer, I want the build system to remain functional during migration, so that I can test changes incrementally.

#### Acceptance Criteria

1. WHEN modifying a source file for namespacing, THEN the system SHALL rebuild only that file and verify it compiles
2. WHEN a compilation error occurs, THEN the system SHALL fix the error before proceeding to the next file
3. WHEN a subsystem is complete, THEN the system SHALL perform a full build of that subsystem
4. WHEN all subsystems are complete, THEN the system SHALL perform a full project build
5. WHEN build errors occur, THEN the system SHALL NOT proceed until they are resolved

### Requirement 8

**User Story:** As a developer, I want new subsystems created for MPRIS and utility code, so that related functionality is properly organized.

#### Acceptance Criteria

1. WHEN creating the MPRIS subsystem, THEN the system SHALL create `src/mpris/` directory with appropriate Makefile.am
2. WHEN moving MPRIS files, THEN the system SHALL use `git mv` to preserve history
3. WHEN MPRIS files are moved, THEN the system SHALL wrap implementations in `namespace PsyMP3 { namespace MPRIS { ... } }`
4. WHEN creating the Util subsystem, THEN the system SHALL create `src/util/` directory with appropriate Makefile.am
5. WHEN utility files are moved, THEN the system SHALL wrap implementations in `namespace PsyMP3 { namespace Util { ... } }`

### Requirement 9

**User Story:** As a developer, I want IO-related files moved from src/ root to src/io/, so that all IO functionality is centralized.

#### Acceptance Criteria

1. WHEN moving memory management files, THEN the system SHALL move them to `src/io/` and wrap in `PsyMP3::IO` namespace
2. WHEN moving buffer pool files, THEN the system SHALL move them to `src/io/` and wrap in `PsyMP3::IO` namespace
3. WHEN moving RAII file handle, THEN the system SHALL move it to `src/io/` and wrap in `PsyMP3::IO` namespace
4. WHEN files are moved, THEN the system SHALL use `git mv` to preserve history
5. WHEN files are moved, THEN the system SHALL update src/io/Makefile.am to include them in the build

### Requirement 10

**User Story:** As a developer, I want core application files to remain in src/ root, so that the application structure remains clear and maintainable.

#### Acceptance Criteria

1. WHEN reviewing src/ root files, THEN the system SHALL NOT move player.cpp, audio.cpp, playlist.cpp, or other core application files
2. WHEN core files remain in src/ root, THEN the system SHALL NOT add subsystem namespaces to them
3. WHEN core files need namespacing, THEN the system SHALL only use top-level `PsyMP3` namespace if necessary
4. WHEN distinguishing core vs subsystem files, THEN the system SHALL follow the file analysis document
5. WHEN in doubt about file placement, THEN the system SHALL consult the user before moving files

### Requirement 11

**User Story:** As a developer, I want the `demuxers` directory folded into `demuxer`, so that all demuxer code is in a single consistent location.

#### Acceptance Criteria

1. WHEN folding demuxers into demuxer, THEN the system SHALL move `src/demuxers/flac/` to `src/demuxer/flac/` using `git mv`
2. WHEN folding demuxers into demuxer, THEN the system SHALL move `src/demuxers/ogg/` to `src/demuxer/ogg/` using `git mv`
3. WHEN demuxer subdirectories are moved, THEN the system SHALL verify they already have proper `PsyMP3::Demuxer::<Format>` namespaces
4. WHEN demuxers directory is empty, THEN the system SHALL remove the `src/demuxers/` directory and its Makefile.am
5. WHEN updating build system, THEN the system SHALL integrate moved demuxers into `src/demuxer/Makefile.am`
6. WHEN build system is updated, THEN the system SHALL remove references to `src/demuxers/` from configure.ac and parent Makefiles

### Requirement 12

**User Story:** As a developer, I want redundant prefixes removed from helper class names, so that namespaced code is cleaner and more idiomatic.

#### Acceptance Criteria

1. WHEN a helper class is in `PsyMP3::Demuxer::ISO` namespace and has `ISODemuxer` prefix, THEN the system SHALL remove the `ISODemuxer` prefix from the class name
2. WHEN the main demuxer class `ISODemuxer` is encountered, THEN the system SHALL keep the name `ISODemuxer` unchanged
3. WHEN renaming classes, THEN the system SHALL update all references in headers, source files, and forward declarations
4. WHEN renaming classes, THEN the system SHALL update Makefile.am files if filenames change
5. WHEN a class like `ISODemuxerBoxParser` is renamed to `BoxParser`, THEN the system SHALL ensure it remains in `PsyMP3::Demuxer::ISO` namespace
6. WHEN removing prefixes, THEN the system SHALL verify the build succeeds after each class is renamed
7. WHEN similar patterns exist in other subsystems, THEN the system SHALL apply the same prefix removal logic to helper classes only
