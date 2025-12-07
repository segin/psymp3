# ISO Demuxer Implementation Plan

This implementation plan covers the complete refactoring of the ISO Base Media File Format demuxer based on the new modular architecture defined in design.md.

## Phase 1: Core Infrastructure

### Task 1: Create Directory Structure and Build System
- [ ] 1.1. Create `src/demuxer/iso/` directory for ISO demuxer implementation
- [ ] 1.2. Create `include/demuxer/iso/` directory for ISO demuxer headers
- [ ] 1.3. Create `src/demuxer/iso/Makefile.am` to build `libisodemuxer.a` convenience library
- [ ] 1.4. Update `src/demuxer/Makefile.am` to include `iso` in SUBDIRS
- [ ] 1.5. Update `configure.ac` to include `src/demuxer/iso/Makefile` in AC_CONFIG_FILES
- [ ] 1.6. Update `src/Makefile.am` to link `demuxer/iso/libisodemuxer.a`
- [ ] 1.7. Verify clean build with `./generate-configure.sh && ./configure && make -j$(nproc)`
- _Requirements: Project modular architecture standards_

### Task 2: Implement ISOBoxReader Component
- [ ] 2.1. Create `include/demuxer/iso/ISOBoxReader.h` with BoxHeader, FullBoxHeader, UUIDBoxHeader structures
- [ ] 2.2. Create `src/demuxer/iso/ISOBoxReader.cpp` with low-level I/O operations
- [ ] 2.3. Implement ReadBoxHeader() for 32-bit and 64-bit box sizes (size==1 for extended, size==0 for EOF)
- [ ] 2.4. Implement ReadFullBoxHeader() for version and flags extraction
- [ ] 2.5. Implement primitive reading methods (ReadUInt8, ReadUInt16BE, ReadUInt32BE, ReadUInt64BE)
- [ ] 2.6. Implement fixed-point reading (ReadFixed16_16, ReadFixed8_8, ReadMatrix)
- [ ] 2.7. Implement position management (GetPosition, Seek, Skip, GetFileSize)
- [ ] 2.8. Implement ValidateBoxBounds() for security validation
- [ ] 2.9. Add mutex for thread safety following public/private lock pattern
- _Requirements: 1.1, 1.8, 19.1-19.8_

### Task 3: Implement ISOSecurityValidator Component
- [ ] 3.1. Create `include/demuxer/iso/ISOSecurityValidator.h` with Limits structure
- [ ] 3.2. Create `src/demuxer/iso/ISOSecurityValidator.cpp`
- [ ] 3.3. Implement size validation (ValidateBoxSize, ValidateSampleSize, ValidateAllocation)
- [ ] 3.4. Implement recursion tracking (EnterBox, LeaveBox, GetCurrentDepth)
- [ ] 3.5. Implement count validation (ValidateTrackCount, ValidateSampleCount, ValidateArraySize)
- [ ] 3.6. Implement offset validation (ValidateOffset, ValidateRange)
- [ ] 3.7. Implement allocation tracking (TrackAllocation, ReleaseAllocation, CanAllocate)
- [ ] 3.8. Add configurable limits with sensible defaults
- _Requirements: 17.1-17.8_


## Phase 2: Container Parsing

### Task 4: Implement Core Data Structures
- [ ] 4.1. Create `include/demuxer/iso/ISODataStructures.h` with all data structures from design
- [ ] 4.2. Implement FileTypeInfo structure with HasBrand() and GetMIMEType() methods
- [ ] 4.3. Implement MovieHeaderInfo structure with GetDurationSeconds(), GetRate(), GetVolume()
- [ ] 4.4. Implement TrackHeaderInfo structure with flag accessors and dimension getters
- [ ] 4.5. Implement MediaHeaderInfo structure with GetLanguageCode() and GetDurationSeconds()
- [ ] 4.6. Implement HandlerInfo structure with type checking methods (IsAudio, IsVideo, etc.)
- [ ] 4.7. Implement SampleDescription structure for audio and video sample entries
- [ ] 4.8. Implement EditListEntry structure with IsEmptyEdit() and GetMediaRate()
- [ ] 4.9. Implement all sample table structures (TimeToSampleEntry, CompositionOffsetEntry, etc.)
- [ ] 4.10. Implement TrackInfo structure with all fields and convenience methods
- _Requirements: 1.2-1.7, 20.1-20.8_

### Task 5: Implement ISOContainerParser - File Level
- [ ] 5.1. Create `include/demuxer/iso/ISOContainerParser.h` with class interface
- [ ] 5.2. Create `src/demuxer/iso/ISOContainerParser.cpp`
- [ ] 5.3. Implement ParseFile() top-level parsing method
- [ ] 5.4. Implement ParseFileTypeBox() for ftyp parsing with brand extraction
- [ ] 5.5. Implement ParseSegmentTypeBox() for styp parsing
- [ ] 5.6. Implement IterateContainerBox() for recursive box iteration with depth limiting
- [ ] 5.7. Implement SkipBox() for unknown box handling
- [ ] 5.8. Add accessors (GetFileType, GetMovieHeader, GetTracks, IsFragmented)
- _Requirements: 1.1, 15.1-15.8, 18.1-18.2_

### Task 6: Implement ISOContainerParser - Movie Box
- [ ] 6.1. Implement ParseMovieBox() for moov container parsing
- [ ] 6.2. Implement ParseMovieHeaderBox() for mvhd with version 0/1 support
- [ ] 6.3. Implement ParseTrackBox() for trak container parsing
- [ ] 6.4. Implement ParseTrackHeaderBox() for tkhd with version 0/1 support
- [ ] 6.5. Implement ParseTrackReferenceBox() for tref parsing
- [ ] 6.6. Implement ParseTrackGroupBox() for track group parsing
- [ ] 6.7. Implement ParseEditBox() and ParseEditListBox() for edts/elst
- [ ] 6.8. Validate mandatory box presence (mvhd required in moov, tkhd required in trak)
- _Requirements: 18.3-18.6, 19.4-19.5, 24.1-24.8_

### Task 7: Implement ISOContainerParser - Media Box
- [ ] 7.1. Implement ParseMediaBox() for mdia container parsing
- [ ] 7.2. Implement ParseMediaHeaderBox() for mdhd with version 0/1 support
- [ ] 7.3. Implement ParseHandlerBox() for hdlr with handler type extraction
- [ ] 7.4. Implement ParseExtendedLanguageBox() for elng with BCP 47 tags
- [ ] 7.5. Implement ParseMediaInformationBox() for minf container
- [ ] 7.6. Implement media header boxes (ParseVideoMediaHeaderBox, ParseSoundMediaHeaderBox, etc.)
- [ ] 7.7. Implement ParseDataInformationBox() and ParseDataReferenceBox() for dinf/dref
- [ ] 7.8. Validate mandatory boxes (mdhd, hdlr required in mdia)
- _Requirements: 18.6-18.7, 22.1-22.8_

### Task 8: Implement ISOContainerParser - Sample Table
- [ ] 8.1. Implement ParseSampleTableBox() for stbl container
- [ ] 8.2. Implement ParseSampleDescriptionBox() for stsd with sample entry parsing
- [ ] 8.3. Implement ParseTimeToSampleBox() for stts
- [ ] 8.4. Implement ParseCompositionOffsetBox() for ctts with version 0/1 (signed/unsigned)
- [ ] 8.5. Implement ParseCompositionShiftBox() for cslg
- [ ] 8.6. Implement ParseSampleToChunkBox() for stsc
- [ ] 8.7. Implement ParseSampleSizeBox() for stsz (constant and per-sample)
- [ ] 8.8. Implement ParseCompactSampleSizeBox() for stz2
- [ ] 8.9. Implement ParseChunkOffsetBox() for stco (32-bit)
- [ ] 8.10. Implement ParseChunkOffset64Box() for co64 (64-bit)
- [ ] 8.11. Implement ParseSyncSampleBox() for stss
- [ ] 8.12. Implement ParseShadowSyncBox() for stsh
- [ ] 8.13. Implement ParseSampleDependencyTypeBox() for sdtp
- [ ] 8.14. Implement ParseDegradationPriorityBox() for stdp
- [ ] 8.15. Implement ParsePaddingBitsBox() for padb
- [ ] 8.16. Validate mandatory sample table boxes (stsd, stsz/stz2, stsc, stco/co64)
- _Requirements: 5.1-5.8, 18.7-18.8, 21.1-21.8_


### Task 9: Implement ISOContainerParser - Sample Groups and Sub-Samples
- [ ] 9.1. Implement ParseSampleToGroupBox() for sbgp
- [ ] 9.2. Implement ParseSampleGroupDescriptionBox() for sgpd
- [ ] 9.3. Implement ParseSubSampleInformationBox() for subs
- [ ] 9.4. Implement ParseSampleAuxInfoSizesBox() for saiz
- [ ] 9.5. Implement ParseSampleAuxInfoOffsetsBox() for saio
- _Requirements: 26.1-26.8, 28.1-28.8_

## Phase 3: Sample Table Management

### Task 10: Implement ISOSampleTableManager
- [ ] 10.1. Create `include/demuxer/iso/ISOSampleTableManager.h`
- [ ] 10.2. Create `src/demuxer/iso/ISOSampleTableManager.cpp`
- [ ] 10.3. Implement constructor that builds optimized index structures
- [ ] 10.4. Implement BuildChunkIndex() for efficient chunk lookups
- [ ] 10.5. Implement BuildTimeIndex() for time-to-sample mapping
- [ ] 10.6. Implement BuildSyncIndex() using std::set for O(1) sync sample lookup
- [ ] 10.7. Implement GetTotalSamples() and GetTotalDuration()
- [ ] 10.8. Add mutex for thread safety following public/private lock pattern
- _Requirements: 5.1-5.8, 8.1-8.4_

### Task 11: Implement Sample Lookup Methods
- [ ] 11.1. Implement GetSampleOffset() using chunk index
- [ ] 11.2. Implement GetSampleSize() with constant and per-sample support
- [ ] 11.3. Implement GetSampleTime() using time index
- [ ] 11.4. Implement GetSampleCompositionTime() with ctts handling
- [ ] 11.5. Implement GetSampleDescriptionIndex() from stsc mapping
- [ ] 11.6. Implement GetSampleAtTime() with binary search
- [ ] 11.7. Implement GetSampleAtCompositionTime() for presentation time lookup
- _Requirements: 5.2-5.5, 20.6-20.7_

### Task 12: Implement Sync Sample and Chunk Operations
- [ ] 12.1. Implement IsSyncSample() using sync sample set
- [ ] 12.2. Implement GetNearestSyncSample() for keyframe seeking
- [ ] 12.3. Implement GetSyncSampleAtOrBefore() for time-based keyframe lookup
- [ ] 12.4. Implement GetChunkForSample() using chunk index
- [ ] 12.5. Implement GetChunkOffset() for file positioning
- [ ] 12.6. Implement GetFirstSampleInChunk() and GetSamplesInChunk()
- [ ] 12.7. Implement GetSampleDependency() for sdtp data
- [ ] 12.8. Implement batch GetSampleInfo() and GetSampleRange() for efficiency
- _Requirements: 5.6, 26.1-26.8_

## Phase 4: Seeking Engine

### Task 13: Implement ISOSeekingEngine
- [ ] 13.1. Create `include/demuxer/iso/ISOSeekingEngine.h` with SeekResult structure
- [ ] 13.2. Create `src/demuxer/iso/ISOSeekingEngine.cpp`
- [ ] 13.3. Implement ProcessEditList() to build processed edit segments
- [ ] 13.4. Implement FindEditForPresentationTime() for edit segment lookup
- [ ] 13.5. Implement SeekToPresentationTime() with edit list application
- [ ] 13.6. Implement SeekToMediaTime() for direct media time seeking
- [ ] 13.7. Implement SeekToSample() for sample-based seeking
- [ ] 13.8. Implement SeekToKeyframe() for keyframe-aware seeking
- [ ] 13.9. Add mutex for thread safety following public/private lock pattern
- _Requirements: 5.6-5.7, 24.1-24.8_

### Task 14: Implement Time Conversion Methods
- [ ] 14.1. Implement HasEditList() check
- [ ] 14.2. Implement GetPresentationDuration() accounting for edit list
- [ ] 14.3. Implement PresentationToMediaTime() for timeline mapping
- [ ] 14.4. Implement MediaToPresentationTime() for reverse mapping
- [ ] 14.5. Implement MediaTimeToSeconds() and SecondsToMediaTime() conversions
- [ ] 14.6. Implement GetCompositionTime() and GetDecodeTime() for sample timing
- [ ] 14.7. Implement GetCompositionOffset() for ctts handling
- [ ] 14.8. Implement GetEditSegments() for edit list inspection
- _Requirements: 20.1-20.8, 24.4-24.8_


## Phase 5: Fragmented MP4 Support

### Task 15: Implement Fragment Data Structures
- [ ] 15.1. Add fragment structures to ISODataStructures.h (TrackExtendsDefaults, TrackFragmentHeader, etc.)
- [ ] 15.2. Implement TrackRun structure with flag accessors
- [ ] 15.3. Implement TrackFragment and MovieFragment structures
- [ ] 15.4. Implement TrackFragmentRandomAccess and TrackFragmentRandomAccessEntry
- [ ] 15.5. Implement SegmentIndex and SegmentIndexReference structures
- [ ] 15.6. Implement SubsegmentIndex structures
- [ ] 15.7. Implement ProducerReferenceTime structure for live streams
- _Requirements: 3.1-3.8_

### Task 16: Implement ISOContainerParser - Fragment Parsing
- [ ] 16.1. Implement ParseMovieExtendsBox() for mvex container
- [ ] 16.2. Implement ParseMovieExtendsHeaderBox() for mehd
- [ ] 16.3. Implement ParseTrackExtendsBox() for trex with default values
- [ ] 16.4. Implement ParseMovieFragmentBox() for moof container
- [ ] 16.5. Implement ParseMovieFragmentHeaderBox() for mfhd with sequence number
- [ ] 16.6. Implement ParseTrackFragmentBox() for traf container
- [ ] 16.7. Implement ParseTrackFragmentHeaderBox() for tfhd with all optional fields
- [ ] 16.8. Implement ParseTrackRunBox() for trun with sample data
- [ ] 16.9. Implement ParseTrackFragmentDecodeTimeBox() for tfdt
- _Requirements: 3.1-3.5_

### Task 17: Implement ISOContainerParser - Random Access and Segment Index
- [ ] 17.1. Implement ParseMovieFragmentRandomAccessBox() for mfra container
- [ ] 17.2. Implement ParseTrackFragmentRandomAccessBox() for tfra
- [ ] 17.3. Implement ParseMovieFragmentRandomAccessOffsetBox() for mfro
- [ ] 17.4. Implement ParseSegmentIndexBox() for sidx
- [ ] 17.5. Implement ParseSubsegmentIndexBox() for ssix
- [ ] 17.6. Implement ParseProducerReferenceTimeBox() for prft
- _Requirements: 3.4-3.5, 36.1-36.8, 51.1-51.8_

### Task 18: Implement ISOFragmentManager
- [ ] 18.1. Create `include/demuxer/iso/ISOFragmentManager.h`
- [ ] 18.2. Create `src/demuxer/iso/ISOFragmentManager.cpp`
- [ ] 18.3. Implement ScanForFragments() to discover all fragments in file
- [ ] 18.4. Implement BuildFragmentIndex() for per-track fragment indexing
- [ ] 18.5. Implement GetFragment() accessors (by index, time, offset)
- [ ] 18.6. Implement GetRandomAccessPoints() and GetNearestRandomAccessPoint()
- [ ] 18.7. Implement segment index accessors (HasSegmentIndex, GetSegmentIndex, GetSegmentCount)
- [ ] 18.8. Add mutex for thread safety following public/private lock pattern
- _Requirements: 3.1-3.8_

### Task 19: Implement Fragment Sample Access
- [ ] 19.1. Implement GetFragmentSample() for sample lookup within fragments
- [ ] 19.2. Implement GetFragmentSampleRange() for batch sample access
- [ ] 19.3. Implement GetDefaultSampleDuration() with trex/tfhd fallback
- [ ] 19.4. Implement GetDefaultSampleSize() with trex/tfhd fallback
- [ ] 19.5. Implement GetDefaultSampleFlags() with trex/tfhd fallback
- [ ] 19.6. Implement ProcessTrackFragment() for sample offset calculation
- [ ] 19.7. Implement IsLiveStream() and RefreshFragments() for live stream support
- _Requirements: 3.6-3.8_

## Phase 6: Metadata Extraction

### Task 20: Implement ISOMetadataExtractor
- [ ] 20.1. Create `include/demuxer/iso/ISOMetadataExtractor.h` with iTunesMetadata structure
- [ ] 20.2. Create `src/demuxer/iso/ISOMetadataExtractor.cpp`
- [ ] 20.3. Implement ParseMetadata() for meta box parsing
- [ ] 20.4. Implement ParseiTunesListBox() for ilst parsing
- [ ] 20.5. Implement ParseDataBox() for text data extraction
- [ ] 20.6. Implement ParseBinaryDataBox() for artwork and binary data
- [ ] 20.7. Implement GetMetadata() accessor
- [ ] 20.8. Add mutex for thread safety following public/private lock pattern
- _Requirements: 4.1-4.8_

### Task 21: Implement Item-Based Metadata
- [ ] 21.1. Implement ParseItemLocationBox() for iloc
- [ ] 21.2. Implement ParseItemInfoBox() for iinf/infe
- [ ] 21.3. Implement ParseItemReferenceBox() for iref
- [ ] 21.4. Implement ParsePrimaryItemBox() for pitm
- [ ] 21.5. Implement HasItemMetadata() and GetItemInfo()
- [ ] 21.6. Implement GetItemData() for item content retrieval
- [ ] 21.7. Implement GetItemReferences() for item relationships
- [ ] 21.8. Implement ParseUserData() for udta box
- _Requirements: 39.1-39.8, 46.1-46.8_


## Phase 7: Codec Configuration

### Task 22: Implement ISOCodecConfigExtractor
- [ ] 22.1. Create `include/demuxer/iso/ISOCodecConfigExtractor.h` with codec config structures
- [ ] 22.2. Create `src/demuxer/iso/ISOCodecConfigExtractor.cpp`
- [ ] 22.3. Implement ParseESDescriptor() for esds box parsing
- [ ] 22.4. Implement ParseAACConfig() and ParseAudioSpecificConfig() for AAC
- [ ] 22.5. Implement ParseALACConfig() for ALAC magic cookie
- [ ] 22.6. Implement ParseAVCConfig() for avcC with SPS/PPS extraction
- [ ] 22.7. Implement ParseFLACConfig() for dfLa configuration
- [ ] 22.8. Implement ParseChannelLayout() for chnl box
- [ ] 22.9. Add mutex for thread safety following public/private lock pattern
- _Requirements: 2.1-2.8, 14.1-14.8_

### Task 23: Implement Sample Entry Parsing
- [ ] 23.1. Implement ParseAudioSampleEntry() for audio sample descriptions
- [ ] 23.2. Implement ParseVideoSampleEntry() for video sample descriptions
- [ ] 23.3. Handle AAC sample entry with esds extraction
- [ ] 23.4. Handle ALAC sample entry with alac box extraction
- [ ] 23.5. Handle mulaw/alaw sample entries for telephony audio
- [ ] 23.6. Handle PCM sample entries with bit depth and endianness
- [ ] 23.7. Handle AVC sample entry with avcC extraction
- [ ] 23.8. Handle FLAC sample entry with dfLa extraction
- _Requirements: 2.1-2.8, 11.1-11.8, 14.1-14.8_

## Phase 8: Compliance Validation

### Task 24: Implement ISOComplianceValidator
- [ ] 24.1. Create `include/demuxer/iso/ISOComplianceValidator.h` with ValidationIssue structure
- [ ] 24.2. Create `src/demuxer/iso/ISOComplianceValidator.cpp`
- [ ] 24.3. Implement ValidateFileStructure() for ftyp and moov presence
- [ ] 24.4. Implement ValidateMovieBox() for mvhd and track validation
- [ ] 24.5. Implement ValidateTrack() for track structure compliance
- [ ] 24.6. Implement ValidateSampleTable() for sample table consistency
- [ ] 24.7. Implement ValidateFragment() for fragment structure compliance
- [ ] 24.8. Implement ValidateBoxHierarchy() and ValidateBoxCount()
- [ ] 24.9. Implement GetIssues() and GetReport() for validation results
- [ ] 24.10. Support Strict, Permissive, and ReportOnly modes
- _Requirements: 12.1-12.8, 18.1-18.8_

## Phase 9: Main Demuxer Integration

### Task 25: Implement ISODemuxer - Core Interface
- [ ] 25.1. Create `include/demuxer/iso/ISODemuxer.h` with Demuxer interface
- [ ] 25.2. Create `src/demuxer/iso/ISODemuxer.cpp`
- [ ] 25.3. Implement constructor initializing all components
- [ ] 25.4. Implement Open() with file parsing and component initialization
- [ ] 25.5. Implement Close() with proper cleanup
- [ ] 25.6. Implement IsOpen() state check
- [ ] 25.7. Implement static CanHandle() for format detection
- [ ] 25.8. Implement static GetFormatName(), GetSupportedExtensions(), GetSupportedMimeTypes()
- [ ] 25.9. Add mutex for thread safety following public/private lock pattern
- _Requirements: 10.1-10.3, 13.1-13.8, 16.1-16.8_

### Task 26: Implement ISODemuxer - Stream Information
- [ ] 26.1. Implement BuildStreamInfo() to create StreamInfo from tracks
- [ ] 26.2. Implement SelectPrimaryStreams() for audio/video selection
- [ ] 26.3. Implement CreateStreamInfo() for individual track conversion
- [ ] 26.4. Implement GetStreamCount() and GetStreamInfo()
- [ ] 26.5. Implement GetPrimaryAudioStream() and GetPrimaryVideoStream()
- [ ] 26.6. Implement GetFileType() and IsFragmented() accessors
- [ ] 26.7. Implement GetTracks() for direct track access
- _Requirements: 10.3-10.4_

### Task 27: Implement ISODemuxer - Sample Reading
- [ ] 27.1. Implement ReadSample() for sequential sample reading
- [ ] 27.2. Implement ReadSampleAt() for random access sample reading
- [ ] 27.3. Implement ReadSampleFromTrack() using sample table manager
- [ ] 27.4. Implement ReadSampleFromFragment() for fragmented files
- [ ] 27.5. Handle track state management (currentSample, currentTime, endOfTrack)
- [ ] 27.6. Create MediaChunk with proper timing and codec information
- _Requirements: 10.4, 5.4-5.5_

### Task 28: Implement ISODemuxer - Seeking
- [ ] 28.1. Implement Seek() using seeking engine
- [ ] 28.2. Implement SeekToSample() for sample-based seeking
- [ ] 28.3. Implement GetPosition() returning current playback position
- [ ] 28.4. Implement GetDuration() returning total duration
- [ ] 28.5. Handle seeking in fragmented files
- [ ] 28.6. Update track states after seeking
- _Requirements: 5.6-5.7, 10.5_

### Task 29: Implement ISODemuxer - Metadata
- [ ] 29.1. Implement GetMetadata() returning extracted metadata
- [ ] 29.2. Populate Metadata structure from iTunesMetadata
- [ ] 29.3. Handle artwork extraction and storage
- [ ] 29.4. Support custom metadata tags
- _Requirements: 4.1-4.8, 10.6_


## Phase 10: Error Handling

### Task 30: Implement Error Handling Infrastructure
- [ ] 30.1. Create `include/demuxer/iso/ISOError.h` with ISOError enum
- [ ] 30.2. Implement ISOException class with error, message, and offset
- [ ] 30.3. Define error categories (file structure, sample table, codec, fragment, seeking, security, I/O)
- [ ] 30.4. Implement error recovery strategies in each component
- [ ] 30.5. Add box-level recovery (skip corrupted boxes)
- [ ] 30.6. Add track-level recovery (mark damaged tracks unavailable)
- [ ] 30.7. Add sample-level recovery (skip unreadable samples)
- [ ] 30.8. Add fragment-level recovery (skip incomplete fragments)
- _Requirements: 7.1-7.8_

## Phase 11: Streaming Support

### Task 31: Implement Progressive Download Support
- [ ] 31.1. Handle movie box at file end scenario
- [ ] 31.2. Implement byte range request handling
- [ ] 31.3. Create buffering logic for unavailable samples
- [ ] 31.4. Handle network interruption recovery
- [ ] 31.5. Implement seeking in incomplete files
- [ ] 31.6. Handle fast-start files (moov before mdat)
- [ ] 31.7. Minimize redundant data requests
- _Requirements: 6.1-6.8_

## Phase 12: Update Master Header and Registration

### Task 32: Update psymp3.h Master Header
- [ ] 32.1. Add includes for all ISO demuxer headers in `include/demuxer/iso/`
- [ ] 32.2. Add `using` declarations for backward compatibility
- [ ] 32.3. Ensure conditional compilation guards for ISO demuxer
- [ ] 32.4. Verify all components are accessible from global namespace where needed
- _Requirements: Project header inclusion policy_

### Task 33: Register with Demuxer Factory
- [ ] 33.1. Register ISODemuxer with DemuxerRegistry
- [ ] 33.2. Add format detection priority
- [ ] 33.3. Register supported extensions (.mp4, .m4a, .m4v, .mov, .3gp, .f4a)
- [ ] 33.4. Register supported MIME types (video/mp4, audio/mp4, application/mp4)
- [ ] 33.5. Verify integration with MediaFactory
- _Requirements: 10.7, 13.1-13.8, 16.1-16.8_

## Phase 13: Testing

### Task 34: Unit Tests - Box Parsing
- [ ] 34.1. Create `tests/test_iso_box_reader.cpp`
- [ ] 34.2. Test 32-bit box header parsing
- [ ] 34.3. Test 64-bit extended size parsing (size==1)
- [ ] 34.4. Test box extending to EOF (size==0)
- [ ] 34.5. Test FullBox version and flags parsing
- [ ] 34.6. Test UUID box parsing
- [ ] 34.7. Test primitive reading methods
- [ ] 34.8. Test fixed-point reading
- [ ] 34.9. Test box bounds validation
- _Requirements: 19.1-19.8_

### Task 35: Unit Tests - Container Parsing
- [ ] 35.1. Create `tests/test_iso_container_parser.cpp`
- [ ] 35.2. Test ftyp parsing with various brands
- [ ] 35.3. Test mvhd parsing (version 0 and 1)
- [ ] 35.4. Test tkhd parsing (version 0 and 1)
- [ ] 35.5. Test mdhd parsing with language codes
- [ ] 35.6. Test hdlr parsing for audio/video tracks
- [ ] 35.7. Test edit list parsing
- [ ] 35.8. Test sample table parsing
- [ ] 35.9. Test mandatory box validation
- _Requirements: 1.1-1.8, 18.1-18.8_

### Task 36: Unit Tests - Sample Table Manager
- [ ] 36.1. Create `tests/test_iso_sample_table.cpp`
- [ ] 36.2. Test sample offset calculation
- [ ] 36.3. Test sample size lookup (constant and variable)
- [ ] 36.4. Test time-to-sample mapping
- [ ] 36.5. Test composition time handling
- [ ] 36.6. Test sync sample lookup
- [ ] 36.7. Test chunk operations
- [ ] 36.8. Test batch sample info retrieval
- _Requirements: 5.1-5.8, 21.1-21.8_

### Task 37: Unit Tests - Seeking Engine
- [ ] 37.1. Create `tests/test_iso_seeking.cpp`
- [ ] 37.2. Test seeking without edit list
- [ ] 37.3. Test seeking with simple edit list
- [ ] 37.4. Test seeking with empty edits
- [ ] 37.5. Test seeking with rate modifications
- [ ] 37.6. Test keyframe seeking
- [ ] 37.7. Test time conversion methods
- [ ] 37.8. Test presentation/media time mapping
- _Requirements: 24.1-24.8_

### Task 38: Unit Tests - Fragment Manager
- [ ] 38.1. Create `tests/test_iso_fragment.cpp`
- [ ] 38.2. Test fragment discovery
- [ ] 38.3. Test fragment sample access
- [ ] 38.4. Test default value resolution
- [ ] 38.5. Test random access point lookup
- [ ] 38.6. Test segment index handling
- [ ] 38.7. Test live stream refresh
- _Requirements: 3.1-3.8_

### Task 39: Unit Tests - Security and Compliance
- [ ] 39.1. Create `tests/test_iso_security.cpp`
- [ ] 39.2. Test size validation limits
- [ ] 39.3. Test recursion depth limiting
- [ ] 39.4. Test allocation tracking
- [ ] 39.5. Test offset validation
- [ ] 39.6. Create `tests/test_iso_compliance.cpp`
- [ ] 39.7. Test mandatory box validation
- [ ] 39.8. Test box hierarchy validation
- [ ] 39.9. Test validation modes (Strict, Permissive, ReportOnly)
- _Requirements: 17.1-17.8, 18.1-18.8_


### Task 40: Integration Tests
- [ ] 40.1. Create `tests/test_iso_integration.cpp`
- [ ] 40.2. Test with real MP4 files (AAC audio)
- [ ] 40.3. Test with M4A files (ALAC audio)
- [ ] 40.4. Test with MOV files (QuickTime)
- [ ] 40.5. Test with 3GP files (mobile format)
- [ ] 40.6. Test with fragmented MP4 files
- [ ] 40.7. Test with files containing edit lists
- [ ] 40.8. Test with files >4GB (64-bit offsets)
- [ ] 40.9. Test with telephony audio (mulaw/alaw)
- [ ] 40.10. Test with FLAC-in-MP4
- [ ] 40.11. Test with AVC/H.264 video
- [ ] 40.12. Test seeking accuracy across codecs
- [ ] 40.13. Test metadata extraction
- [ ] 40.14. Test error recovery with corrupted files
- _Requirements: All requirements validation_

### Task 41: Performance Tests
- [ ] 41.1. Create `tests/test_iso_performance.cpp`
- [ ] 41.2. Test parsing performance with large files
- [ ] 41.3. Test seeking performance
- [ ] 41.4. Test memory usage with large sample tables
- [ ] 41.5. Test lazy loading effectiveness
- [ ] 41.6. Benchmark sample lookup operations
- _Requirements: 8.1-8.8_

## Phase 14: Migration and Cleanup

### Task 42: Migrate Existing Code
- [ ] 42.1. Identify reusable code from existing ISODemuxer implementation
- [ ] 42.2. Migrate box parsing logic to ISOBoxReader
- [ ] 42.3. Migrate sample table logic to ISOSampleTableManager
- [ ] 42.4. Migrate seeking logic to ISOSeekingEngine
- [ ] 42.5. Migrate fragment handling to ISOFragmentManager
- [ ] 42.6. Migrate metadata extraction to ISOMetadataExtractor
- [ ] 42.7. Migrate codec config extraction to ISOCodecConfigExtractor
- [ ] 42.8. Update all callers to use new component interfaces
- _Requirements: Backward compatibility_

### Task 43: Remove Old Implementation
- [ ] 43.1. Remove old ISODemuxer files from `src/` after migration complete
- [ ] 43.2. Remove old header files from `include/`
- [ ] 43.3. Update Makefile.am to remove old source files
- [ ] 43.4. Update psymp3.h to remove old includes
- [ ] 43.5. Verify clean build after removal
- [ ] 43.6. Run full test suite to verify no regressions
- _Requirements: Clean codebase_

### Task 44: Documentation
- [ ] 44.1. Update README.md with ISO demuxer capabilities
- [ ] 44.2. Document supported formats and codecs
- [ ] 44.3. Document streaming and fragmented MP4 support
- [ ] 44.4. Add usage examples
- [ ] 44.5. Document error handling and recovery
- _Requirements: Project documentation standards_

## Summary

### Phase Dependencies

```
Phase 1 (Infrastructure) ──┬──> Phase 2 (Container Parsing)
                           │
                           └──> Phase 3 (Sample Table) ──> Phase 4 (Seeking)
                                        │
                                        └──> Phase 5 (Fragments)

Phase 2 ──> Phase 6 (Metadata)
Phase 2 ──> Phase 7 (Codec Config)
Phase 2 ──> Phase 8 (Compliance)

Phases 3-8 ──> Phase 9 (Main Demuxer)
Phase 9 ──> Phase 10 (Error Handling)
Phase 9 ──> Phase 11 (Streaming)
Phase 9 ──> Phase 12 (Registration)

Phase 12 ──> Phase 13 (Testing)
Phase 13 ──> Phase 14 (Migration)
```

### Component Count

| Component | Header | Source | Tests |
|-----------|--------|--------|-------|
| ISOBoxReader | 1 | 1 | 1 |
| ISOSecurityValidator | 1 | 1 | 1 |
| ISODataStructures | 1 | 0 | 0 |
| ISOContainerParser | 1 | 1 | 1 |
| ISOSampleTableManager | 1 | 1 | 1 |
| ISOSeekingEngine | 1 | 1 | 1 |
| ISOFragmentManager | 1 | 1 | 1 |
| ISOMetadataExtractor | 1 | 1 | 0 |
| ISOCodecConfigExtractor | 1 | 1 | 0 |
| ISOComplianceValidator | 1 | 1 | 1 |
| ISOError | 1 | 0 | 0 |
| ISODemuxer | 1 | 1 | 2 |
| **Total** | **12** | **10** | **9** |

### Requirements Coverage

All 70 requirements from requirements.md are covered by this implementation plan:
- Core parsing: Requirements 1, 18-19, 64-65
- Audio codecs: Requirements 2, 11, 14, 31-32
- Sample tables: Requirements 5, 21, 33, 42-45, 57
- Fragmented MP4: Requirements 3, 37-38, 58-63
- Metadata: Requirements 4, 39-40, 46-50
- Edit lists: Requirement 24
- Sample groups: Requirements 26, 28-29
- Protection: Requirements 17, 30, 48
- Segment indexing: Requirements 36, 51-52
- Streaming: Requirements 6
- Error handling: Requirements 7
- Performance: Requirements 8
- Format variants: Requirements 9, 13, 15-16
- Integration: Requirements 10, 12
- Timescale/duration: Requirements 20
- Data references: Requirements 22
- Track identification: Requirements 23, 25
