#include "psymp3.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <audio_file>" << std::endl;
        return 1;
    }
    
    std::string file_path = argv[1];
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    try {
        std::cout << "=== Audio Pipeline Debug ===" << std::endl;
        std::cout << "File: " << file_path << std::endl;
        
        // Test MediaFactory stream creation
        std::cout << "\n1. Testing MediaFactory::createStream..." << std::endl;
        std::unique_ptr<Stream> stream = MediaFactory::createStream(file_path);
        
        if (!stream) {
            std::cout << "ERROR: MediaFactory::createStream returned null" << std::endl;
            return 1;
        }
        
        std::cout << "SUCCESS: Stream created" << std::endl;
        std::cout << "Stream type: " << typeid(*stream).name() << std::endl;
        std::cout << "Rate: " << stream->getRate() << " Hz" << std::endl;
        std::cout << "Channels: " << stream->getChannels() << std::endl;
        std::cout << "Length: " << stream->getLength() << " ms" << std::endl;
        std::cout << "Bitrate: " << stream->getBitrate() << " bps" << std::endl;
        
        // Test reading some data
        std::cout << "\n2. Testing stream data reading..." << std::endl;
        std::vector<int16_t> buffer(4096);
        size_t bytes_read = stream->getData(buffer.size() * sizeof(int16_t), buffer.data());
        
        std::cout << "Bytes read: " << bytes_read << std::endl;
        std::cout << "Samples read: " << bytes_read / sizeof(int16_t) << std::endl;
        std::cout << "EOF: " << (stream->eof() ? "true" : "false") << std::endl;
        
        // Check if we got any non-zero data
        bool has_audio_data = false;
        for (size_t i = 0; i < bytes_read / sizeof(int16_t); i++) {
            if (buffer[i] != 0) {
                has_audio_data = true;
                break;
            }
        }
        
        std::cout << "Has non-zero audio data: " << (has_audio_data ? "YES" : "NO") << std::endl;
        
        if (has_audio_data) {
            // Show first few samples
            std::cout << "First 10 samples: ";
            for (int i = 0; i < 10 && i < bytes_read / sizeof(int16_t); i++) {
                std::cout << buffer[i] << " ";
            }
            std::cout << std::endl;
        }
        
        // Test if it's a DemuxedStream
        DemuxedStream* demuxed = dynamic_cast<DemuxedStream*>(stream.get());
        if (demuxed) {
            std::cout << "\n3. DemuxedStream specific tests..." << std::endl;
            std::cout << "Demuxer type: " << demuxed->getDemuxerType() << std::endl;
            std::cout << "Codec type: " << demuxed->getCodecType() << std::endl;
            
            auto streams = demuxed->getAvailableStreams();
            std::cout << "Available streams: " << streams.size() << std::endl;
            for (const auto& s : streams) {
                std::cout << "  Stream " << s.stream_id << ": " << s.codec_type 
                         << " (" << s.codec_name << "), " << s.sample_rate << "Hz, " 
                         << s.channels << " channels" << std::endl;
            }
        }
        
        std::cout << "\n=== Debug Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception caught: " << e.what() << std::endl;
        return 1;
    }
    
    SDL_Quit();
    return 0;
}