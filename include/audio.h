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
    Audio(std::unique_ptr<Stream> stream_to_own, FastFourier *fft, std::mutex *player_mutex);
    ~Audio();

    void play(bool go);
    void lock(void);
    void unlock(void);
    bool isFinished() const;
    std::unique_ptr<Stream> setStream(std::unique_ptr<Stream> new_stream);

    int getRate() const { return m_rate; }
    int getChannels() const { return m_channels; }
    Stream* getCurrentStream() const { return m_current_stream_raw_ptr.load(); }
    uint64_t getBufferLatencyMs() const;
    void resetBuffer();
    uint64_t getSamplesPlayed() const;
    void setSamplesPlayed(uint64_t samples);

private:
    void setup();
    static void callback(void *userdata, Uint8 *buf, int len);
    static void toFloat(int channels, int16_t *in, float *out);

    // Private unlocked versions of public methods (assumes locks are already held)
    // Lock acquisition order: m_stream_mutex before m_buffer_mutex
    // These methods should be used when calling from within already-locked contexts
    // to prevent deadlocks and improve performance
    bool isFinished_unlocked() const;
    std::unique_ptr<Stream> setStream_unlocked(std::unique_ptr<Stream> new_stream);
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

    std::unique_ptr<Stream> m_owned_stream; // The stream currently owned by this Audio object
    std::atomic<Stream*> m_current_stream_raw_ptr; // Raw pointer for atomic access by audio callback
    FastFourier *m_fft;
    std::mutex *m_player_mutex; // The mutex from the player for FFT data
    
    int m_rate;
    int m_channels;
    bool m_playing;
    std::atomic<uint64_t> m_samples_played{0};
    std::atomic<bool> m_stream_eof{false};
};

#endif // AUDIO_H