/*
 * audio.cpp - Audio class implementation
 * This also wraps SDL_gfx for primitives.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "psymp3.h"

Audio::Audio(struct atdata *data)
{
    m_stream = data->stream;
    std::cout << "Audio::Audio(): " << std::dec << m_stream->getRate() << "Hz, channels: " << std::dec << m_stream->getChannels() << std::endl;
    setup(data);
}

Audio::~Audio()
{
    play(false);
}

void Audio::setup(struct atdata *data)
{
    SDL_AudioSpec fmt;
    fmt.freq = m_stream->getRate();
    fmt.format = AUDIO_S16; /* Always, I hope */
    fmt.channels = m_stream->getChannels();
    fmt.samples = 512 * fmt.channels * 2; /* 512 samples for fft */
    fmt.callback = callback;
    fmt.userdata = (void *) data;
    if ( SDL_OpenAudio(&fmt, (SDL_AudioSpec *) NULL) < 0 ) {
        std::cerr << "Unable to open audio: " << SDL_GetError() << std::endl;
        // throw;
    }
}

void Audio::play(bool go)
{
    m_playing = go;
    if (go)
        SDL_PauseAudio(0);
    else
        SDL_PauseAudio(1);
}

void Audio::callback(void *data, Uint8 *buf, int len)
{
    struct atdata *ldata = (struct atdata *) data;
    Stream *stream = ldata->stream;
    FastFourier *fft = ldata->fft;
    Mutex *mutex = ldata->mutex;
    std::cout << "callback: len " << std::dec << len << std::endl;
    mutex->lock();
    stream->getData(len, (void *) buf);
    toFloat(stream->getChannels(), (int16_t *) buf, fft->getTimeDom());
    fft->doFFT();
    mutex->unlock();
}

void Audio::toFloat(int channels, int16_t *in, float *out)
{
    if(channels == 1)
        for(int x = 0; x < 512; x++)
            out[x] = in[x] / 32768.0f;
    else if (channels == 2)
        for(int x = 0; x < 512; x++)
            out[x] = ((long long) in[x * 2] + in[(x * 2) + 1]) / 65536.0f;
}
