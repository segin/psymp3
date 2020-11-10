/*
 * audio.h - Audio class header
 * This also wraps SDL_gfx for primitives.
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

class Audio
{
    public:
        Audio(struct atdata *data);
        ~Audio();
        void play(bool go);
        static void callback(void *data, Uint8 *buf, int len);
        static void toFloat(int channels, int16_t *in, float *out); // 512 samples of stereo
        bool isPlaying() { return m_playing; };
        int getRate() { return m_rate; };
        int getChannels() { return m_channels; };
        void reopen(struct atdata *data);
        void lock(void);
        void unlock(void);
    protected:
    private:
        void setup(struct atdata *data);
        Stream *m_stream;
        bool m_playing;
        int  m_rate;
        int  m_channels;
};

#endif // AUDIO_H
