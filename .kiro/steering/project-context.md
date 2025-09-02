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

### Technical Specifications

#### FLAC Format
- **FLAC Specification**: The official FLAC specification is available in `docs/rfc9639.txt` (RFC 9639) with a summary in `docs/RFC9639_FLAC_SUMMARY.md`
- When working with FLAC demuxer implementation, always consult the RFC for authoritative format details, frame structure, and bit-level encoding
- All FLAC implementation decisions should be validated against RFC 9639 to ensure compliance

#### Ogg Container Format
- **Ogg Specification**: The official Ogg encapsulation format is defined in `docs/rfc3533.txt` (RFC 3533) with a summary in `docs/RFC3533_OGG_SUMMARY.md`
- **Media Type**: Additional Ogg media type information is available in `docs/rfc3534.txt` (RFC 3534)
- When working with Ogg demuxer implementation, always consult RFC 3533 for page structure, packet boundaries, and stream multiplexing
- All Ogg container implementation decisions should be validated against RFC 3533 to ensure proper page parsing and stream handling

#### Vorbis Audio Codec
- **Vorbis Specification**: The official Vorbis I specification is available in `docs/vorbis-spec.html` with a summary in `docs/VORBIS_SPEC_SUMMARY.md`
- When working with Vorbis codec implementation, always consult the specification for header structure, audio encoding process, and quality parameters
- All Vorbis implementation decisions should be validated against the official specification to ensure proper decoding and quality handling

#### Opus Audio Codec
- **Opus Specification**: The official Opus codec specification is available in `docs/rfc6716.txt` (RFC 6716) with a summary in `docs/RFC6716_OPUS_SUMMARY.md`
- **Ogg Encapsulation**: Opus-specific Ogg encapsulation is defined in `docs/rfc7845.txt` (RFC 7845)
- When working with Opus codec implementation, always consult RFC 6716 for packet structure, frame sizes, and audio modes
- All Opus implementation decisions should be validated against RFC 6716 and RFC 7845 to ensure proper decoding and Ogg container integration

## Development Notes
- This is a mature codebase with established patterns
- Focus on maintaining consistency with existing architecture
- Consider backward compatibility when making changes
- The custom widget system suggests this predates modern UI frameworks
- Recent refactoring introduced conditional compilation for codec and demuxer support to handle optional dependencies gracefully