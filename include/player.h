/*
 * player.h - player singleton class header.
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

#ifndef PLAYER_H
#define PLAYER_H

struct atdata {
    Stream *stream;
    FastFourier *fft;
    Mutex *mutex;
};

/* PsyMP3 main class! */

class Player
{
    public:
        Player();
        ~Player();
        void Run(std::vector<std::string> args);
        static Uint32 AppLoopTimer(Uint32 interval, void* param);
        /* SDL event synthesis */
        static void synthesizeUserEvent(int uevent, void *data1, void *data2);
        static void synthesizeKeyEvent(SDLKey kpress);
        /* state management */
        bool nextTrack(void);
        bool prevTrack(void);
        bool stop(void);
        bool pause(void);
        bool play(void);
        bool playPause(void);
        void openTrack(TagLib::String path);
    protected:
        int state;
        void renderSpectrum(Surface *graph);
    private:
        void updateInfo(void);

        Display *screen;
        Surface *graph;
        Playlist *playlist;
        Font *font;
        Stream *stream;
        Audio *audio;
        FastFourier *fft;
        Mutex *mutex;
        System *system;
        std::map<std::string, Widget> info;
        struct atdata ATdata;
};

#endif // PLAYER_H
