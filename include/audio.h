/*
 * audio.h - Audio class header
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

#ifndef AUDIO_H
#define AUDIO_H

class Audio {
public:
    Audio(std::unique_ptr<Stream> stream_to_own,
          FastFourier *fft,
          std::mutex *player_mutex,
          std::vector<int16_t> primed_samples = {},
          bool primed_eof = false);
    ~Audio();

    void play(bool go);
    bool isFinished() const;
    std::unique_ptr<Stream> setStream(std::unique_ptr<Stream> new_stream,
                                      std::vector<int16_t> primed_samples = {},
                                      bool primed_eof = false);

    int getRate() const { return m_rate; }
    int getChannels() const { return m_channels; }
    int getDeviceRate() const { return m_device_rate; }
    int getDeviceChannels() const { return m_device_channels; }
    Stream* getCurrentStream() const { return m_current_stream_raw_ptr.load(); }
    uint64_t getBufferLatencyMs() const;
    void resetBuffer();
    uint64_t getSamplesPlayed() const;
    void setSamplesPlayed(uint64_t samples);

    void setVolume(float volume);
    float getVolume() const;

    std::mutex& getFFTMutex() const { return m_fft_mutex; }

private:
    void setup();
    static void callback(void *userdata, Uint8 *buf, int len);
    static void toFloat(int channels, int16_t *in, float *out);
    static std::pair<std::vector<int16_t>, bool> primeStream(Stream* stream, size_t max_samples);

    // Private unlocked versions of public methods (assumes locks are already held)
    // Lock acquisition order: m_stream_mutex before m_buffer_mutex
    // These methods should be used when calling from within already-locked contexts
    // to prevent deadlocks and improve performance
    bool isFinished_unlocked() const;
    std::unique_ptr<Stream> setStream_unlocked(std::unique_ptr<Stream> new_stream,
                                               std::vector<int16_t> primed_samples,
                                               bool primed_eof);
    void resetBuffer_unlocked();
    uint64_t getBufferLatencyMs_unlocked() const;

    // Decoder thread and buffer
    void decoderThreadLoop();
    std::thread m_decoder_thread;
    std::vector<int16_t> m_buffer;
    mutable std::mutex m_buffer_mutex;
    mutable std::mutex m_stream_mutex;
    std::condition_variable m_stream_cv;
    std::condition_variable m_buffer_cv;
    std::atomic<bool> m_active;

    std::shared_ptr<Stream> m_owned_stream; // Shared so the decoder thread can keep the current stream alive safely
    std::atomic<Stream*> m_current_stream_raw_ptr; // Raw pointer for atomic access by audio callback
    FastFourier *m_fft;
    mutable std::mutex m_fft_mutex;
    std::mutex *m_player_mutex; // The mutex from the player for general state
    
    int m_rate = 0;     // 0 until setup() succeeds; getRate()/divisors must tolerate it
    int m_channels = 0; // 0 until setup() succeeds
    int m_device_rate = 0;
    int m_device_channels = 0;
    SDL_AudioDeviceID m_device_id = 0;
    std::atomic<float> m_volume{1.0f};
    std::atomic<bool> m_playing;
    std::atomic<uint64_t> m_samples_played{0};
    std::atomic<bool> m_stream_eof{false};
};

#endif // AUDIO_H
