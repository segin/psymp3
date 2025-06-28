/*
 * audio.cpp - Audio class implementation
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

#include "psymp3.h"

Audio::Audio(struct atdata *data)
    : m_stream(data->stream),
      m_fft(data->fft),
      m_player_mutex(data->mutex),
      m_active(true),
      m_playing(false)
{
    std::cout << "Audio::Audio(): " << std::dec << m_stream->getRate() << "Hz, channels: " << std::dec << m_stream->getChannels() << std::endl;
    m_buffer.reserve(16384); // Reserve space for the buffer to avoid reallocations
    setup();
    m_decoder_thread = std::thread(&Audio::decoderThreadLoop, this);
}

Audio::~Audio() {
    play(false);
    if (m_active) {
        m_active = false;
        m_buffer_cv.notify_all(); // Wake up all waiting threads
        if (m_decoder_thread.joinable()) {
            m_decoder_thread.join();
        }
    }
    SDL_CloseAudio();
}

void Audio::setup() {
    SDL_AudioSpec fmt;
    fmt.freq = m_rate = m_stream->getRate();
    fmt.format = AUDIO_S16; /* Always, I hope */
    fmt.channels = m_channels = m_stream->getChannels();
    std::cout << "Audio::setup: channels: " << m_stream->getChannels() << std::endl;
    fmt.samples = 512 * fmt.channels; /* 512 samples for fft */
    fmt.callback = callback;
    fmt.userdata = this;
    if ( SDL_OpenAudio(&fmt, nullptr) < 0 ) {
        std::cerr << "Unable to open audio: " << SDL_GetError() << std::endl;
        // throw;
    }
}

void Audio::play(bool go) {
    m_playing = go;
    SDL_PauseAudio(go ? 0 : 1);
}

void Audio::lock(void) {
    SDL_LockAudio();
}

void Audio::unlock(void) {
    SDL_UnlockAudio();
}

void Audio::decoderThreadLoop() {
    System::setThisThreadName("audio-decoder");
    std::vector<int16_t> decode_chunk(4096); // Decode in 8KB chunks
    constexpr size_t BUFFER_HIGH_WATER_MARK = 48000 * 2; // 1 sec of 48kHz stereo

    while (m_active) {
        {
            std::unique_lock<std::mutex> lock(m_buffer_mutex);
            m_buffer_cv.wait(lock, [this] {
                // Wait if the buffer is full, but not if we're shutting down
                return m_buffer.size() < BUFFER_HIGH_WATER_MARK || !m_active;
            });
        }

        if (!m_active) break;

        size_t bytes_read = 0;
        bool eof = false;
        {
            // Lock the player mutex to safely access the stream object
            std::lock_guard<std::mutex> lock(*m_player_mutex);
            if (m_stream) {
                bytes_read = m_stream->getData(decode_chunk.size() * sizeof(int16_t), decode_chunk.data());
                eof = m_stream->eof();
            }
        }

        if (bytes_read > 0) {
            std::lock_guard<std::mutex> lock(m_buffer_mutex);
            size_t samples_read = bytes_read / sizeof(int16_t);
            m_buffer.insert(m_buffer.end(), decode_chunk.begin(), decode_chunk.begin() + samples_read);
        }

        m_buffer_cv.notify_one(); // Notify callback that data is available

        if (eof) {
            // The stream is finished, so the decoder's job is done for this track.
            // The thread will exit, and a new Audio object will be created for the next track.
            break;
        }
    }
}

/* Actually push the audio to the soundcard.
 * Audio is summed to mono (if stereo) and then FFT'd.
 */
void Audio::callback(void *userdata, Uint8 *buf, int len) {
    // Name the audio thread on its first run.
    // 'thread_local' ensures this is only done once per thread.
    thread_local bool thread_name_set = false;
    if (!thread_name_set) {
        System::setThisThreadName("sdl-audio");
        thread_name_set = true;
    }

    Audio *self = static_cast<Audio *>(userdata);
    size_t bytes_copied = 0;

    {
        std::unique_lock<std::mutex> lock(self->m_buffer_mutex);
        // Wait for data to become available in the buffer
        self->m_buffer_cv.wait(lock, [self] {
            return !self->m_buffer.empty() || !self->m_active;
        });

        size_t bytes_to_copy = len;
        size_t bytes_available = self->m_buffer.size() * sizeof(int16_t);
        bytes_copied = std::min(bytes_to_copy, bytes_available);

        if (bytes_copied > 0) {
            memcpy(buf, self->m_buffer.data(), bytes_copied);
            size_t samples_copied = bytes_copied / sizeof(int16_t);
            self->m_buffer.erase(self->m_buffer.begin(), self->m_buffer.begin() + samples_copied);
        }
    }
    self->m_buffer_cv.notify_one(); // Notify decoder thread that there is space

    // Fill remaining buffer with silence if we couldn't provide enough data
    if (bytes_copied < len) {
        SDL_memset(buf + bytes_copied, 0, len - bytes_copied);
    }

    // Perform FFT on the data we are sending to the sound card
    if (bytes_copied > 0 && !Player::guiRunning) {
        std::lock_guard<std::mutex> lock(*self->m_player_mutex);
        toFloat(self->m_channels, (int16_t *)buf, self->m_fft->getTimeDom());
        self->m_fft->doFFT();
    }
}

void Audio::toFloat(int channels, int16_t *in, float *out) {
    if(channels == 1)
        for(int x = 0; x < 512; x++)
            out[x] = in[x] / 32768.0f;
    else if (channels == 2)
        for(int x = 0; x < 512; x++)
            out[x] = ((long long) in[x * 2] + in[(x * 2) + 1]) / 65536.0f;
}
