/*
 * ModernStream.h - Stream wrapper for modern demuxer/codec architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MODERNSTREAM_H
#define MODERNSTREAM_H

namespace PsyMP3 {
namespace Demuxer {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Modern Stream implementation using the new demuxer/codec architecture
 * 
 * This class bridges the old Stream interface with the new modular
 * demuxer and codec system, providing backward compatibility while
 * leveraging the improved architecture.
 */
class ModernStream : public Stream {
public:
    explicit ModernStream(const TagLib::String& name);
    virtual ~ModernStream();

    // Stream interface implementation
    virtual void open(TagLib::String name) override;
    virtual size_t getData(size_t len, void *buf) override;
    virtual unsigned int getPosition() override;
    virtual unsigned long long getSPosition() override;
    virtual void seekTo(unsigned long pos) override;
    virtual bool eof() override;

private:
    std::unique_ptr<DemuxedStream> m_demuxed_stream;
    TagLib::String m_file_name;
    bool m_opened = false;
    
    /**
     * @brief Initialize the demuxer/codec chain for the given file
     */
    bool initializeAudioChain(const TagLib::String& name);
};


} // namespace Demuxer
} // namespace PsyMP3
#endif // MODERNSTREAM_H