# Command Usage Guidelines

## Head Command Usage

When using the `head` command to limit output, you are **forbidden** from using sizes as small as `10`. 

**Minimum Requirements:**
- Use `head -50` or larger for meaningful output analysis
- For debug output analysis, prefer `head -100` or more
- For log analysis, use `head -200` or more

**Rationale:**
Small head sizes like `head -10` provide insufficient context for debugging and analysis, leading to incomplete understanding of issues and requiring multiple iterations to gather adequate information.

## PsyMP3 Debug Channels

**IMPORTANT:** Only use the debug channels listed below. These are the actual channels available in PsyMP3.

### Available Debug Channels:
- **HTTPIOHandler** - HTTP I/O operations and network requests
- **audio** - Audio system operations and playback
- **chunk** - Audio chunk processing and buffering
- **codec** - General codec operations and initialization
- **compliance** - Format compliance validation
- **demux** - General demuxer operations
- **demuxer** - Demuxer factory and registration
- **display** - Display and rendering operations
- **error** - Error handling and reporting
- **flac** - FLAC format processing and parsing
- **flac_benchmark** - FLAC performance benchmarking
- **flac_codec** - FLAC codec operations
- **flac_rfc_validator** - FLAC RFC 9639 compliance validation
- **font** - Font rendering and text display
- **http** - HTTP client operations
- **io** - General I/O operations
- **iso** - ISO container format processing
- **iso_compliance** - ISO format compliance validation
- **lastfm** - Last.fm scrobbling operations
- **loader** - Media file loading operations
- **lyrics** - Lyrics processing and display
- **memory** - Memory management and optimization
- **mp3** - MP3 format processing
- **mpris** - MPRIS media player interface
- **ogg** - Ogg container format processing
- **opus** - Opus format processing
- **opus_codec** - Opus codec operations
- **performance** - Performance monitoring and optimization
- **player** - Media player core operations
- **playlist** - Playlist management
- **plugin** - Plugin system operations
- **raii** - RAII resource management
- **resource** - Resource allocation and management
- **spectrum** - Spectrum analyzer operations
- **stream** - Stream management and processing
- **streaming** - Network streaming operations
- **system** - System-level operations
- **test** - Testing framework operations
- **timer** - Timing and scheduling operations
- **vorbis** - Vorbis format processing
- **widget** - UI widget operations

### Usage Examples:
```bash
# CORRECT - Using actual debug channels
./src/psymp3 --debug=flac,demux "file.flac" 2>&1 | head -50
./src/psymp3 --debug=flac_codec,flac_rfc_validator "file.flac" 2>&1 | head -100
./src/psymp3 --debug=all --logfile=debug.log "file.flac"

# FORBIDDEN - Using non-existent channels
./src/psymp3 --debug=flac_demuxer "file.flac"  # flac_demuxer does not exist!
./src/psymp3 --debug=flac_parser "file.flac"   # flac_parser does not exist!
```

### Channel Selection Guidelines:
- **For FLAC issues:** Use `flac`, `flac_codec`, `flac_rfc_validator`, `demux`
- **For container parsing:** Use `demux`, `demuxer`, `compliance`
- **For codec issues:** Use `codec`, `flac_codec`, `opus_codec`
- **For I/O problems:** Use `io`, `HTTPIOHandler`, `stream`
- **For performance analysis:** Use `performance`, `flac_benchmark`, `memory`
- **For debugging everything:** Use `all` (but prefer specific channels for focused debugging)