# Project Context

## About PsyMP3
PsyMP3 appears to be a multimedia player application with the following characteristics:

### Core Components
- **Audio Processing**: Support for multiple codecs (MP3, FLAC, Opus, Vorbis, PCM)
- **Demuxing**: Various container format support (OGG, ISO, RIFF)
- **UI Framework**: Custom widget-based windowing system
- **Streaming**: HTTP and file-based I/O handling
- **Integration**: Last.fm scrobbling, MPRIS support

### Architecture Patterns
- **Widget System**: Custom UI framework with drawable widgets, layouts, and windowing
- **Codec Architecture**: Pluggable audio codec system with conditional compilation support
- **Demuxer Pattern**: Factory-based demuxer selection for different container formats
- **I/O Abstraction**: Handler-based system for different input sources
- **Conditional Compilation**: Codec and demuxer support determined at build time based on available dependencies

### Key Libraries and Dependencies
- SDL for low-level graphics and audio
- Various audio libraries (libmpg123, FLAC, Opus, Vorbis)
- HTTP client functionality
- XML processing capabilities
- Font rendering (TrueType support)

## Development Notes
- This is a mature codebase with established patterns
- Focus on maintaining consistency with existing architecture
- Consider backward compatibility when making changes
- The custom widget system suggests this predates modern UI frameworks
- Recent refactoring introduced conditional compilation for codec and demuxer support to handle optional dependencies gracefully