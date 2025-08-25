#include "psymp3.h"

int main() {
    const std::string filename = "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac";
    
    std::cout << "Testing real FLAC file: " << filename << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TagLib::String(filename.c_str()));
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        std::cout << "Created demuxer, calling parseContainer()..." << std::endl;
        bool result = demuxer->parseContainer();
        
        std::cout << "parseContainer() returned: " << (result ? "true" : "false") << std::endl;
        
        if (!result) {
            if (demuxer->hasError()) {
                auto error = demuxer->getLastError();
                std::cout << "Error: [" << error.category << "] " << error.message << std::endl;
            } else {
                std::cout << "No error information available" << std::endl;
            }
        } else {
            std::cout << "Parse succeeded!" << std::endl;
            
            auto streams = demuxer->getStreams();
            std::cout << "Found " << streams.size() << " streams" << std::endl;
            
            if (!streams.empty()) {
                const auto& stream = streams[0];
                std::cout << "Stream info:" << std::endl;
                std::cout << "  Codec: " << stream.codec_name << std::endl;
                std::cout << "  Sample rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "  Channels: " << stream.channels << std::endl;
                std::cout << "  Bits per sample: " << stream.bits_per_sample << std::endl;
                std::cout << "  Duration: " << stream.duration_ms << " ms" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}