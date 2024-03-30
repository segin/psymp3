/*
 * audio.cpp - Audio class implementation
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
    SDL_CloseAudio();
}

void Audio::setup(struct atdata *data)
{
    SDL_AudioSpec fmt;
    fmt.freq = m_rate = m_stream->getRate();
    fmt.format = AUDIO_S16; /* Always, I hope */
    fmt.channels = m_channels = m_stream->getChannels();
    std::cout << "Audio::setup: channels: " << m_stream->getChannels() << std::endl;
    fmt.samples = 512 * fmt.channels; /* 512 samples for fft */
    fmt.callback = callback;
    fmt.userdata = (void *) data;
    if ( SDL_OpenAudio(&fmt, nullptr) < 0 ) {
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

/* Reopen due to format change across tracks. */
void Audio::reopen(struct atdata *data)
{
    m_stream = data->stream;
    std::cout << "Audio::reopen(): " << std::dec << m_stream->getRate() << "Hz, channels: " << std::dec << m_stream->getChannels() << std::endl;
    SDL_CloseAudio();
    setup(data);
}

void Audio::lock(void)
{
    SDL_LockAudio();
}

void Audio::unlock(void)
{
    SDL_UnlockAudio();
}

/* Actually push the audio to the soundcard.
 * Audio is summed to mono (if stereo) and then FFT'd.
 */
void Audio::callback(void *data, Uint8 *buf, int len)
{
    struct atdata *ldata = static_cast<struct atdata *>(data);
    Stream *stream = ldata->stream;
    FastFourier *fft = ldata->fft;
    Mutex *mutex = ldata->mutex;
#ifdef _AUDIO_DEBUG
    std::cout << "callback: len " << std::dec << len << std::endl;
#endif
   // mutex->lock();
    stream->getData(len, (void *) buf);
    if(!Player::guiRunning) {
        toFloat(stream->getChannels(), (int16_t *) buf, fft->getTimeDom());
        fft->doFFT();
    }
   // mutex->unlock();
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
