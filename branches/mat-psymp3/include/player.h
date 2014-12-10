/*
 * player.h - player singleton class header.
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
        std::map<std::string, Surface> info;
        struct atdata ATdata;
};

#endif // PLAYER_H
