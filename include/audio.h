/*
 * audio.h - Audio class header
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
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
    Audio(struct atdata *data);
    ~Audio();

    void play(bool go);
    void lock(void);
    void unlock(void);

    int getRate() const { return m_rate; }
    int getChannels() const { return m_channels; }

private:
    void setup();
    static void callback(void *userdata, Uint8 *buf, int len);
    static void toFloat(int channels, int16_t *in, float *out);

    // Decoder thread and buffer
    void decoderThreadLoop();
    std::thread m_decoder_thread;
    std::vector<int16_t> m_buffer;
    std::mutex m_buffer_mutex;
    std::condition_variable m_buffer_cv;
    std::atomic<bool> m_active;

    Stream *m_stream;
    FastFourier *m_fft;
    std::mutex *m_player_mutex; // The mutex from the player for FFT data
    
    int m_rate;
    int m_channels;
    bool m_playing;
};

#endif // AUDIO_H