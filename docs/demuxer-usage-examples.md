# PsyMP3 Demuxer Architecture Usage Examples

## Table of Contents
- [Basic Usage Examples](#basic-usage-examples)
- [Advanced Integration Patterns](#advanced-integration-patterns)
- [Error Handling Examples](#error-handling-examples)
- [Performance Optimization Examples](#performance-optimization-examples)
- [Thread Safety Examples](#thread-safety-examples)
- [Plugin Development Examples](#plugin-development-examples)

## Basic Usage Examples

### Example 1: Simple Audio File Processing

```cpp
#include "psymp3.h"

void processAudioFile(const std::string& filename) {
    try {
        // Create IOHandler for the file
        auto handler = std::make_unique<FileIOHandler>(filename);
        
        // Create appropriate demuxer
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        if (!demuxer) {
            std::cerr << "Unsupported file format: " << filename << std::endl;
            return;
        }
        
        // Parse the container
        if (!demuxer->parseContainer()) {
            std::cerr << "Failed to parse container: " << demuxer->getLastError().message << std::endl;
            return;
        }
        
        // Display stream information
        auto streams = demuxer->getStreams();
        std::cout << "Found " << streams.size() << " streams in " << filename << std::endl;
        
        for (const auto& stream : streams) {
            if (stream.isAudio()) {
                std::cout << "Audio Stream " << stream.stream_id << ":" << std::endl;
                std::cout << "  Codec: " << stream.codec_name << std::endl;
                std::cout << "  Sample Rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "  Channels: " << stream.channels << std::endl;
                std::cout << "  Duration: " << stream.duration_ms / 1000.0 << " seconds" << std::endl;
                
                if (!stream.artist.empty()) {
                    std::cout << "  Artist: " << stream.artist << std::endl;
                }
                if (!stream.title.empty()) {
                    std::cout << "  Title: " << stream.title << std::endl;
                }
            }
        }
        
        // Process audio data
        size_t total_chunks = 0;
        size_t total_bytes = 0;
        
        while (!demuxer->isEOF()) {
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                total_chunks++;
                total_bytes += chunk.getDataSize();
                
                // Process the chunk data here
                // For example, pass to audio codec for decoding
                processAudioChunk(chunk);
            }
        }
        
        std::cout << "Processed " << total_chunks << " chunks (" 
                  << total_bytes << " bytes)" << std::endl;
                  
    } catch (const std::exception& e) {
        std::cerr << "Error processing file: " << e.what() << std::endl;
    }
}

void processAudioChunk(const MediaChunk& chunk) {
    // Example: Pass chunk to appropriate codec for decoding
    // This would typically involve:
    // 1. Selecting the right codec based on stream info
    // 2. Feeding the chunk data to the codec
    // 3. Getting decoded PCM samples
    // 4. Processing the PCM data (playback, analysis, etc.)
    
    std::cout << "Processing chunk from stream " << chunk.stream_id 
              << " (" << chunk.getDataSize() << " bytes)" << std::endl;
}
```

### Example 2: Using DemuxedStream for Legacy Compatibility

```cpp
#include "psymp3.h"

void playAudioWithLegacyInterface(const std::string& filename) {
    try {
        // Create DemuxedStream - automatically handles demuxing and decoding
        auto stream = std::make_unique<DemuxedStream>(TagLib::String(filename.c_str()));
        
        // Check if stream was created successfully
        if (stream->eof()) {
            std::cerr << "Failed to open audio file: " << filename << std::endl;
            return;
        }
        
        // Display available streams
        auto available_streams = stream->getAvailableStreams();
        std::cout << "Available streams:" << std::endl;
        for (const auto& stream_info : available_streams) {
            if (stream_info.isAudio()) {
                std::cout << "  Stream " << stream_info.stream_id << ": "
                          << stream_info.codec_name << " "
                          << stream_info.sample_rate << "Hz "
                          << stream_info.channels << "ch" << std::endl;
            }
        }
        
        // Get current stream info
        auto current_stream = stream->getCurrentStreamInfo();
        std::cout << "Playing: " << current_stream.codec_name 
                  << " " << current_stream.sample_rate << "Hz" << std::endl;
        
        // Display metadata
        std::cout << "Artist: " << stream->getArtist().toCString() << std::endl;
        std::cout << "Title: " << stream->getTitle().toCString() << std::endl;
        std::cout << "Album: " << stream->getAlbum().toCString() << std::endl;
        
        // Read and process PCM data
        const size_t buffer_size = 4096;
        std::vector<uint8_t> buffer(buffer_size);
        
        while (!stream->eof()) {
            size_t bytes_read = stream->getData(buffer_size, buffer.data());
            if (bytes_read > 0) {
                // Process PCM data (e.g., send to audio output)
                sendToAudioOutput(buffer.data(), bytes_read);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error playing audio: " << e.what() << std::endl;
    }
}

void sendToAudioOutput(const uint8_t* pcm_data, size_t size) {
    // Example: Send PCM data to audio output system
    // This would typically involve SDL, ALSA, or other audio APIs
    std::cout << "Sending " << size << " bytes to audio output" << std::endl;
}
```

### Example 3: HTTP Streaming with MediaFactory

```cpp
#include "psymp3.h"

void streamAudioFromHTTP(const std::string& url) {
    try {
        // MediaFactory automatically handles HTTP URLs
        auto stream = MediaFactory::createStream(url);
        if (!stream) {
            std::cerr << "Failed to create stream for URL: " << url << std::endl;
            return;
        }
        
        // Analyze content to get format information
        auto content_info = MediaFactory::analyzeContent(url);
        std::cout << "Detected format: " << content_info.detected_format << std::endl;
        std::cout << "MIME type: " << content_info.mime_type << std::endl;
        std::cout << "Confidence: " << content_info.confidence << std::endl;
        
        // Stream audio data
        const size_t buffer_size = 8192; // Larger buffer for streaming
        std::vector<uint8_t> buffer(buffer_size);
        
        while (!stream->eof()) {
            size_t bytes_read = stream->getData(buffer_size, buffer.data());
            if (bytes_read > 0) {
                // Process streaming audio data
                processStreamingAudio(buffer.data(), bytes_read);
            } else {
                // Handle potential network delays
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error streaming audio: " << e.what() << std::endl;
    }
}

void processStreamingAudio(const uint8_t* data, size_t size) {
    // Process streaming audio with appropriate buffering
    static std::vector<uint8_t> stream_buffer;
    
    // Add new data to buffer
    stream_buffer.insert(stream_buffer.end(), data, data + size);
    
    // Process complete frames when available
    const size_t frame_size = 1024; // Example frame size
    while (stream_buffer.size() >= frame_size) {
        // Process one frame
        processAudioFrame(stream_buffer.data(), frame_size);
        
        // Remove processed data
        stream_buffer.erase(stream_buffer.begin(), stream_buffer.begin() + frame_size);
    }
}

void processAudioFrame(const uint8_t* frame_data, size_t frame_size) {
    // Process individual audio frame
    std::cout << "Processing audio frame (" << frame_size << " bytes)" << std::endl;
}
```

## Advanced Integration Patterns

### Example 4: Multi-Stream Processing

```cpp
#include "psymp3.h"

class MultiStreamProcessor {
private:
    std::unique_ptr<Demuxer> demuxer;
    std::map<uint32_t, std::unique_ptr<AudioCodec>> codecs;
    std::map<uint32_t, std::queue<MediaChunk>> stream_buffers;
    
public:
    bool initialize(const std::string& filename) {
        auto handler = std::make_unique<FileIOHandler>(filename);
        demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        
        if (!demuxer || !demuxer->parseContainer()) {
            return false;
        }
        
        // Initialize codecs for all audio streams
        auto streams = demuxer->getStreams();
        for (const auto& stream : streams) {
            if (stream.isAudio()) {
                auto codec = createCodecForStream(stream);
                if (codec) {
                    codecs[stream.stream_id] = std::move(codec);
                }
            }
        }
        
        return !codecs.empty();
    }
    
    void processAllStreams() {
        while (!demuxer->isEOF()) {
            // Read chunks from all streams
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                // Buffer chunk for its stream
                stream_buffers[chunk.stream_id].push(std::move(chunk));
            }
            
            // Process buffered chunks
            for (auto& [stream_id, buffer] : stream_buffers) {
                if (!buffer.empty() && codecs.count(stream_id)) {
                    processStreamChunk(stream_id, buffer.front());
                    buffer.pop();
                }
            }
        }
        
        // Process remaining buffered chunks
        for (auto& [stream_id, buffer] : stream_buffers) {
            while (!buffer.empty() && codecs.count(stream_id)) {
                processStreamChunk(stream_id, buffer.front());
                buffer.pop();
            }
        }
    }
    
    void switchToStream(uint32_t stream_id) {
        if (codecs.count(stream_id)) {
            std::cout << "Switching to stream " << stream_id << std::endl;
            // Clear buffers for other streams
            for (auto& [id, buffer] : stream_buffers) {
                if (id != stream_id) {
                    std::queue<MediaChunk> empty;
                    buffer.swap(empty);
                }
            }
        }
    }
    
private:
    std::unique_ptr<AudioCodec> createCodecForStream(const StreamInfo& stream) {
        // Create appropriate codec based on stream info
        // This is a simplified example - actual implementation would
        // use the codec registry and handle various codec types
        
        if (stream.codec_name == "vorbis") {
            return std::make_unique<VorbisCodec>(stream);
        } else if (stream.codec_name == "flac") {
            return std::make_unique<FLACCodec>(stream);
        } else if (stream.codec_name == "opus") {
            return std::make_unique<OpusCodec>(stream);
        }
        // Add more codec types as needed
        
        return nullptr;
    }
    
    void processStreamChunk(uint32_t stream_id, const MediaChunk& chunk) {
        auto& codec = codecs[stream_id];
        
        // Decode chunk to PCM
        AudioFrame frame = codec->decode(chunk.data);
        if (frame.isValid()) {
            // Process decoded PCM data
            processPCMFrame(stream_id, frame);
        }
    }
    
    void processPCMFrame(uint32_t stream_id, const AudioFrame& frame) {
        std::cout << "Stream " << stream_id << ": decoded " 
                  << frame.sample_count << " samples" << std::endl;
        // Process PCM data (playback, analysis, etc.)
    }
};

void demonstrateMultiStreamProcessing(const std::string& filename) {
    MultiStreamProcessor processor;
    
    if (processor.initialize(filename)) {
        processor.processAllStreams();
    } else {
        std::cerr << "Failed to initialize multi-stream processor" << std::endl;
    }
}
```

### Example 5: Seeking and Position Management

```cpp
#include "psymp3.h"

class AudioPlayer {
private:
    std::unique_ptr<DemuxedStream> stream;
    uint64_t total_duration_ms = 0;
    uint64_t current_position_ms = 0;
    
public:
    bool open(const std::string& filename) {
        stream = std::make_unique<DemuxedStream>(TagLib::String(filename.c_str()));
        
        if (stream->eof()) {
            return false;
        }
        
        // Get duration from stream info
        auto stream_info = stream->getCurrentStreamInfo();
        total_duration_ms = stream_info.duration_ms;
        current_position_ms = 0;
        
        return true;
    }
    
    void play() {
        const size_t buffer_size = 4096;
        std::vector<uint8_t> buffer(buffer_size);
        
        while (!stream->eof()) {
            size_t bytes_read = stream->getData(buffer_size, buffer.data());
            if (bytes_read > 0) {
                // Send to audio output
                sendToAudioOutput(buffer.data(), bytes_read);
                
                // Update position (simplified - actual calculation would
                // depend on sample rate and format)
                updatePosition(bytes_read);
            }
        }
    }
    
    bool seekTo(double position_seconds) {
        uint64_t target_ms = static_cast<uint64_t>(position_seconds * 1000);
        
        if (target_ms > total_duration_ms) {
            target_ms = total_duration_ms;
        }
        
        try {
            // Convert to sample position for seeking
            auto stream_info = stream->getCurrentStreamInfo();
            unsigned long sample_position = static_cast<unsigned long>(
                (target_ms * stream_info.sample_rate) / 1000
            );
            
            stream->seekTo(sample_position);
            current_position_ms = target_ms;
            
            std::cout << "Seeked to " << position_seconds << " seconds" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Seek failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    double getCurrentPosition() const {
        return current_position_ms / 1000.0;
    }
    
    double getTotalDuration() const {
        return total_duration_ms / 1000.0;
    }
    
    void displayProgress() {
        double current = getCurrentPosition();
        double total = getTotalDuration();
        double progress = (total > 0) ? (current / total) * 100.0 : 0.0;
        
        std::cout << "Progress: " << std::fixed << std::setprecision(1)
                  << current << "s / " << total << "s (" 
                  << progress << "%)" << std::endl;
    }
    
private:
    void updatePosition(size_t bytes_processed) {
        // Simplified position update - actual implementation would
        // calculate based on sample rate, channels, and bit depth
        auto stream_info = stream->getCurrentStreamInfo();
        if (stream_info.sample_rate > 0 && stream_info.channels > 0) {
            size_t bytes_per_second = stream_info.sample_rate * 
                                    stream_info.channels * 
                                    (stream_info.bits_per_sample / 8);
            if (bytes_per_second > 0) {
                current_position_ms += (bytes_processed * 1000) / bytes_per_second;
            }
        }
    }
    
    void sendToAudioOutput(const uint8_t* data, size_t size) {
        // Send to actual audio output system
        // This is a placeholder for the actual audio output implementation
    }
};

void demonstrateAudioPlayer(const std::string& filename) {
    AudioPlayer player;
    
    if (!player.open(filename)) {
        std::cerr << "Failed to open audio file: " << filename << std::endl;
        return;
    }
    
    std::cout << "Opened: " << filename << std::endl;
    std::cout << "Duration: " << player.getTotalDuration() << " seconds" << std::endl;
    
    // Demonstrate seeking
    player.seekTo(30.0); // Seek to 30 seconds
    player.displayProgress();
    
    player.seekTo(60.0); // Seek to 1 minute
    player.displayProgress();
    
    player.seekTo(0.0);  // Seek back to beginning
    player.displayProgress();
    
    // Play from beginning (in a real implementation, this would be in a separate thread)
    // player.play();
}
```

## Error Handling Examples

### Example 6: Comprehensive Error Handling

```cpp
#include "psymp3.h"

class RobustAudioProcessor {
private:
    std::unique_ptr<Demuxer> demuxer;
    std::string current_file;
    
public:
    enum class ProcessResult {
        SUCCESS,
        FILE_NOT_FOUND,
        UNSUPPORTED_FORMAT,
        CORRUPTED_FILE,
        IO_ERROR,
        MEMORY_ERROR,
        UNKNOWN_ERROR
    };
    
    ProcessResult processFile(const std::string& filename) {
        current_file = filename;
        
        try {
            // Step 1: Create IOHandler with error checking
            std::unique_ptr<IOHandler> handler;
            try {
                handler = std::make_unique<FileIOHandler>(filename);
            } catch (const std::exception& e) {
                std::cerr << "Failed to open file '" << filename << "': " << e.what() << std::endl;
                return ProcessResult::FILE_NOT_FOUND;
            }
            
            // Step 2: Create demuxer with format detection
            demuxer = DemuxerFactory::createDemuxer(std::move(handler));
            if (!demuxer) {
                std::cerr << "Unsupported file format: " << filename << std::endl;
                return ProcessResult::UNSUPPORTED_FORMAT;
            }
            
            // Step 3: Parse container with error recovery
            if (!demuxer->parseContainer()) {
                return handleParseError();
            }
            
            // Step 4: Validate streams
            auto streams = demuxer->getStreams();
            if (streams.empty()) {
                std::cerr << "No streams found in file: " << filename << std::endl;
                return ProcessResult::CORRUPTED_FILE;
            }
            
            // Step 5: Process data with error recovery
            return processDataWithRecovery();
            
        } catch (const std::bad_alloc& e) {
            std::cerr << "Memory allocation failed: " << e.what() << std::endl;
            return ProcessResult::MEMORY_ERROR;
        } catch (const std::exception& e) {
            std::cerr << "Unexpected error: " << e.what() << std::endl;
            return ProcessResult::UNKNOWN_ERROR;
        }
    }
    
private:
    ProcessResult handleParseError() {
        const auto& error = demuxer->getLastError();
        
        std::cerr << "Parse error in '" << current_file << "':" << std::endl;
        std::cerr << "  Category: " << error.category << std::endl;
        std::cerr << "  Message: " << error.message << std::endl;
        std::cerr << "  Offset: " << error.file_offset << std::endl;
        
        // Determine error type and potential recovery
        if (error.category == "IO") {
            return ProcessResult::IO_ERROR;
        } else if (error.category == "Format") {
            // Try to recover from format errors
            if (error.recovery != DemuxerErrorRecovery::NONE) {
                std::cout << "Attempting error recovery..." << std::endl;
                demuxer->clearError();
                
                // Retry parsing after recovery attempt
                if (demuxer->parseContainer()) {
                    std::cout << "Recovery successful!" << std::endl;
                    return processDataWithRecovery();
                }
            }
            return ProcessResult::CORRUPTED_FILE;
        } else if (error.category == "Memory") {
            return ProcessResult::MEMORY_ERROR;
        }
        
        return ProcessResult::UNKNOWN_ERROR;
    }
    
    ProcessResult processDataWithRecovery() {
        size_t successful_chunks = 0;
        size_t failed_chunks = 0;
        size_t consecutive_failures = 0;
        const size_t max_consecutive_failures = 10;
        
        while (!demuxer->isEOF()) {
            MediaChunk chunk = demuxer->readChunk();
            
            if (chunk.isValid()) {
                // Process successful chunk
                if (processChunk(chunk)) {
                    successful_chunks++;
                    consecutive_failures = 0; // Reset failure counter
                } else {
                    failed_chunks++;
                    consecutive_failures++;
                }
            } else {
                // Handle read failure
                if (demuxer->hasError()) {
                    const auto& error = demuxer->getLastError();
                    std::cerr << "Read error: " << error.message << std::endl;
                    
                    // Attempt recovery based on error type
                    if (attemptReadRecovery(error)) {
                        demuxer->clearError();
                        consecutive_failures = 0;
                        continue;
                    } else {
                        failed_chunks++;
                        consecutive_failures++;
                    }
                } else {
                    // EOF or empty chunk
                    break;
                }
            }
            
            // Check if too many consecutive failures
            if (consecutive_failures >= max_consecutive_failures) {
                std::cerr << "Too many consecutive failures (" 
                          << consecutive_failures << "), aborting" << std::endl;
                return ProcessResult::CORRUPTED_FILE;
            }
        }
        
        // Report processing statistics
        std::cout << "Processing complete:" << std::endl;
        std::cout << "  Successful chunks: " << successful_chunks << std::endl;
        std::cout << "  Failed chunks: " << failed_chunks << std::endl;
        
        // Determine overall result
        if (successful_chunks == 0) {
            return ProcessResult::CORRUPTED_FILE;
        } else if (failed_chunks > successful_chunks / 2) {
            std::cout << "Warning: High failure rate, file may be corrupted" << std::endl;
        }
        
        return ProcessResult::SUCCESS;
    }
    
    bool processChunk(const MediaChunk& chunk) {
        try {
            // Validate chunk data
            if (chunk.data.empty()) {
                return false;
            }
            
            // Process chunk data
            // This is where you would typically pass the chunk to a codec
            std::cout << "Processing chunk from stream " << chunk.stream_id 
                      << " (" << chunk.getDataSize() << " bytes)" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing chunk: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool attemptReadRecovery(const DemuxerError& error) {
        std::cout << "Attempting read recovery for error: " << error.message << std::endl;
        
        // Recovery strategies based on error category
        if (error.category == "IO") {
            // For I/O errors, try a small delay and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return true;
        } else if (error.category == "Format") {
            // For format errors, try to skip to next valid section
            return error.recovery == DemuxerErrorRecovery::SKIP_SECTION;
        }
        
        return false;
    }
};

void demonstrateRobustProcessing(const std::vector<std::string>& filenames) {
    RobustAudioProcessor processor;
    
    for (const auto& filename : filenames) {
        std::cout << "\nProcessing: " << filename << std::endl;
        
        auto result = processor.processFile(filename);
        
        switch (result) {
            case RobustAudioProcessor::ProcessResult::SUCCESS:
                std::cout << "✓ Successfully processed" << std::endl;
                break;
            case RobustAudioProcessor::ProcessResult::FILE_NOT_FOUND:
                std::cout << "✗ File not found" << std::endl;
                break;
            case RobustAudioProcessor::ProcessResult::UNSUPPORTED_FORMAT:
                std::cout << "✗ Unsupported format" << std::endl;
                break;
            case RobustAudioProcessor::ProcessResult::CORRUPTED_FILE:
                std::cout << "✗ File appears to be corrupted" << std::endl;
                break;
            case RobustAudioProcessor::ProcessResult::IO_ERROR:
                std::cout << "✗ I/O error occurred" << std::endl;
                break;
            case RobustAudioProcessor::ProcessResult::MEMORY_ERROR:
                std::cout << "✗ Memory allocation failed" << std::endl;
                break;
            case RobustAudioProcessor::ProcessResult::UNKNOWN_ERROR:
                std::cout << "✗ Unknown error occurred" << std::endl;
                break;
        }
    }
}
```

## Performance Optimization Examples

### Example 7: Memory-Efficient Streaming

```cpp
#include "psymp3.h"

class MemoryEfficientStreamer {
private:
    std::unique_ptr<Demuxer> demuxer;
    std::unique_ptr<AudioCodec> codec;
    
    // Bounded buffers to prevent memory exhaustion
    std::queue<MediaChunk> chunk_buffer;
    std::queue<AudioFrame> frame_buffer;
    
    // Buffer limits
    static constexpr size_t MAX_CHUNK_BUFFER_SIZE = 8;
    static constexpr size_t MAX_FRAME_BUFFER_SIZE = 4;
    static constexpr size_t MAX_CHUNK_BUFFER_BYTES = 256 * 1024; // 256KB
    
    size_t current_chunk_buffer_bytes = 0;
    
public:
    bool initialize(const std::string& filename) {
        // Create demuxer
        auto handler = std::make_unique<FileIOHandler>(filename);
        demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        
        if (!demuxer || !demuxer->parseContainer()) {
            return false;
        }
        
        // Find first audio stream and create codec
        auto streams = demuxer->getStreams();
        for (const auto& stream : streams) {
            if (stream.isAudio()) {
                codec = createCodecForStream(stream);
                if (codec) {
                    break;
                }
            }
        }
        
        return codec != nullptr;
    }
    
    void streamWithMemoryControl() {
        while (!demuxer->isEOF() || !chunk_buffer.empty() || !frame_buffer.empty()) {
            // Fill chunk buffer if needed and memory allows
            fillChunkBuffer();
            
            // Decode chunks to frames if needed
            decodeChunksToFrames();
            
            // Process available frames
            processAvailableFrames();
            
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
private:
    void fillChunkBuffer() {
        // Only fill if we have space and memory budget
        while (chunk_buffer.size() < MAX_CHUNK_BUFFER_SIZE &&
               current_chunk_buffer_bytes < MAX_CHUNK_BUFFER_BYTES &&
               !demuxer->isEOF()) {
            
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                current_chunk_buffer_bytes += chunk.getDataSize();
                chunk_buffer.push(std::move(chunk));
            } else {
                break;
            }
        }
    }
    
    void decodeChunksToFrames() {
        // Decode chunks while we have space in frame buffer
        while (!chunk_buffer.empty() && 
               frame_buffer.size() < MAX_FRAME_BUFFER_SIZE) {
            
            MediaChunk chunk = std::move(chunk_buffer.front());
            chunk_buffer.pop();
            current_chunk_buffer_bytes -= chunk.getDataSize();
            
            // Decode chunk to audio frame
            AudioFrame frame = codec->decode(chunk.data);
            if (frame.isValid()) {
                frame_buffer.push(std::move(frame));
            }
        }
    }
    
    void processAvailableFrames() {
        // Process all available frames
        while (!frame_buffer.empty()) {
            AudioFrame frame = std::move(frame_buffer.front());
            frame_buffer.pop();
            
            // Process frame (send to audio output, etc.)
            processAudioFrame(frame);
        }
    }
    
    void processAudioFrame(const AudioFrame& frame) {
        // Example: Send frame to audio output with minimal memory copying
        sendToAudioOutput(frame.data.data(), frame.data.size());
        
        // Update statistics
        static size_t total_frames = 0;
        static size_t total_samples = 0;
        
        total_frames++;
        total_samples += frame.sample_count;
        
        if (total_frames % 100 == 0) {
            std::cout << "Processed " << total_frames << " frames ("
                      << total_samples << " samples)" << std::endl;
            
            // Report memory usage
            std::cout << "Memory usage: " 
                      << current_chunk_buffer_bytes << " bytes in chunk buffer, "
                      << chunk_buffer.size() << " chunks, "
                      << frame_buffer.size() << " frames" << std::endl;
        }
    }
    
    std::unique_ptr<AudioCodec> createCodecForStream(const StreamInfo& stream) {
        // Create appropriate codec (simplified example)
        if (stream.codec_name == "vorbis") {
            return std::make_unique<VorbisCodec>(stream);
        } else if (stream.codec_name == "flac") {
            return std::make_unique<FLACCodec>(stream);
        }
        return nullptr;
    }
    
    void sendToAudioOutput(const uint8_t* data, size_t size) {
        // Send to actual audio output system
        // This should be implemented to use the actual audio API
    }
};

void demonstrateMemoryEfficientStreaming(const std::string& filename) {
    MemoryEfficientStreamer streamer;
    
    if (streamer.initialize(filename)) {
        std::cout << "Starting memory-efficient streaming of: " << filename << std::endl;
        streamer.streamWithMemoryControl();
        std::cout << "Streaming complete" << std::endl;
    } else {
        std::cerr << "Failed to initialize streamer for: " << filename << std::endl;
    }
}
```

## Thread Safety Examples

### Example 8: Thread-Safe Multi-File Processing

```cpp
#include "psymp3.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadSafeAudioProcessor {
private:
    struct ProcessingTask {
        std::string filename;
        std::promise<bool> result_promise;
    };
    
    std::queue<ProcessingTask> task_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> shutdown{false};
    std::vector<std::thread> worker_threads;
    
    // Thread-safe statistics
    std::atomic<size_t> files_processed{0};
    std::atomic<size_t> files_failed{0};
    std::mutex stats_mutex;
    std::map<std::string, size_t> format_counts;
    
public:
    ThreadSafeAudioProcessor(size_t num_threads = std::thread::hardware_concurrency()) {
        // Start worker threads
        for (size_t i = 0; i < num_threads; ++i) {
            worker_threads.emplace_back(&ThreadSafeAudioProcessor::workerThread, this, i);
        }
    }
    
    ~ThreadSafeAudioProcessor() {
        shutdown = true;
        queue_cv.notify_all();
        
        for (auto& thread : worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    std::future<bool> processFileAsync(const std::string& filename) {
        ProcessingTask task;
        task.filename = filename;
        auto future = task.result_promise.get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(std::move(task));
        }
        queue_cv.notify_one();
        
        return future;
    }
    
    void processFilesAsync(const std::vector<std::string>& filenames) {
        std::vector<std::future<bool>> futures;
        
        // Submit all tasks
        for (const auto& filename : filenames) {
            futures.push_back(processFileAsync(filename));
        }
        
        // Wait for all tasks to complete
        for (size_t i = 0; i < futures.size(); ++i) {
            bool success = futures[i].get();
            std::cout << "File " << (i + 1) << "/" << filenames.size() 
                      << ": " << filenames[i] 
                      << (success ? " ✓" : " ✗") << std::endl;
        }
        
        printStatistics();
    }
    
    void printStatistics() {
        std::lock_guard<std::mutex> lock(stats_mutex);
        
        std::cout << "\nProcessing Statistics:" << std::endl;
        std::cout << "Files processed: " << files_processed.load() << std::endl;
        std::cout << "Files failed: " << files_failed.load() << std::endl;
        
        std::cout << "Format distribution:" << std::endl;
        for (const auto& [format, count] : format_counts) {
            std::cout << "  " << format << ": " << count << std::endl;
        }
    }
    
private:
    void workerThread(size_t thread_id) {
        std::cout << "Worker thread " << thread_id << " started" << std::endl;
        
        while (!shutdown) {
            ProcessingTask task;
            
            // Get next task
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this] { return !task_queue.empty() || shutdown; });
                
                if (shutdown && task_queue.empty()) {
                    break;
                }
                
                if (!task_queue.empty()) {
                    task = std::move(task_queue.front());
                    task_queue.pop();
                } else {
                    continue;
                }
            }
            
            // Process task
            bool success = processFileSafely(task.filename, thread_id);
            task.result_promise.set_value(success);
            
            // Update statistics
            if (success) {
                files_processed++;
            } else {
                files_failed++;
            }
        }
        
        std::cout << "Worker thread " << thread_id << " finished" << std::endl;
    }
    
    bool processFileSafely(const std::string& filename, size_t thread_id) {
        try {
            // Each thread uses its own demuxer instance for thread safety
            auto handler = std::make_unique<FileIOHandler>(filename);
            auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
            
            if (!demuxer) {
                std::cerr << "[Thread " << thread_id << "] Unsupported format: " 
                          << filename << std::endl;
                return false;
            }
            
            if (!demuxer->parseContainer()) {
                std::cerr << "[Thread " << thread_id << "] Parse failed: " 
                          << filename << std::endl;
                return false;
            }
            
            // Detect format for statistics
            std::string format = detectFormat(demuxer.get());
            {
                std::lock_guard<std::mutex> lock(stats_mutex);
                format_counts[format]++;
            }
            
            // Process file
            size_t chunk_count = 0;
            while (!demuxer->isEOF()) {
                MediaChunk chunk = demuxer->readChunk();
                if (chunk.isValid()) {
                    chunk_count++;
                    // Process chunk (simplified)
                }
            }
            
            std::cout << "[Thread " << thread_id << "] Processed " << filename 
                      << " (" << chunk_count << " chunks)" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[Thread " << thread_id << "] Error processing " 
                      << filename << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    std::string detectFormat(Demuxer* demuxer) {
        // Simple format detection based on demuxer type
        // In a real implementation, this might use RTTI or a virtual method
        
        auto streams = demuxer->getStreams();
        if (!streams.empty()) {
            const auto& first_stream = streams[0];
            if (first_stream.codec_name == "vorbis" || first_stream.codec_name == "opus") {
                return "Ogg";
            } else if (first_stream.codec_name == "pcm") {
                return "WAV/AIFF";
            } else if (first_stream.codec_name == "aac") {
                return "MP4/M4A";
            }
        }
        
        return "Unknown";
    }
};

void demonstrateThreadSafeProcessing() {
    // Example file list
    std::vector<std::string> test_files = {
        "audio1.ogg",
        "audio2.wav",
        "audio3.m4a",
        "audio4.flac",
        "audio5.mp3"
    };
    
    // Create processor with 4 worker threads
    ThreadSafeAudioProcessor processor(4);
    
    std::cout << "Processing " << test_files.size() 
              << " files with 4 worker threads..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    processor.processFilesAsync(test_files);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    std::cout << "Processing completed in " << duration.count() << " ms" << std::endl;
}
```

## Plugin Development Examples

### Example 9: Custom Demuxer Plugin

```cpp
#include "psymp3.h"

// Example: Custom demuxer for a hypothetical "MyAudio" format
class MyAudioDemuxer : public Demuxer {
private:
    struct MyAudioHeader {
        char signature[4];        // "MYAU"
        uint32_t version;         // Format version
        uint32_t sample_rate;     // Sample rate
        uint16_t channels;        // Number of channels
        uint16_t bits_per_sample; // Bits per sample
        uint32_t data_size;       // Size of audio data
        uint32_t flags;           // Format flags
    };
    
    MyAudioHeader header;
    uint64_t data_start_offset = 0;
    uint64_t current_data_offset = 0;
    
public:
    explicit MyAudioDemuxer(std::unique_ptr<IOHandler> handler) 
        : Demuxer(std::move(handler)) {}
    
    bool parseContainer() override {
        try {
            // Read and validate header
            if (m_handler->read(&header, sizeof(header), 1) != 1) {
                reportError("IO", "Failed to read MyAudio header");
                return false;
            }
            
            // Validate signature
            if (std::memcmp(header.signature, "MYAU", 4) != 0) {
                reportError("Format", "Invalid MyAudio signature");
                return false;
            }
            
            // Validate version
            if (header.version > 1) {
                reportError("Format", "Unsupported MyAudio version: " + 
                           std::to_string(header.version));
                return false;
            }
            
            // Calculate data start offset
            data_start_offset = sizeof(header);
            current_data_offset = data_start_offset;
            
            // Create stream info
            StreamInfo stream;
            stream.stream_id = 1;
            stream.codec_type = "audio";
            stream.codec_name = "myaudio_pcm";
            stream.sample_rate = header.sample_rate;
            stream.channels = header.channels;
            stream.bits_per_sample = header.bits_per_sample;
            stream.bitrate = header.sample_rate * header.channels * header.bits_per_sample;
            
            // Calculate duration
            uint64_t total_samples = header.data_size / 
                                   (header.channels * (header.bits_per_sample / 8));
            stream.duration_samples = total_samples;
            stream.duration_ms = (total_samples * 1000) / header.sample_rate;
            
            m_streams.push_back(stream);
            m_duration_ms = stream.duration_ms;
            setParsed(true);
            
            return true;
            
        } catch (const std::exception& e) {
            reportError("Exception", "Error parsing MyAudio container: " + 
                       std::string(e.what()));
            return false;
        }
    }
    
    std::vector<StreamInfo> getStreams() const override {
        return m_streams;
    }
    
    StreamInfo getStreamInfo(uint32_t stream_id) const override {
        if (stream_id == 1 && !m_streams.empty()) {
            return m_streams[0];
        }
        return StreamInfo{};
    }
    
    MediaChunk readChunk() override {
        return readChunk(1); // Default to stream 1
    }
    
    MediaChunk readChunk(uint32_t stream_id) override {
        if (stream_id != 1 || isEOF()) {
            return MediaChunk{};
        }
        
        try {
            // Calculate chunk size (e.g., 1024 samples)
            const size_t samples_per_chunk = 1024;
            size_t bytes_per_sample = header.channels * (header.bits_per_sample / 8);
            size_t chunk_size = samples_per_chunk * bytes_per_sample;
            
            // Don't read beyond end of data
            uint64_t remaining_bytes = (data_start_offset + header.data_size) - 
                                     current_data_offset;
            if (remaining_bytes == 0) {
                setEOF(true);
                return MediaChunk{};
            }
            
            chunk_size = std::min(chunk_size, static_cast<size_t>(remaining_bytes));
            
            // Seek to current position
            if (m_handler->seek(static_cast<long>(current_data_offset), SEEK_SET) != 0) {
                reportError("IO", "Failed to seek to data position");
                return MediaChunk{};
            }
            
            // Read chunk data
            MediaChunk chunk(stream_id, chunk_size);
            size_t bytes_read = m_handler->read(chunk.data.data(), 1, chunk_size);
            
            if (bytes_read == 0) {
                setEOF(true);
                return MediaChunk{};
            }
            
            chunk.data.resize(bytes_read);
            chunk.timestamp_samples = (current_data_offset - data_start_offset) / bytes_per_sample;
            chunk.file_offset = current_data_offset;
            
            // Update position
            current_data_offset += bytes_read;
            updatePosition((chunk.timestamp_samples * 1000) / header.sample_rate);
            
            return chunk;
            
        } catch (const std::exception& e) {
            reportError("Exception", "Error reading MyAudio chunk: " + 
                       std::string(e.what()));
            return MediaChunk{};
        }
    }
    
    bool seekTo(uint64_t timestamp_ms) override {
        try {
            // Calculate target sample position
            uint64_t target_sample = (timestamp_ms * header.sample_rate) / 1000;
            size_t bytes_per_sample = header.channels * (header.bits_per_sample / 8);
            uint64_t target_offset = data_start_offset + (target_sample * bytes_per_sample);
            
            // Clamp to valid range
            uint64_t max_offset = data_start_offset + header.data_size;
            if (target_offset >= max_offset) {
                target_offset = max_offset;
                setEOF(true);
            } else {
                setEOF(false);
            }
            
            current_data_offset = target_offset;
            updatePosition(timestamp_ms);
            
            return true;
            
        } catch (const std::exception& e) {
            reportError("Exception", "Error seeking in MyAudio file: " + 
                       std::string(e.what()));
            return false;
        }
    }
    
    bool isEOF() const override {
        return isEOFAtomic() || 
               (current_data_offset >= data_start_offset + header.data_size);
    }
    
    uint64_t getDuration() const override {
        return m_duration_ms;
    }
    
    uint64_t getPosition() const override {
        if (header.sample_rate == 0) return 0;
        
        size_t bytes_per_sample = header.channels * (header.bits_per_sample / 8);
        uint64_t current_sample = (current_data_offset - data_start_offset) / bytes_per_sample;
        return (current_sample * 1000) / header.sample_rate;
    }
};

// Plugin registration function
void registerMyAudioPlugin() {
    // Register demuxer factory function
    DemuxerFactory::registerDemuxer("myaudio", 
        [](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
            return std::make_unique<MyAudioDemuxer>(std::move(handler));
        });
    
    // Register format signature
    FormatSignature signature("myaudio", {'M', 'Y', 'A', 'U'}, 0, 90);
    DemuxerFactory::registerSignature(signature);
    
    // Register with MediaFactory
    MediaFormat format;
    format.format_id = "myaudio";
    format.display_name = "MyAudio Format";
    format.extensions = {".myau", ".mya"};
    format.mime_types = {"audio/myaudio"};
    format.magic_signatures = {"MYAU"};
    format.priority = 90;
    format.supports_streaming = true;
    format.supports_seeking = true;
    format.is_container = true;
    format.description = "Custom MyAudio container format";
    
    StreamFactory factory = [](const std::string& uri, const ContentInfo& info) -> std::unique_ptr<Stream> {
        return std::make_unique<DemuxedStream>(TagLib::String(uri.c_str()));
    };
    
    MediaFactory::registerFormat(format, factory);
    
    std::cout << "MyAudio plugin registered successfully" << std::endl;
}

void demonstrateCustomPlugin() {
    // Register the plugin
    registerMyAudioPlugin();
    
    // Test the plugin
    std::string test_file = "test.myau";
    
    // Test with DemuxerFactory
    auto handler = std::make_unique<FileIOHandler>(test_file);
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    
    if (demuxer) {
        std::cout << "Successfully created MyAudio demuxer" << std::endl;
        
        if (demuxer->parseContainer()) {
            auto streams = demuxer->getStreams();
            std::cout << "Found " << streams.size() << " streams" << std::endl;
            
            for (const auto& stream : streams) {
                std::cout << "Stream " << stream.stream_id << ": "
                          << stream.codec_name << " "
                          << stream.sample_rate << "Hz "
                          << stream.channels << "ch" << std::endl;
            }
        }
    }
    
    // Test with MediaFactory
    if (MediaFactory::supportsExtension(".myau")) {
        std::cout << "MediaFactory recognizes .myau extension" << std::endl;
        
        auto stream = MediaFactory::createStream(test_file);
        if (stream) {
            std::cout << "Successfully created stream via MediaFactory" << std::endl;
        }
    }
}
```

These examples demonstrate the comprehensive usage patterns, error handling strategies, performance optimization techniques, thread safety considerations, and plugin development approaches for the PsyMP3 Demuxer Architecture. Each example includes detailed comments explaining the concepts and best practices for effective integration with the system.