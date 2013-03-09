/*
 * audio.h - Audio class header
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
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
    protected:
    private:
        void setup(struct atdata *data);
        Stream *m_stream;
        bool m_playing;
};

#endif // AUDIO_H
