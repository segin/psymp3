# Design Document

## Overview

This design implements a registry-based conditional codec architecture for PsyMP3. The system will replace the current hardcoded conditional compilation checks scattered throughout the codebase with a centralized registration system. Codecs and demuxers will register themselves at application startup, and the MediaFactory will query these registries instead of using preprocessor conditionals.

The key architectural change is moving from compile-time conditional logic distributed across multiple files to a runtime registry system with conditional compilation isolated to registration functions only.

## Architecture

### Registry System

The system introduces two main registries:

1. **Codec Registry** - Manages available audio codecs
2. **Demuxer Registry** - Manages available container demuxers

Both registries will be populated at application startup through dedicated registration functions that contain all conditional compilation logic.

### Registration Flow

```
Application Startup
    ↓
registerAllCodecs()     ← Contains all #ifdef logic for codecs
    ↓
registerAllDemuxers()   ← Contains all #ifdef logic for demuxers
    ↓
MediaFactory uses registries (no #ifdef needed)
```

### Conditional Compilation Strategy

- **Centralized**: All `#ifdef` checks are in registration functions only
- **Granular**: Each codec/demuxer can be independently enabled/disabled
- **Dependency-aware**: OggDemuxer registration depends on available Ogg codecs

## Components and Interfaces

### CodecRegistry Class

```cpp
class CodecRegistry {
public:
    static void registerCodec(const std::string& codec_name, CodecFactoryFunc factory);
    static std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    static bool isCodecSupported(const std::string& codec_name);
    static std::vector<std::string> getSupportedCodecs();
    
private:
    static std::map<std::string, CodecFactoryFunc> s_codec_factories;
};
```

### DemuxerRegistry Class

```cpp
class DemuxerRegistry {
public:
    static void registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory);
    static std::unique_ptr<Demuxer> createDemuxer(const std::string& format_id, std::unique_ptr<IOHandler> handler);
    static bool isFormatSupported(const std::string& format_id);
    static std::vector<std::string> getSupportedFormats();
    
private:
    static std::map<std::string, DemuxerFactoryFunc> s_demuxer_factories;
};
```

### Registration Functions

#### registerAllCodecs()
```cpp
void registerAllCodecs() {
#ifdef HAVE_MP3
    CodecRegistry::registerCodec("mp3", [](const StreamInfo& info) {
        return std::make_unique<MP3PassthroughCodec>(info);
    });
#endif

#ifdef HAVE_VORBIS
    CodecRegistry::registerCodec("vorbis", [](const StreamInfo& info) {
        return std::make_unique<VorbisPassthroughCodec>(info);
    });
#endif

#ifdef HAVE_OPUS
    CodecRegistry::registerCodec("opus", [](const StreamInfo& info) {
        return std::make_unique<OpusPassthroughCodec>(info);
    });
#endif

#ifdef HAVE_FLAC
    CodecRegistry::registerCodec("flac", [](const StreamInfo& info) {
        return std::make_unique<FLACCodec>(info);
    });
#endif

    // Always register PCM, A-law, μ-law (no conditional compilation)
    CodecRegistry::registerCodec("pcm", [](const StreamInfo& info) {
        return std::make_unique<PCMCodec>(info);
    });
    CodecRegistry::registerCodec("alaw", [](const StreamInfo& info) {
        return std::make_unique<ALawCodec>(info);
    });
    CodecRegistry::registerCodec("mulaw", [](const StreamInfo& info) {
        return std::make_unique<MuLawCodec>(info);
    });
}
```

#### registerAllDemuxers()
```cpp
void registerAllDemuxers() {
    // Always register these demuxers (no conditional compilation)
    DemuxerRegistry::registerDemuxer("riff", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<ChunkDemuxer>(std::move(handler));
    });
    DemuxerRegistry::registerDemuxer("aiff", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<ChunkDemuxer>(std::move(handler));
    });
    DemuxerRegistry::registerDemuxer("mp4", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<ISODemuxer>(std::move(handler));
    });

#ifdef HAVE_FLAC
    DemuxerRegistry::registerDemuxer("flac", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<FLACDemuxer>(std::move(handler));
    });
#endif

    // OggDemuxer registration depends on available Ogg codecs
#if defined(HAVE_VORBIS) || defined(HAVE_OPUS) || (defined(HAVE_FLAC) && defined(HAVE_OGG))
    DemuxerRegistry::registerDemuxer("ogg", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<OggDemuxer>(std::move(handler));
    });
#endif
}
```

### OggDemuxer Conditional Compilation

The OggDemuxer will have explicit `#ifdef` blocks for each codec:

```cpp
// In OggDemuxer.cpp
std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
#ifdef HAVE_VORBIS
    if (hasSignature(packet_data, "\x01vorbis")) {
        return "vorbis";
    }
#endif

#ifdef HAVE_OPUS
    if (hasSignature(packet_data, "OpusHead")) {
        return "opus";
    }
#endif

#ifdef HAVE_FLAC
    if (hasSignature(packet_data, "\x7FFLAC")) {
        return "flac";
    }
#endif

    return ""; // Unknown codec
}
```

## Data Models

### StreamInfo Enhancement

The existing `StreamInfo` structure will be enhanced to work better with the registry system:

```cpp
struct StreamInfo {
    // Existing fields...
    uint32_t stream_id;
    std::string codec_name;
    std::string codec_type;
    
    // Enhanced fields for registry system
    bool is_supported;           // Set by registry lookup
    std::string unsupported_reason; // Why codec isn't supported
};
```

### MediaFormat Enhancement

The `MediaFormat` structure in MediaFactory will be enhanced to work with the registry and provide better format detection:

```cpp
struct MediaFormat {
    // Existing fields...
    std::string format_id;
    std::vector<std::string> extensions;
    std::vector<std::string> mime_types;
    std::vector<std::string> magic_signatures;
    int priority;
    
    // Enhanced fields
    bool requires_demuxer;       // True if needs demuxer registry
    std::vector<std::string> supported_codecs; // Codecs this format can contain
    bool is_container;           // True for container formats
    std::string description;     // Human-readable description
};
```

### Standardized Extension Mappings

The system will use standardized file extension mappings:

```cpp
// Ogg container formats
ogg_format.extensions = {"OGG", "OGA", "OPUS"};
ogg_format.mime_types = {"application/ogg", "audio/ogg", "audio/opus"};

// ISO container formats  
iso_format.extensions = {"MOV", "MP4", "M4A", "3GP"};
iso_format.mime_types = {"audio/mp4", "audio/m4a", "video/mp4", "video/quicktime"};

// FLAC format
flac_format.extensions = {"FLAC"};
flac_format.mime_types = {"audio/flac", "audio/x-flac"};

// MP3 elementary stream
mp3_format.extensions = {"MP3", "MPA", "MPGA", "BIT", "MP2", "M2A", "MP2A"};
mp3_format.mime_types = {"audio/mpeg", "audio/mp3", "audio/x-mp3"};
```

## Format Detection and Routing

### Enhanced Detection Strategy

The system will use a multi-layered approach for accurate format detection:

1. **Magic Byte Detection**: Primary method using file signatures
2. **MIME Type Detection**: For web-served content
3. **Extension-based Detection**: Fallback method
4. **Content Analysis**: Deep inspection for ambiguous cases

### Priority-based Resolution

When multiple formats match, the system uses priority scoring:

```cpp
class FormatDetector {
public:
    struct DetectionResult {
        std::string format_id;
        float confidence;
        std::string detection_method;
        std::string reason;
    };
    
    static DetectionResult detectFormat(IOHandler* handler, const std::string& file_path, const std::string& mime_type);
    
private:
    static DetectionResult detectByMagicBytes(IOHandler* handler);
    static DetectionResult detectByMimeType(const std::string& mime_type);
    static DetectionResult detectByExtension(const std::string& file_path);
    static DetectionResult detectByContentAnalysis(IOHandler* handler);
};
```

### Ogg Container Codec Detection

For Ogg containers, the system will probe for specific codec signatures:

```cpp
// In OggDemuxer.cpp
std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
#ifdef HAVE_VORBIS
    if (hasSignature(packet_data, "\x01vorbis")) {
        return "vorbis";
    }
#endif

#ifdef HAVE_OPUS
    if (hasSignature(packet_data, "OpusHead")) {
        return "opus";
    }
#endif

#ifdef HAVE_FLAC
    if (hasSignature(packet_data, "\x7FFLAC")) {
        return "flac";
    }
#endif

    return ""; // Unknown codec - will be rejected
}
```

## Error Handling

### Graceful Degradation

When a codec or demuxer is not available:

1. **MediaFactory**: Returns appropriate error messages indicating missing codec support
2. **Registry Queries**: Return false for unsupported formats with descriptive reasons
3. **User Feedback**: Clear error messages about which codecs are disabled
4. **Format Detection**: Provides detailed failure reasons

### Error Types

```cpp
class UnsupportedCodecException : public std::runtime_error {
public:
    UnsupportedCodecException(const std::string& codec_name, const std::string& reason);
    const std::string& getCodecName() const;
    const std::string& getReason() const;
};

class UnsupportedFormatException : public std::runtime_error {
public:
    UnsupportedFormatException(const std::string& format_id, const std::string& reason);
    const std::string& getFormatId() const;
    const std::string& getReason() const;
};

class FormatDetectionException : public std::runtime_error {
public:
    FormatDetectionException(const std::string& file_path, const std::string& reason);
    const std::string& getFilePath() const;
    const std::string& getReason() const;
};
```

## Testing Strategy

### Unit Tests

1. **Registry Tests**:
   - Test codec registration and lookup
   - Test demuxer registration and lookup
   - Test conditional compilation scenarios

2. **Integration Tests**:
   - Test MediaFactory with different codec combinations
   - Test OggDemuxer with various codec availability scenarios
   - Test error handling for unsupported formats

3. **Build Configuration Tests**:
   - Test builds with different codec combinations disabled
   - Verify that disabled codecs are properly excluded
   - Test that FLAC demuxer is always built when FLAC codec is enabled

### Test Scenarios

#### Codec Combination Tests

1. **All codecs enabled**: Verify full functionality
2. **Only Vorbis enabled**: Verify Ogg demuxer supports only Vorbis
3. **Only Opus enabled**: Verify Ogg demuxer supports only Opus  
4. **Only FLAC enabled**: Verify both FLAC demuxer and Ogg FLAC support
5. **No Ogg codecs enabled**: Verify Ogg demuxer is not compiled
6. **MP3 only**: Verify legacy MP3 architecture still works

#### Error Handling Tests

1. **Unsupported codec requests**: Verify proper error messages
2. **Unsupported format requests**: Verify graceful failure
3. **Partial codec support**: Verify Ogg demuxer rejects unsupported codecs

### Performance Tests

1. **Registry Lookup Performance**: Ensure registry queries are fast
2. **Startup Time**: Verify registration doesn't significantly impact startup
3. **Memory Usage**: Verify registry doesn't consume excessive memory

## Implementation Notes

### Migration Strategy

1. **Phase 1**: Implement registry classes and registration functions
2. **Phase 2**: Update MediaFactory to use registries
3. **Phase 3**: Add conditional compilation to OggDemuxer
4. **Phase 4**: Update existing codec specs for registry support
5. **Phase 5**: Remove old conditional compilation from MediaFactory

### Backward Compatibility

- Existing Stream classes continue to work unchanged
- Public APIs remain the same
- Only internal implementation changes to use registries

### Build System Integration

The build system will be optimized to avoid compiling disabled codec files entirely:

#### configure.ac Updates
- Enhanced conditional logic to determine which source files to compile
- New variables for codec-specific source file lists
- Improved dependency checking for codec combinations
- **Individual dependency checking**: Replace the combined `PKG_CHECK_MODULES([DEPENDS], [sdl >= 1.2 taglib >= 1.6 openssl >= 1.0 libcurl >= 7.20.0])` with individual checks for better error reporting:

```bash
# Individual core dependency checks for better error reporting
PKG_CHECK_MODULES([SDL_CHECK], [sdl >= 1.2])
PKG_CHECK_MODULES([TAGLIB_CHECK], [taglib >= 1.6]) 
PKG_CHECK_MODULES([OPENSSL_CHECK], [openssl >= 1.0])
PKG_CHECK_MODULES([CURL_CHECK], [libcurl >= 7.20.0])
```

- **Codec dependency validation**: Ensure invalid combinations are caught:

```bash
# Validate codec dependencies
if test "x$have_flac" = "xyes"; then
  # FLAC codec requires FLAC demuxer - always enable both
  have_flac_demuxer=yes
fi

if test "x$have_vorbis" = "xyes" && test "x$have_ogg" = "xno"; then
  AC_MSG_ERROR([Vorbis support requires libogg])
fi

if test "x$have_opus" = "xyes" && test "x$have_ogg" = "xno"; then
  AC_MSG_ERROR([Opus support requires libogg])
fi
```

- **Enhanced configuration summary**: Detailed reporting of enabled features and reasons for disabled features

This ensures that if any core dependency is missing, the configure script will clearly indicate which specific library is not found, rather than giving a generic "one of these is missing" error.

#### src/Makefile.am Updates
```makefile
# Base source files (always compiled)
psymp3_SOURCES = \
    main.cpp \
    MediaFactory.cpp \
    DemuxerFactory.cpp \
    AudioCodec.cpp \
    CodecRegistry.cpp \
    DemuxerRegistry.cpp

# Conditional codec sources
if HAVE_MP3
psymp3_SOURCES += MP3Codec.cpp
endif

if HAVE_VORBIS
psymp3_SOURCES += VorbisCodec.cpp
endif

if HAVE_OPUS
psymp3_SOURCES += OpusCodec.cpp
endif

if HAVE_FLAC
psymp3_SOURCES += FLACCodec.cpp FLACDemuxer.cpp
endif

if HAVE_OGGDEMUXER
psymp3_SOURCES += OggDemuxer.cpp OggCodecs.cpp
endif
```

#### Benefits
- Faster compilation (fewer files to compile)
- Smaller binary size (no dead code)
- Cleaner build process
- Better dependency management

The existing configure.ac conditional compilation will be enhanced, with `#ifdef` checks centralized in registration functions and the build system optimized to exclude disabled codec source files entirely.