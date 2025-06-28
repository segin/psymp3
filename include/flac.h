/*
 * flac.h - FLAC decoder class header
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef FLAC_H
#define FLAC_H

#include "IOHandler.h"

class FlacDecoder: public FLAC::Decoder::Stream
{
    public:
        FlacDecoder(TagLib::String path);
        ~FlacDecoder();
        TagLib::String m_path;
        // Internal buffer for decoded 16-bit PCM samples
        std::vector<int16_t> m_output_buffer;
        std::mutex m_output_buffer_mutex;
        std::condition_variable m_output_buffer_cv;
        FLAC__StreamMetadata_StreamInfo m_stream_info; // To store metadata from callback
        virtual ::FLAC__StreamDecoderInitStatus init();

        // Threading and state management
        void startDecoderThread();
        void stopDecoderThread();
        void requestSeek(FLAC__uint64 sample_offset);
        FLAC__uint64 get_current_sample_position() const { return m_current_sample_position.load(); }

    protected:
        virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
        virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata);
        virtual void error_callback(::FLAC__StreamDecoderErrorStatus status);
        virtual ::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t *bytes);
        virtual ::FLAC__StreamDecoderSeekStatus seek_callback(FLAC__uint64 absolute_byte_offset);
        virtual ::FLAC__StreamDecoderTellStatus tell_callback(FLAC__uint64 *absolute_byte_offset);
        virtual ::FLAC__StreamDecoderLengthStatus length_callback(FLAC__uint64 *stream_length);
        virtual bool eof_callback();
    private:
    	FlacDecoder(const FlacDecoder&);
	    FlacDecoder &operator=(const FlacDecoder&);
        std::unique_ptr<IOHandler> m_handler;
        std::thread m_decoder_thread;
        std::atomic<bool> m_decoding_active;
        std::atomic<bool> m_seek_request;
        std::atomic<FLAC__uint64> m_seek_position_samples;
        std::atomic<FLAC__uint64> m_current_sample_position{0};

        void decoderThreadLoop(); // The function for our thread
};

class Flac : public Stream
{
    public:
        /** Default constructor */
        Flac(TagLib::String name);
        /** Default destructor */
        virtual ~Flac();
        virtual void open(TagLib::String name);
        virtual size_t getData(size_t len, void *buf);
        virtual unsigned int getPosition();
        virtual unsigned long long getSPosition();
        virtual void seekTo(unsigned long pos);
        virtual bool eof();
    protected:
    private:
        FlacDecoder m_handle;
};

#endif // FLAC_H
