/*
 * playlist.h - Playlist class header
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

#ifndef PLAYLIST_H
#define PLAYLIST_H

class Playlist
{
    public:
        Playlist(std::vector<std::string> args);
        Playlist(TagLib::String playlist);
        ~Playlist();
        bool addFile(TagLib::String path);
        void parseArgs(std::vector<std::string> args);
    protected:
    private:
    std::vector<track> tracks;
};

#endif // PLAYLIST_H