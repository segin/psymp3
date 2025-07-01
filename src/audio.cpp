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

/**
 * @brief Constructs an Audio object.
 * @param data A pointer to a struct containing shared data from the Player,
 *             including the stream to play, the FFT object to populate, and
 *             the mutex for thread-safe access to the stream.
 * This constructor initializes the audio system for the given stream and starts the background decoder thread.
 */
Audio::Audio(std::unique_ptr<Stream> stream_to_own, FastFourier *fft, std::mutex *player_mutex)
    : m_owned_stream(std::move(stream_to_own)),
      m_current_stream_raw_ptr(m_owned_stream.get()),
      m_fft(fft),
      m_player_mutex(player_mutex),
      m_active(true),
      m_playing(false)
{
    if (!m_owned_stream) {
        throw std::invalid_argument("Audio constructor called with a null stream.");
    }
    std::cout << "Audio::Audio(): " << std::dec << m_owned_stream->getRate() << "Hz, channels: " << std::dec << m_owned_stream->getChannels() << std::endl;
    m_buffer.reserve(16384); // Reserve space for the buffer to avoid reallocations
    setup();
    m_decoder_thread = std::thread(&Audio::decoderThreadLoop, this);
}

/**
 * @brief Destroys the Audio object.
 * 
 * This stops playback, signals the decoder thread to terminate, waits for it to join,
 * and then closes the SDL audio device, ensuring a clean shutdown.
 */
Audio::~Audio() {
    play(false);
    if (m_active) {
        m_active = false;
        m_stream_cv.notify_all(); // Wake up if waiting for a new stream
        m_buffer_cv.notify_all(); // Wake up if waiting for buffer space
        if (m_decoder_thread.joinable()) {
            m_decoder_thread.join();
        }
    }
    SDL_CloseAudio();
}

/**
 * @brief Configures and opens the SDL audio device.
 * 
 * It sets up the audio format (sample rate, channels, etc.) based on the properties
 * of the current stream and registers the static `callback` function to be called by SDL.
 */
void Audio::setup() {
    SDL_AudioSpec fmt;
    // Use m_current_stream_raw_ptr to get rate and channels
    fmt.freq = m_rate = m_current_stream_raw_ptr.load()->getRate();
    fmt.format = AUDIO_S16; /* Always, I hope */
    fmt.channels = m_channels = m_current_stream_raw_ptr.load()->getChannels();
    std::cout << "Audio::setup: channels: " << m_current_stream_raw_ptr.load()->getChannels() << std::endl;
    fmt.samples = 512 * fmt.channels; /* 512 samples for fft */
    fmt.callback = callback;
    fmt.userdata = this;
    if ( SDL_OpenAudio(&fmt, nullptr) < 0 ) {
        std::cerr << "Unable to open audio: " << SDL_GetError() << std::endl;
        // throw;
    }
}

/**
 * @brief Starts or pauses audio playback.
 * @param go `true` to start/resume playback, `false` to pause.
 */
void Audio::play(bool go) {
    m_playing = go;
    SDL_PauseAudio(go ? 0 : 1);
}

/**
 * @brief Sets a new stream for the decoder thread to process.
 * This method is thread-safe and is used to seamlessly switch tracks.
 * @param new_stream A pointer to the new Stream object.
 */
std::unique_ptr<Stream> Audio::setStream(std::unique_ptr<Stream> new_stream)
{
    std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);

    m_buffer.clear();
    m_owned_stream = std::move(new_stream);
    m_current_stream_raw_ptr.store(m_owned_stream.get());

    // Notify the decoder thread that a new stream is available.
    m_stream_cv.notify_one();
    return nullptr; // Ownership transferred, return null unique_ptr
}

/**
 * @brief Checks if the audio playback for the current stream is completely finished.
 *
 * This method is the definitive way to determine if a track has ended. It's considered
 * finished only when the decoder has no more data to provide AND the internal playback
 * buffer has been fully consumed by the audio device.
 *
 * @return `true` if the stream is decoded and the buffer is empty, `false` otherwise.
 */
bool Audio::isFinished() const
{
    std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    // It's finished if the decoder is done with the stream (m_stream is null)
    // AND the playback buffer is empty.
    return m_current_stream_raw_ptr.load() == nullptr && m_buffer.empty();
}

/**
 * @brief Locks the audio device using the SDL_LockAudio function.
 * @deprecated This is a legacy function. Modern thread safety is handled by std::mutex.
 */
void Audio::lock(void) {
    SDL_LockAudio();
}

/**
 * @brief Unlocks the audio device using the SDL_UnlockAudio function.
 * @deprecated This is a legacy function. Modern thread safety is handled by std::mutex.
 */
void Audio::unlock(void) {
    SDL_UnlockAudio();
}

/**
 * @brief The main loop for the background audio decoder thread.
 * 
 * This function runs in a separate thread and is responsible for continuously
 * decoding audio data from the source stream and placing it into a thread-safe
 * buffer. It waits if the buffer is full and is woken up by the audio callback
 * when more data is needed. This producer-consumer pattern decouples file I/O
 * and decoding from the real-time audio callback.
 */
void Audio::decoderThreadLoop() {
    System::setThisThreadName("audio-decoder");
    std::vector<int16_t> decode_chunk(4096); // Decode in 8KB chunks
    // The high water mark prevents the decoder from reading too far ahead,
    // which is important for responsive seeking and track changes. It defines
    // the maximum amount of decoded audio to keep in the buffer.
    constexpr size_t BUFFER_HIGH_WATER_MARK = 48000 * 2; // 1 sec of 48kHz stereo

    while (m_active)
    {
        Stream* local_stream = nullptr;
        // Wait until there is a valid stream to process, and get a safe local copy.
        {
            std::unique_lock<std::mutex> lock(m_stream_mutex);
            m_stream_cv.wait(lock, [this] {
                return m_owned_stream != nullptr || !m_active;
            });
            if (!m_active) break;
            local_stream = m_owned_stream.get();
        }

        // Inner loop: Decode from the current stream until it ends.
        // Use the local_stream pointer to avoid race conditions.
        while (local_stream && m_active) {
            {
                std::unique_lock<std::mutex> lock(m_buffer_mutex);
                m_buffer_cv.wait(lock, [this] {
                    return m_buffer.size() < BUFFER_HIGH_WATER_MARK || !m_active || !m_playing;
                });
            }

            if (!m_active) break;

            size_t bytes_read = 0;
            bool eof = false;
            {
                // Lock the player mutex to synchronize with main thread operations like seeking.
                std::lock_guard<std::mutex> lock(*m_player_mutex);
                // CRITICAL: Before using local_stream, verify it's still the active stream.
                // If not, the main thread has changed it, and we must exit this inner loop.
                if (local_stream == m_current_stream_raw_ptr.load()) {
                    bytes_read = local_stream->getData(decode_chunk.size() * sizeof(int16_t), decode_chunk.data());
                    eof = local_stream->eof();
                } else {
                    // Stream has changed. Break to re-evaluate in the outer loop.
                    break;
                }
            }

            if (bytes_read > 0) {
                std::lock_guard<std::mutex> lock(m_buffer_mutex);
                size_t samples_read = bytes_read / sizeof(int16_t);
                m_buffer.insert(m_buffer.end(), decode_chunk.begin(), decode_chunk.begin() + samples_read);
            }
            m_buffer_cv.notify_one();

            if (eof) {
                // This stream is finished. Signal this by setting the shared pointer to null.
                // This will cause isFinished() to return true (once the buffer is empty)
                // and will make this thread wait for a new stream.
                std::lock_guard<std::mutex> lock(m_stream_mutex);
                m_owned_stream.reset(); // Release ownership
                m_current_stream_raw_ptr.store(nullptr);
                break; // Exit the inner decoding loop.
            }
        }
    }
}

/**
 * @brief The static callback function invoked by SDL to request audio data.
 * 
 * This function is the heart of the audio playback. It runs on a high-priority
 * thread managed by SDL. Its primary job is to copy decoded audio data from the
 * internal buffer into the buffer provided by SDL. If not enough data is available,
 * it fills the remainder with silence. It also passes the audio data to the FFT
 * for processing before it's sent to the sound card.
 * 
 * @param userdata A pointer to the `Audio` instance.
 * @param buf A pointer to the hardware audio buffer to be filled.
 * @param len The length of the buffer in bytes.
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
            return !self->m_buffer.empty() || !self->m_active || !self->m_playing;
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

/**
 * @brief Converts 16-bit integer audio samples to floating-point samples for FFT processing.
 * 
 * This function normalizes the audio data to a range of [-1.0, 1.0]. If the input
 * is stereo, it sums the left and right channels to produce a mono signal, which is
 * what the FFT algorithm expects.
 * 
 * @param channels The number of channels in the input audio (1 for mono, 2 for stereo).
 * @param in A pointer to the buffer of 16-bit signed integer input samples.
 * @param out A pointer to the destination buffer for the converted float samples.
 */
void Audio::toFloat(int channels, int16_t *in, float *out) {
    if(channels == 1)
        for(int x = 0; x < 512; x++)
            out[x] = in[x] / 32768.0f;
    else if (channels == 2)
        for(int x = 0; x < 512; x++)
            out[x] = ((long long) in[x * 2] + in[(x * 2) + 1]) / 65536.0f;
}
