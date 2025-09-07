/*
 * ISODemuxer.cpp - ISO Base Media File Format demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

ISODemuxer::ISODemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)), selectedTrackIndex(-1), currentSampleIndex(0) {
    initializeComponents();
}

ISODemuxer::~ISODemuxer() {
    cleanup();
}

void ISODemuxer::initializeComponents() {
    // Initialize core components with shared IOHandler
    std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
        // Custom deleter that does nothing - we don't want to delete the handler
        // since it's owned by the unique_ptr in the base class
    });
    
    boxParser = std::make_unique<ISODemuxerBoxParser>(sharedHandler);
    sampleTables = std::make_unique<ISODemuxerSampleTableManager>();
    fragmentHandler = std::make_unique<ISODemuxerFragmentHandler>();
    metadataExtractor = std::make_unique<ISODemuxerMetadataExtractor>();
    streamManager = std::make_unique<ISODemuxerStreamManager>();
    seekingEngine = std::make_unique<ISODemuxerSeekingEngine>();
    streamingManager = std::make_unique<ISODemuxerStreamManager>();
    errorRecovery = std::make_unique<ISODemuxerErrorRecovery>(sharedHandler);
}

void ISODemuxer::cleanup() {
    // Components will be automatically cleaned up by unique_ptr destructors
    audioTracks.clear();
    selectedTrackIndex = -1;
    currentSampleIndex = 0;
}

bool ISODemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size with error handling
        off_t file_size = 0;
        bool fileSizeResult = performIOWithRetry([this, &file_size]() {
            m_handler->seek(0, SEEK_END);
            file_size = m_handler->tell();
            m_handler->seek(0, SEEK_SET);
            return file_size > 0;
        }, "Getting file size");
        
        if (!fileSizeResult) {
            reportError("FileAccess", "Failed to determine file size");
            return false;
        }
        
        if (file_size <= 0) {
            reportError("FileSize", "Invalid file size: " + std::to_string(file_size));
            return false;
        }
        
        // Check if this is a streaming source with movie box at end
        if (streamingManager->isStreaming()) {
            Debug::log("iso", "ISODemuxer: Detected streaming source, checking for progressive download");
            if (HandleProgressiveDownload()) {
                m_parsed = true;
                return !audioTracks.empty();
            }
        }
        
        // Parse top-level boxes to find ftyp, moov, and fragments
        std::string containerType;
        bool foundFileType = false;
        bool foundMovie = false;
        bool hasFragments = false;
        
        // Create shared IOHandler for fragment processing
        std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
            // Custom deleter that does nothing - we don't want to delete the handler
        });
        
        boxParser->ParseBoxRecursively(0, static_cast<uint64_t>(file_size), 
            [this, &containerType, &foundFileType, &foundMovie, &hasFragments, sharedHandler](const BoxHeader& header, uint64_t boxOffset) {
                switch (header.type) {
                    case BOX_FTYP:
                        // File type box - identify container variant
                        foundFileType = boxParser->ParseFileTypeBox(header.dataOffset, 
                                                                   header.size - (header.dataOffset - boxOffset), 
                                                                   containerType);
                        return foundFileType;
                    case BOX_MOOV:
                        // Movie box - extract track information
                        foundMovie = ParseMovieBoxWithTracks(header.dataOffset, 
                                                           header.size - (header.dataOffset - boxOffset));
                        return foundMovie;
                    case BOX_MOOF:
                        // Movie fragment box - process with FragmentHandler
                        if (fragmentHandler->ProcessMovieFragment(boxOffset, sharedHandler)) {
                            hasFragments = true;
                        }
                        return true;
                    case BOX_MFRA:
                        // Movie fragment random access box
                        boxParser->ParseFragmentBox(boxOffset, header.size);
                        return true;
                    case BOX_SIDX:
                        // Segment index box
                        boxParser->ParseFragmentBox(boxOffset, header.size);
                        return true;
                    case BOX_MDAT:
                        // Media data box - skip for now
                        return true;
                    case BOX_FREE:
                    case BOX_SKIP:
                        // Free space - skip
                        return true;
                    default:
                        return boxParser->SkipUnknownBox(header);
                }
            });
        
        if (!foundFileType) {
            // No ftyp box found, assume generic MP4
            containerType = "MP4";
        }
        
        if (!foundMovie) {
            return false;
        }
        
        // Handle fragmented MP4 files
        if (hasFragments && fragmentHandler->IsFragmented()) {
            // Set default values from movie header for missing fragment headers
            if (!audioTracks.empty()) {
                fragmentHandler->SetDefaultValues(audioTracks[0]);
            }
            
            // Update sample tables with fragment information
            for (auto& track : audioTracks) {
                // Get current fragment and update track sample tables
                MovieFragmentInfo* currentFragment = fragmentHandler->GetCurrentFragment();
                if (currentFragment) {
                    for (const auto& traf : currentFragment->trackFragments) {
                        if (traf.trackId == track.trackId) {
                            fragmentHandler->UpdateSampleTables(traf, track);
                            break;
                        }
                    }
                }
            }
        }
        
        // Validate telephony codec configurations before adding to stream manager
        for (const auto& track : audioTracks) {
            if (track.codecType == "ulaw" || track.codecType == "alaw") {
                if (!ValidateTelephonyCodecConfiguration(track)) {
                    // Log warning about non-compliant telephony configuration
                    // Continue processing but note the issue
                }
            }
        }
        
        // Validate and repair sample tables for all tracks
        for (auto& track : audioTracks) {
            if (!ValidateAndRepairSampleTables(track)) {
                reportError("SampleTableValidation", "Sample table validation failed for track " + 
                           std::to_string(track.trackId));
                // Continue with other tracks - graceful degradation
            }
            
            // Handle missing codec configuration
            std::vector<uint8_t> sampleData; // Would need to extract first sample for inference
            if (!HandleMissingCodecConfig(track, sampleData)) {
                reportError("CodecConfiguration", "Failed to resolve codec configuration for track " + 
                           std::to_string(track.trackId));
                // Continue with other tracks - graceful degradation
            }
        }
        
        // Add tracks to stream manager
        for (const auto& track : audioTracks) {
            try {
                streamManager->AddAudioTrack(track);
            } catch (const std::exception& e) {
                reportError("StreamManager", "Failed to add track " + std::to_string(track.trackId) + 
                           " to stream manager");
                // Continue with other tracks
            }
        }
        
        // Calculate duration from audio tracks with special handling for telephony
        for (const auto& track : audioTracks) {
            uint64_t track_duration_ms;
            
            if (track.codecType == "ulaw" || track.codecType == "alaw") {
                // Use precise sample-based calculation for telephony codecs
                if (track.sampleRate > 0 && !track.sampleTableInfo.sampleTimes.empty()) {
                    size_t totalSamples = track.sampleTableInfo.sampleTimes.size();
                    track_duration_ms = (totalSamples * 1000ULL) / track.sampleRate;
                } else {
                    // Fallback to timescale calculation
                    track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
                }
            } else {
                // Standard calculation for other codecs
                track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
            }
            
            m_duration_ms = std::max(m_duration_ms, track_duration_ms);
        }
        
        m_parsed = true;
        
        // Final validation - ensure we have at least one valid track
        if (audioTracks.empty()) {
            reportError("NoAudioTracks", "No valid audio tracks found in container");
            return false;
        }
        
        return true;
        
    } catch (const std::bad_alloc& e) {
        // Handle memory allocation failures
        handleMemoryFailure(0, "parseContainer");
        return false;
    } catch (const std::exception& e) {
        // Handle other exceptions with detailed error reporting
        reportError("ParseException", "Exception during container parsing: " + std::string(e.what()));
        return false;
    } catch (...) {
        // Handle unknown exceptions
        reportError("UnknownException", "Unknown exception during container parsing");
        return false;
    }
}



std::vector<StreamInfo> ISODemuxer::getStreams() const {
    if (streamManager) {
        return streamManager->GetStreamInfos();
    }
    return std::vector<StreamInfo>();
}

StreamInfo ISODemuxer::getStreamInfo(uint32_t stream_id) const {
    auto streams = getStreams();
    auto it = std::find_if(streams.begin(), streams.end(),
                          [stream_id](const StreamInfo& info) {
                              return info.stream_id == stream_id;
                          });
    
    if (it != streams.end()) {
        return *it;
    }
    
    return StreamInfo{};
}

MediaChunk ISODemuxer::readChunk() {
    // Find the first available audio track if none selected
    if (selectedTrackIndex == -1 && !audioTracks.empty()) {
        selectedTrackIndex = 0;
    }
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(audioTracks.size())) {
        return readChunk(audioTracks[selectedTrackIndex].trackId);
    }
    
    return MediaChunk{};
}

MediaChunk ISODemuxer::readChunk(uint32_t stream_id) {
    if (!streamManager) {
        reportError("ReadChunk", "StreamManager not initialized");
        return MediaChunk{};
    }
    
    AudioTrackInfo* track = streamManager->GetTrack(stream_id);
    if (!track) {
        reportError("ReadChunk", "Track not found: " + std::to_string(stream_id));
        return MediaChunk{};
    }
    
    // Check if this is a fragmented file
    if (fragmentHandler && fragmentHandler->IsFragmented()) {
        // Extract sample from current fragment
        uint64_t sampleOffset;
        uint32_t sampleSize;
        
        if (fragmentHandler->ExtractFragmentSample(stream_id, track->currentSampleIndex, 
                                                  sampleOffset, sampleSize)) {
            // Read sample data from fragment with error handling
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            
            // Validate sample size
            if (sampleSize == 0) {
                reportError("FragmentSample", "Zero-sized sample in fragment");
                setEOF(true);
                return MediaChunk{};
            }
            
            // Handle memory allocation with error recovery
            try {
                chunk.data.resize(sampleSize);
            } catch (const std::bad_alloc& e) {
                if (!handleMemoryFailure(sampleSize, "fragment sample data")) {
                    reportError("MemoryAllocation", "Failed to allocate " + std::to_string(sampleSize) + 
                               " bytes for fragment sample");
                    return MediaChunk{};
                }
                // Try again with smaller allocation if recovery was attempted
                return MediaChunk{};
            }
            
            // Perform I/O with retry mechanism
            bool readSuccess = performIOWithRetry([this, sampleOffset, sampleSize, &chunk]() {
                return (m_handler->seek(static_cast<long>(sampleOffset), SEEK_SET) == 0 &&
                        m_handler->read(chunk.data.data(), 1, sampleSize) == sampleSize);
            }, "reading fragment sample data");
            
            if (readSuccess) {
                // Set timing information
                uint64_t timestamp_ms = CalculateTelephonyTiming(*track, track->currentSampleIndex);
                chunk.timestamp_samples = (timestamp_ms * track->sampleRate) / 1000; // Convert to samples
                
                // Apply codec-specific processing
                ProcessCodecSpecificData(chunk, *track);
                
                // Update track position
                track->currentSampleIndex++;
                updatePosition(timestamp_ms);
                
                return chunk;
            } else {
                reportError("FragmentRead", "Failed to read fragment sample data");
            }
        } else {
            reportError("FragmentExtraction", "Failed to extract sample from fragment");
        }
        
        // Fragment sample extraction failed
        setEOF(true);
        return MediaChunk{};
    }
    
    // Use SampleTableManager for non-fragmented files
    if (!sampleTables) {
        reportError("ReadChunk", "SampleTableManager not initialized");
        return MediaChunk{};
    }
    
    // Get sample information for current position with error handling
    ISODemuxerSampleTableManager::SampleInfo sampleInfo;
    try {
        sampleInfo = sampleTables->GetSampleInfo(track->currentSampleIndex);
    } catch (const std::exception& e) {
        reportError("SampleInfo", "Failed to get sample info for index " + 
                   std::to_string(track->currentSampleIndex) + ": " + e.what());
        setEOF(true);
        return MediaChunk{};
    }
    
    if (sampleInfo.size == 0) {
        // Check if this is truly EOF or a corrupted sample table
        if (track->currentSampleIndex == 0) {
            reportError("SampleTable", "First sample has zero size - possible corruption");
        }
        setEOF(true);
        return MediaChunk{};
    }
    
    // For streaming sources, ensure sample data is available
    if (streamingManager->isStreaming()) {
        // Check if data is available or wait for it
        if (!EnsureSampleDataAvailable(sampleInfo.offset, sampleInfo.size)) {
            // Data not available and couldn't be fetched
            return MediaChunk{};
        }
        
        // Prefetch upcoming samples for smoother playback
        PrefetchUpcomingSamples(track->currentSampleIndex, *track);
    }
    
    // Extract sample data from mdat box using sample tables with error handling
    MediaChunk chunk;
    try {
        chunk = ExtractSampleData(stream_id, *track, sampleInfo);
    } catch (const std::exception& e) {
        reportError("SampleExtraction", "Failed to extract sample data: " + std::string(e.what()));
        setEOF(true);
        return MediaChunk{};
    }
    
    if (chunk.data.empty()) {
        reportError("EmptySample", "Extracted sample data is empty for track " + 
                   std::to_string(stream_id) + " sample " + std::to_string(track->currentSampleIndex));
        setEOF(true);
        return MediaChunk{};
    }
    
    // Update track position
    track->currentSampleIndex++;
    
    // Update global position based on track timing
    double timestamp = sampleTables->SampleToTime(track->currentSampleIndex);
    updatePosition(static_cast<uint64_t>(timestamp * 1000.0));
    
    return chunk;
}

bool ISODemuxer::seekTo(uint64_t timestamp_ms) {
    // If no track is selected, select the first audio track
    if (selectedTrackIndex == -1 && !audioTracks.empty()) {
        selectedTrackIndex = 0;
    }
    
    if (selectedTrackIndex >= static_cast<int>(audioTracks.size())) {
        return false;
    }
    
    AudioTrackInfo& track = audioTracks[selectedTrackIndex];
    
    // Validate seek position against track duration
    uint64_t trackDurationMs = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
    if (timestamp_ms > trackDurationMs && trackDurationMs > 0) {
        // Clamp to track duration
        timestamp_ms = trackDurationMs;
    }
    
    // Handle fragmented files
    if (fragmentHandler && fragmentHandler->IsFragmented()) {
        // For fragmented files, we need to find the appropriate fragment
        // and seek within that fragment
        
        // Get all available fragments
        uint32_t fragmentCount = fragmentHandler->GetFragmentCount();
        if (fragmentCount == 0) {
            return false;
        }
        
        // Calculate target time in track timescale
        uint64_t targetTime = 0;
        if (track.timescale > 0) {
            targetTime = (timestamp_ms * track.timescale) / 1000;
        }
        
        // Find the fragment containing this timestamp
        // Start with the first fragment and iterate until we find the right one
        bool foundFragment = false;
        for (uint32_t i = 1; i <= fragmentCount; i++) {
            MovieFragmentInfo* fragment = fragmentHandler->GetFragment(i);
            if (!fragment) {
                continue;
            }
            
            // Find the track fragment for this track
            for (const auto& traf : fragment->trackFragments) {
                if (traf.trackId == track.trackId) {
                    // Check if this fragment contains our target time
                    // We need to calculate the end time of this fragment
                    uint64_t fragmentStartTime = traf.tfdt;
                    uint64_t fragmentDuration = 0;
                    
                    // Calculate fragment duration by summing all sample durations
                    for (const auto& trun : traf.trackRuns) {
                        if (!trun.sampleDurations.empty()) {
                            // Use per-sample durations
                            for (uint32_t duration : trun.sampleDurations) {
                                fragmentDuration += duration;
                            }
                        } else {
                            // Use default duration
                            fragmentDuration += trun.sampleCount * traf.defaultSampleDuration;
                        }
                    }
                    
                    uint64_t fragmentEndTime = fragmentStartTime + fragmentDuration;
                    
                    // Check if target time is within this fragment
                    if (targetTime >= fragmentStartTime && targetTime < fragmentEndTime) {
                        // Found the right fragment
                        fragmentHandler->SeekToFragment(i);
                        foundFragment = true;
                        
                        // Calculate sample index within fragment
                        uint64_t timeOffset = targetTime - fragmentStartTime;
                        uint64_t sampleIndex = 0;
                        uint64_t accumulatedDuration = 0;
                        
                        // Find the sample containing our target time
                        for (const auto& trun : traf.trackRuns) {
                            for (uint32_t j = 0; j < trun.sampleCount; j++) {
                                uint32_t sampleDuration = 0;
                                
                                if (j < trun.sampleDurations.size()) {
                                    sampleDuration = trun.sampleDurations[j];
                                } else {
                                    sampleDuration = traf.defaultSampleDuration;
                                }
                                
                                if (accumulatedDuration + sampleDuration > timeOffset) {
                                    // Found the sample
                                    track.currentSampleIndex = sampleIndex;
                                    currentSampleIndex = sampleIndex;
                                    updatePosition(timestamp_ms);
                                    setEOF(false);
                                    return true;
                                }
                                
                                accumulatedDuration += sampleDuration;
                                sampleIndex++;
                            }
                        }
                        
                        // If we get here, use the first sample in the fragment
                        track.currentSampleIndex = 0;
                        currentSampleIndex = 0;
                        updatePosition((fragmentStartTime * 1000ULL) / track.timescale);
                        setEOF(false);
                        return true;
                    }
                }
            }
        }
        
        // If we couldn't find the exact fragment, try to seek to the closest one
        if (!foundFragment) {
            // Find the last fragment
            MovieFragmentInfo* lastFragment = fragmentHandler->GetFragment(fragmentCount);
            if (lastFragment) {
                fragmentHandler->SeekToFragment(fragmentCount);
                track.currentSampleIndex = 0;
                currentSampleIndex = 0;
                updatePosition(timestamp_ms);
                setEOF(false);
                return true;
            }
        }
        
        return false;
    }
    
    // Handle non-fragmented files
    if (!seekingEngine || !sampleTables) {
        return false;
    }
    
    double timestamp_seconds = static_cast<double>(timestamp_ms) / 1000.0;
    
    bool success = seekingEngine->SeekToTimestamp(timestamp_seconds, track, *sampleTables);
    
    if (success) {
        currentSampleIndex = track.currentSampleIndex;
        
        // Update position to actual seek position (may be different due to keyframe alignment)
        double actualTimestamp = sampleTables->SampleToTime(track.currentSampleIndex);
        updatePosition(static_cast<uint64_t>(actualTimestamp * 1000.0));
        
        setEOF(false);
        
        // Update all tracks to maintain synchronization (if multiple tracks exist)
        for (auto& otherTrack : audioTracks) {
            if (otherTrack.trackId != track.trackId) {
                // Seek other tracks to equivalent position
                seekingEngine->SeekToTimestamp(timestamp_seconds, otherTrack, *sampleTables);
            }
        }
    }
    
    return success;
}

bool ISODemuxer::isEOF() const {
    return isEOFAtomic();
}

uint64_t ISODemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t ISODemuxer::getPosition() const {
    return m_position_ms;
}

bool ISODemuxer::ParseMovieBoxWithTracks(uint64_t offset, uint64_t size) {
    bool success = boxParser->ParseBoxRecursively(offset, size, 
        [this](const BoxHeader& header, uint64_t boxOffset) {
            switch (header.type) {
                case BOX_MVHD:
                    // Movie header - contains global timescale and duration
                    return true; // Skip for now, will implement in later tasks
                case BOX_TRAK:
                    // Track box - parse for audio tracks
                    {
                        AudioTrackInfo track = {};
                        if (boxParser->ParseTrackBox(header.dataOffset, 
                                                   header.size - (header.dataOffset - boxOffset), 
                                                   track)) {
                            audioTracks.push_back(track);
                        }
                        return true;
                    }
                case BOX_UDTA:
                    // User data - extract metadata
                    {
                        std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
                            // Custom deleter that does nothing
                        });
                        auto extractedMetadata = metadataExtractor->ExtractMetadata(sharedHandler, 
                                                                                   header.dataOffset, 
                                                                                   header.size - (header.dataOffset - boxOffset));
                        // Merge extracted metadata with existing metadata
                        for (const auto& pair : extractedMetadata) {
                            m_metadata[pair.first] = pair.second;
                        }
                        return true;
                    }
                default:
                    return boxParser->SkipUnknownBox(header);
            }
        });
    
    // After parsing all tracks, build sample tables for the first audio track
    if (success && !audioTracks.empty()) {
        // Build sample tables for the first track (for now)
        const AudioTrackInfo& firstTrack = audioTracks[0];
        if (!firstTrack.sampleTableInfo.chunkOffsets.empty()) {
            if (!sampleTables->BuildSampleTables(firstTrack.sampleTableInfo)) {
                // Sample table validation failed
                return false;
            }
        }
    }
    
    return success;
}

MediaChunk ISODemuxer::ExtractSampleData(uint32_t stream_id, const AudioTrackInfo& track, 
                                         const ISODemuxerSampleTableManager::SampleInfo& sampleInfo) {
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.timestamp_samples = track.currentSampleIndex;
    chunk.is_keyframe = sampleInfo.isKeyframe;
    chunk.file_offset = sampleInfo.offset;
    
    // Calculate accurate timing for telephony codecs
    // Note: MediaChunk uses timestamp_samples, not timestamp_ms
    // The timing calculation is handled by the codec layer
    
    // Validate sample information
    if (sampleInfo.size == 0 || sampleInfo.offset == 0) {
        return chunk; // Return empty chunk
    }
    
    // Validate file offset is within bounds
    m_handler->seek(0, SEEK_END);
    off_t file_size = m_handler->tell();
    
    if (sampleInfo.offset >= static_cast<uint64_t>(file_size) || 
        sampleInfo.offset + sampleInfo.size > static_cast<uint64_t>(file_size)) {
        // Sample extends beyond file - corrupted or incomplete file
        return chunk;
    }
    
    // Allocate buffer for sample data
    chunk.data.resize(sampleInfo.size);
    
    // Seek to sample location in mdat box
    if (m_handler->seek(static_cast<long>(sampleInfo.offset), SEEK_SET) != 0) {
        chunk.data.clear();
        return chunk;
    }
    
    // Read sample data from file
    size_t bytes_read = m_handler->read(chunk.data.data(), 1, sampleInfo.size);
    
    if (bytes_read != sampleInfo.size) {
        // Partial read - resize buffer to actual data read
        chunk.data.resize(bytes_read);
        
        // If we read significantly less than expected, this might be an error
        if (bytes_read < sampleInfo.size / 2) {
            chunk.data.clear();
            return chunk;
        }
    }
    
    // Apply codec-specific processing if needed
    ProcessCodecSpecificData(chunk, track);
    
    return chunk;
}

void ISODemuxer::ProcessCodecSpecificData(MediaChunk& chunk, const AudioTrackInfo& track) {
    // Apply codec-specific processing based on track codec type
    if (chunk.data.empty()) {
        return;
    }
    
    if (track.codecType == "aac") {
        // AAC samples are typically already properly formatted
        // No additional processing needed for raw AAC frames
        
    } else if (track.codecType == "alac") {
        // ALAC samples are already properly formatted
        // No additional processing needed
        
    } else if (track.codecType == "ulaw" || track.codecType == "alaw") {
        // Telephony codecs - samples are raw companded 8-bit data
        // Ensure proper sample alignment and validate data integrity
        
        // Validate telephony codec parameters (read-only validation)
        if (track.bitsPerSample != 8) {
            // Log warning - telephony codecs should be 8-bit companded
            // Note: Cannot modify const track parameter
        }
        
        if (track.channelCount != 1) {
            // Log warning - telephony codecs should be mono
            // Note: Cannot modify const track parameter
        }
        
        if (track.sampleRate != 8000 && track.sampleRate != 16000 && 
            track.sampleRate != 11025 && track.sampleRate != 22050) {
            // Log warning - non-standard sample rate for telephony
            // Note: Cannot modify const track parameter
        }
        
        // For telephony codecs, samples are already in the correct raw format
        // Validate that sample data is properly aligned for 8-bit companded audio
        
        // Validate sample data size for telephony codecs
        if (track.sampleRate > 0 && track.timescale > 0) {
            // Each sample should be exactly 1 byte for companded audio
            // Basic sanity check - ensure we have reasonable amount of data
            size_t minExpectedSize = 1; // At least 1 byte
            size_t maxExpectedSize = track.sampleRate; // At most 1 second worth
            
            if (chunk.data.size() < minExpectedSize || chunk.data.size() > maxExpectedSize) {
                // Log warning but continue - some files may have unusual sample sizes
            }
        }
        
        // Each byte represents one companded audio sample - no further processing needed
        
    } else if (track.codecType == "pcm") {
        // PCM samples may need endianness correction or padding
        // For now, assume samples are correctly formatted in the container
        // Future enhancement: handle different PCM variants (little/big endian, etc.)
        
    }
    
    // For all codecs, ensure we have valid data
    if (chunk.data.empty()) {
        // If processing resulted in empty data, mark as invalid
        chunk.stream_id = 0;
    }
}

uint64_t ISODemuxer::CalculateTelephonyTiming(const AudioTrackInfo& track, uint64_t sampleIndex) {
    // For telephony codecs, calculate precise timing based on sample rate
    if (track.codecType == "ulaw" || track.codecType == "alaw") {
        // Each sample represents 1/sampleRate seconds
        // Convert to milliseconds: (sampleIndex * 1000) / sampleRate
        if (track.sampleRate > 0) {
            return (sampleIndex * 1000) / track.sampleRate;
        }
    }
    
    // Fallback to timescale-based calculation
    if (track.timescale > 0 && sampleIndex < track.sampleTableInfo.sampleTimes.size()) {
        uint64_t trackTime = track.sampleTableInfo.sampleTimes[sampleIndex];
        return (trackTime * 1000) / track.timescale;
    }
    
    return 0; // Unable to calculate timing
}

bool ISODemuxer::ValidateTelephonyCodecConfiguration(const AudioTrackInfo& track) {
    // Only validate telephony codecs
    if (track.codecType != "ulaw" && track.codecType != "alaw") {
        return true; // Not a telephony codec, validation not applicable
    }
    
    bool isValid = true;
    
    // Validate sample rate (telephony standards)
    if (track.sampleRate != 8000 && track.sampleRate != 16000 && 
        track.sampleRate != 11025 && track.sampleRate != 22050) {
        // Non-standard sample rate for telephony
        isValid = false;
    }
    
    // Validate channel configuration (must be mono for telephony)
    if (track.channelCount != 1) {
        // Telephony audio must be mono
        isValid = false;
    }
    
    // Validate bit depth (must be 8-bit for companded audio)
    if (track.bitsPerSample != 8) {
        // Companded audio must be 8-bit
        isValid = false;
    }
    
    // Validate codec-specific requirements
    if (track.codecType == "alaw") {
        // A-law specific validation (European standard ITU-T G.711)
        // Default sample rate should be 8kHz for European telephony
        if (track.sampleRate == 0) {
            isValid = false;
        }
    } else if (track.codecType == "ulaw") {
        // μ-law specific validation (North American/Japanese standard)
        // Default sample rate should be 8kHz for North American telephony
        if (track.sampleRate == 0) {
            isValid = false;
        }
    }
    
    // Validate that codec configuration is minimal (raw formats need no config)
    if (!track.codecConfig.empty()) {
        // Telephony codecs should have no additional configuration
        // This is acceptable but not required - some containers may include metadata
    }
    
    return isValid;
}



std::map<std::string, std::string> ISODemuxer::getMetadata() const {
    return m_metadata;
}



bool ISODemuxer::HandleProgressiveDownload() {
    Debug::log("iso", "ISODemuxer: Handling progressive download");
    
    // Check if movie box is at the end of the file
    if (!streamingManager->isMovieBoxAtEnd()) {
        // Try to find the movie box
        uint64_t moovOffset = streamingManager->findMovieBox();
        if (moovOffset == 0) {
            Debug::log("iso", "ISODemuxer: Movie box not found in progressive download");
            return false;
        }
        
        Debug::log("iso", "ISODemuxer: Found movie box at offset ", moovOffset);
    }
    
    // Get movie box offset
    uint64_t moovOffset = streamingManager->findMovieBox();
    if (moovOffset == 0) {
        Debug::log("iso", "ISODemuxer: Movie box not found");
        return false;
    }
    
    // Parse movie box to extract tracks
    if (!ParseMovieBoxWithTracks(moovOffset, 0)) {
        Debug::log("iso", "ISODemuxer: Failed to parse movie box");
        return false;
    }
    
    // Check if we found any audio tracks
    if (audioTracks.empty()) {
        Debug::log("iso", "ISODemuxer: No audio tracks found");
        return false;
    }
    
    // Select first audio track by default
    selectedTrackIndex = 0;
    
    // Build sample tables for selected track
    if (!sampleTables->BuildSampleTables(audioTracks[selectedTrackIndex].sampleTableInfo)) {
        Debug::log("iso", "ISODemuxer: Failed to build sample tables");
        return false;
    }
    
    // Add tracks to stream manager
    for (const auto& track : audioTracks) {
        streamManager->AddAudioTrack(track);
    }
    
    // Calculate duration from audio tracks
    for (const auto& track : audioTracks) {
        uint64_t track_duration_ms;
        
        if (track.codecType == "ulaw" || track.codecType == "alaw") {
            // Use precise sample-based calculation for telephony codecs
            if (track.sampleRate > 0 && !track.sampleTableInfo.sampleTimes.empty()) {
                size_t totalSamples = track.sampleTableInfo.sampleTimes.size();
                track_duration_ms = (totalSamples * 1000ULL) / track.sampleRate;
            } else {
                // Fallback to timescale calculation
                track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
            }
        } else {
            // Standard calculation for other codecs
            track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
        }
        
        m_duration_ms = std::max(m_duration_ms, track_duration_ms);
    }
    
    Debug::log("iso", "ISODemuxer: Progressive download handling successful");
    return true;
}

bool ISODemuxer::EnsureSampleDataAvailable(uint64_t offset, size_t size) {
    if (!streamingManager->isStreaming()) {
        return true; // Complete file is always available
    }
    
    Debug::log("iso", "ISODemuxer: Checking data availability at offset ", offset, " size ", size);
    
    // Check if data is available
    if (streamingManager->isDataAvailable(offset, size)) {
        return true;
    }
    
    // Request the data
    streamingManager->requestByteRange(offset, size);
    
    // Wait for data with timeout
    constexpr uint32_t DATA_WAIT_TIMEOUT_MS = 5000; // 5 seconds timeout
    bool dataAvailable = streamingManager->waitForData(offset, size, DATA_WAIT_TIMEOUT_MS);
    
    if (!dataAvailable) {
        Debug::log("iso", "ISODemuxer: Timeout waiting for data at offset ", offset);
    }
    
    return dataAvailable;
}

void ISODemuxer::PrefetchUpcomingSamples(uint64_t currentSample, const AudioTrackInfo& track) {
    if (!streamingManager->isStreaming()) {
        return; // No need to prefetch for complete files
    }
    
    // Prefetch the next few samples
    constexpr uint32_t PREFETCH_COUNT = 5;
    
    for (uint32_t i = 1; i <= PREFETCH_COUNT; i++) {
        uint64_t sampleIndex = currentSample + i;
        
        // Get sample information
        auto sampleInfo = sampleTables->GetSampleInfo(sampleIndex);
        if (sampleInfo.size == 0) {
            break; // End of track
        }
        
        // Request prefetch without waiting
        streamingManager->prefetchSample(sampleInfo.offset, sampleInfo.size);
    }
}

// Error handling method implementations
BoxHeader ISODemuxer::HandleCorruptedBox(const BoxHeader& header, uint64_t containerSize) {
    if (!errorRecovery) {
        return header; // No error recovery available
    }
    
    // Get file size for validation
    uint64_t fileSize = 0;
    if (m_handler) {
        m_handler->seek(0, SEEK_END);
        fileSize = static_cast<uint64_t>(m_handler->tell());
    }
    
    // Use ISODemuxerErrorRecovery to attempt box recovery
    BoxHeader recoveredHeader = errorRecovery->RecoverCorruptedBox(header, containerSize, fileSize);
    
    // Log the recovery attempt
    if (recoveredHeader.type != header.type || recoveredHeader.size != header.size) {
        reportError("BoxRecovery", "Recovered corrupted box", header.type);
    }
    
    return recoveredHeader;
}

bool ISODemuxer::ValidateAndRepairSampleTables(AudioTrackInfo& track) {
    if (!errorRecovery) {
        return true; // No error recovery available, assume valid
    }
    
    // Validate sample tables using ISODemuxerErrorRecovery
    bool repairResult = errorRecovery->RepairSampleTables(track.sampleTableInfo);
    
    if (!repairResult) {
        reportError("SampleTableValidation", "Failed to repair sample tables for track " + 
                   std::to_string(track.trackId));
        return false;
    }
    
    // Log successful repair if any changes were made
    Debug::log("iso", "Successfully validated/repaired sample tables for track ", track.trackId);
    
    return true;
}

bool ISODemuxer::HandleMissingCodecConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData) {
    if (!errorRecovery) {
        return !track.codecConfig.empty(); // Return true if config already exists
    }
    
    // Check if codec configuration is already present
    if (!track.codecConfig.empty()) {
        return true;
    }
    
    // Attempt to infer codec configuration from sample data
    bool inferenceResult = errorRecovery->InferCodecConfig(track, sampleData);
    
    if (inferenceResult) {
        Debug::log("iso", "Successfully inferred codec configuration for ", track.codecType, " track ", track.trackId);
    } else {
        reportError("CodecConfigMissing", "Failed to infer codec configuration for " + 
                   track.codecType + " track " + std::to_string(track.trackId));
    }
    
    return inferenceResult;
}

bool ISODemuxer::PerformIOWithRetry(std::function<bool()> operation, const std::string& errorContext) {
    // Use base class I/O retry mechanism
    return performIOWithRetry(operation, errorContext);
}

bool ISODemuxer::HandleMemoryAllocationFailure(size_t requestedSize, const std::string& context) {
    // Use base class memory failure handling
    return handleMemoryFailure(requestedSize, context);
}

void ISODemuxer::ReportError(const std::string& errorType, const std::string& message, uint32_t boxType) {
    // Use base class error reporting
    reportError(errorType, message, boxType);
    
    // Also log to error recovery component if available
    if (errorRecovery) {
        errorRecovery->LogError(errorType, message, boxType);
    }
    
    // Could also integrate with PsyMP3's error reporting system here
    // For now, errors are logged through ISODemuxerErrorRecovery
}