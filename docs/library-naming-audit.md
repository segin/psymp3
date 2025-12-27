# Library Naming Convention Audit

## Overview

This document tracks the naming conventions for all static libraries in PsyMP3. The project is transitioning to a consistent `libpsymp3-*` naming scheme for subsystem libraries.

## Naming Convention Standards

### Preferred Pattern: `libpsymp3-<subsystem>[-<component>].a`
- **Core subsystems**: `libpsymp3-core.a`, `libpsymp3-tag.a`, `libpsymp3-io.a`
- **Subsystem components**: `libpsymp3-io-file.a`, `libpsymp3-io-http.a`, `libpsymp3-core-utility.a`
- **Rationale**: Clear namespace, prevents conflicts, indicates project ownership

### Legacy Pattern: `lib<name>.a`
- Used by older subsystems and third-party-style components
- Examples: `libcodecs.a`, `libdemuxer.a`, `libwidgetui.a`

## Current Library Inventory

### ✅ Following `libpsymp3-*` Convention (7 libraries)

| Library | Location | Purpose |
|---------|----------|---------|
| `libpsymp3-core.a` | `src/core/` | Core functionality (FFT, song, exceptions, etc.) |
| `libpsymp3-core-utility.a` | `src/core/utility/` | Core utilities (UTF8, XML, utility functions) |
| `libpsymp3-tag.a` | `src/tag/` | Tag framework (ID3v1, ID3v2, Vorbis Comment, etc.) |
| `libpsymp3-io.a` | `src/io/` | I/O abstraction layer |
| `libpsymp3-io-file.a` | `src/io/file/` | File I/O handler |
| `libpsymp3-io-http.a` | `src/io/http/` | HTTP I/O handler |
| `libpsymp3-mpris.a` | `src/mpris/` | MPRIS D-Bus media player interface |

### ❌ Not Following Convention (17 libraries)

#### Codec Libraries (6 libraries)
| Library | Location | Suggested Name | Notes |
|---------|----------|----------------|-------|
| `libcodecs.a` | `src/codecs/` | `libpsymp3-codecs.a` | Core codec infrastructure |
| `libnativeflac.a` | `src/codecs/flac/` | `libpsymp3-codec-flac.a` | Native FLAC decoder (7.5MB) |
| `libmp3codec.a` | `src/codecs/mp3/` | `libpsymp3-codec-mp3.a` | MP3 codec wrapper |
| `libopuscodec.a` | `src/codecs/opus/` | `libpsymp3-codec-opus.a` | Opus codec wrapper |
| `libvorbiscodec.a` | `src/codecs/vorbis/` | `libpsymp3-codec-vorbis.a` | Vorbis codec wrapper |
| `libpcmcodecs.a` | `src/codecs/pcm/` | `libpsymp3-codec-pcm.a` | PCM/G.711 codecs |

#### Demuxer Libraries (6 libraries)
| Library | Location | Suggested Name | Notes |
|---------|----------|----------------|-------|
| `libdemuxer.a` | `src/demuxer/` | `libpsymp3-demuxer.a` | Core demuxer infrastructure |
| `libflacdemuxer.a` | `src/demuxer/flac/` | `libpsymp3-demuxer-flac.a` | FLAC container demuxer |
| `libisodemuxer.a` | `src/demuxer/iso/` | `libpsymp3-demuxer-iso.a` | ISO/MP4 container demuxer |
| `liboggdemuxer.a` | `src/demuxer/ogg/` | `libpsymp3-demuxer-ogg.a` | Ogg container demuxer |
| `librawdemuxer.a` | `src/demuxer/raw/` | `libpsymp3-demuxer-raw.a` | Raw audio demuxer |
| `libriffdemuxer.a` | `src/demuxer/riff/` | `libpsymp3-demuxer-riff.a` | RIFF/WAV container demuxer |

#### Widget Libraries (3 libraries)
| Library | Location | Suggested Name | Notes |
|---------|----------|----------------|-------|
| `libwidgetfoundation.a` | `src/widget/foundation/` | `libpsymp3-widget-foundation.a` | Base widget classes |
| `libwidgetwindowing.a` | `src/widget/windowing/` | `libpsymp3-widget-windowing.a` | Window management widgets |
| `libwidgetui.a` | `src/widget/ui/` | `libpsymp3-widget-ui.a` | UI component widgets |

#### Other Subsystems (2 libraries)
| Library | Location | Suggested Name | Notes |
|---------|----------|----------------|-------|
| `liblastfm.a` | `src/lastfm/` | `libpsymp3-lastfm.a` | Last.fm scrobbling |
| `libutil.a` | `src/util/` | `libpsymp3-util.a` | Utility functions (currently empty) |

## Migration Priority

### High Priority
1. **Codec libraries** - Most visible to external developers, frequently referenced
2. **Demuxer libraries** - Core functionality, frequently referenced
3. **Widget libraries** - Large subsystem with clear hierarchy

### Medium Priority
4. **Last.fm library** - Standalone feature module
5. **Util library** - Currently empty, easy to rename

### Low Priority
- Libraries that are conditionally compiled based on feature flags may require more careful migration

## Migration Considerations

### Build System Impact
- All `Makefile.am` files must be updated (src/, tests/, and subdirectories)
- `src/psymp3.final.cpp` may need updates for final build mode
- Test harness and debugging tools reference these libraries

### Backward Compatibility
- No external API changes required (library names are internal build artifacts)
- Git history preserved through `git mv` for library source files
- Old `.a` files must be cleaned after Makefile changes

### Testing Requirements
- Full build verification after each subsystem rename
- Test suite must pass with new library names
- Verify both regular and final build modes

## Implementation Pattern

For each library rename:

1. Update `noinst_LIBRARIES` in subsystem `Makefile.am`
2. Update `<library>_SOURCES` variable name to match new library name
3. Update all references in `src/Makefile.am` (psymp3_LDADD)
4. Update all references in `tests/Makefile.am`
5. Clean old library files: `make clean` or manual deletion
6. Rebuild: `make -j$(nproc)`
7. Verify tests build and pass
8. Commit with descriptive message
9. Push changes

## Status Tracking

- **Completed**: 7/24 libraries (29%)
- **Remaining**: 17/24 libraries (71%)
- **Last Updated**: December 27, 2025

## Related Documentation

- `AGENTS.md` - Project architecture and build system directives
- `.kiro/steering/development-standards.md` - Modular architecture guidelines
- `docs/demuxer-developer-guide.md` - Demuxer subsystem documentation
- `docs/flac-codec-developer-guide.md` - FLAC codec documentation
