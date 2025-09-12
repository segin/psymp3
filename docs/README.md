# PsyMP3 Documentation Index

This directory contains comprehensive documentation for PsyMP3 development, architecture, and specifications.

## Architecture Documentation

### Core Systems
- [Threading Safety Guidelines](threading-safety-patterns.md) - Mandatory threading patterns for deadlock prevention
- [Threading Safety Analysis](threading_safety_analysis.md) - Comprehensive threading safety analysis
- [Threading Safety Framework Usage](threading-safety-framework-usage.md) - Testing framework for threading safety

### Audio Processing
- [Demuxer API](demuxer-api.md) - Demuxer interface and usage
- [Demuxer Developer Guide](demuxer-developer-guide.md) - Guide for implementing new demuxers
- [Demuxer Troubleshooting](demuxer-troubleshooting.md) - Common demuxer issues and solutions

### FLAC Support
- [FLAC Codec Developer Guide](flac-codec-developer-guide.md) - FLAC codec implementation guide
- [FLAC Demuxer API](flac-demuxer-api.md) - FLAC demuxer interface
- [FLAC Demuxer Developer Guide](flac-demuxer-developer-guide.md) - FLAC demuxer implementation
- [FLAC Demuxer Troubleshooting](flac-demuxer-troubleshooting.md) - FLAC-specific troubleshooting
- [FLAC Frame Indexing Architecture](flac-frame-indexing-architecture.md) - Frame indexing system
- [FLAC Performance Optimizations](flac-performance-optimizations.md) - Performance tuning guide

### Ogg Support
- [Ogg Demuxer Developer Guide](ogg-demuxer-developer-guide.md) - Ogg container demuxer implementation

### MPRIS Integration
- [MPRIS Architecture](mpris-architecture.md) - Complete MPRIS system architecture
- [MPRIS Troubleshooting](mpris-troubleshooting.md) - MPRIS debugging and issue resolution
- [MPRIS Migration Notes](mpris-migration-notes.md) - Changes and migration guide

### Debug and Development
- [Debug Channels](debug-channels.md) - Debug logging system usage

## Specifications and Standards

### Format Specifications
- [RFC Reference Index](RFC_REFERENCE_INDEX.md) - Complete index of audio format specifications

### FLAC (RFC 9639)
- [RFC 9639 FLAC Summary](RFC9639_FLAC_SUMMARY.md) - FLAC specification summary
- [rfc9639.txt](rfc9639.txt) - Complete FLAC specification

### Ogg Container (RFC 3533/3534)
- [RFC 3533 Ogg Summary](RFC3533_OGG_SUMMARY.md) - Ogg container specification summary
- [rfc3533.txt](rfc3533.txt) - Complete Ogg encapsulation specification
- [rfc3534.txt](rfc3534.txt) - Ogg media type specification

### Opus Codec (RFC 6716/7845)
- [RFC 6716 Opus Summary](RFC6716_OPUS_SUMMARY.md) - Opus codec specification summary
- [rfc6716.txt](rfc6716.txt) - Complete Opus codec specification
- [rfc7845.txt](rfc7845.txt) - Opus in Ogg encapsulation

### Vorbis Codec
- [Vorbis Spec Summary](VORBIS_SPEC_SUMMARY.md) - Vorbis specification summary
- [vorbis-spec.html](vorbis-spec.html) - Complete Vorbis I specification

## Testing Documentation

### Test Summaries
- [Integration Summary](INTEGRATION_SUMMARY.md) - System integration testing results
- [MPRIS Comprehensive Tests Summary](MPRIS_COMPREHENSIVE_TESTS_SUMMARY.md) - MPRIS testing results

### Threading Safety Testing
- [Threading Safety Analysis Tools](threading-safety-analysis-tools.md) - Tools for threading analysis
- [Threading Safety Maintenance](threading-safety-maintenance.md) - Ongoing threading safety practices
- [Threading Safety Comprehensive Report](threading_safety_comprehensive_report.md) - Complete threading analysis

## Quick Reference

### For Developers
- **New to PsyMP3**: Start with [Threading Safety Guidelines](threading-safety-patterns.md)
- **Adding Codecs**: See [Demuxer Developer Guide](demuxer-developer-guide.md)
- **FLAC Development**: See [FLAC Codec Developer Guide](flac-codec-developer-guide.md)
- **MPRIS Development**: See [MPRIS Architecture](mpris-architecture.md)
- **Debugging Issues**: Check relevant troubleshooting guides

### For Format Implementation
- **Ogg Containers**: [RFC 3533 Summary](RFC3533_OGG_SUMMARY.md)
- **FLAC Codec**: [RFC 9639 Summary](RFC9639_FLAC_SUMMARY.md)
- **Vorbis Codec**: [Vorbis Spec Summary](VORBIS_SPEC_SUMMARY.md)
- **Opus Codec**: [RFC 6716 Summary](RFC6716_OPUS_SUMMARY.md)

### For Troubleshooting
- **FLAC Issues**: [FLAC Demuxer Troubleshooting](flac-demuxer-troubleshooting.md)
- **MPRIS Issues**: [MPRIS Troubleshooting](mpris-troubleshooting.md)
- **General Demuxer Issues**: [Demuxer Troubleshooting](demuxer-troubleshooting.md)
- **Threading Issues**: [Threading Safety Analysis](threading_safety_analysis.md)

## Documentation Standards

### Writing Guidelines
- Use clear, concise language
- Include code examples where appropriate
- Reference official specifications when applicable
- Maintain consistent formatting and structure

### Maintenance
- Update documentation when code changes
- Validate examples and commands
- Keep cross-references current
- Review for accuracy during releases

## Contributing

When adding new documentation:

1. **Follow existing structure** and naming conventions
2. **Update this index** to include new documents
3. **Cross-reference** related documentation
4. **Include examples** and practical guidance
5. **Test all commands** and code examples

For questions about documentation, see the main project README or contact the development team.