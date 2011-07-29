/*
 * track.h - class header for track class
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

#ifndef TRACK_H
#define TRACK_H

class track
{
    public:
        track();
        track(TagLib::String a_Artist, TagLib::String a_Title, TagLib::String a_Album, TagLib::String a_FilePath, unsigned int a_Len);
        track(TagLib::String a_FilePath);
        virtual ~track();
        track& operator=(const track& rhs);
        TagLib::String GetArtist() { return m_Artist; }
        void SetArtist(TagLib::String val) { m_Artist = val; }
        TagLib::String GetTitle() { return m_Title; }
        void SetTitle(TagLib::String val) { m_Title = val; }
        TagLib::String GetAlbum() { return m_Album; }
        void SetAlbum(TagLib::String val) { m_Album = val; }
        TagLib::String GetFilePath() { return m_FilePath; }
        void SetFilePath(TagLib::String val) { m_FilePath = val; }
        unsigned int GetLen() { return m_Len; }
        void SetLen(unsigned int val) { m_Len = val; }
    protected:
    private:
        TagLib::String m_Artist;
        TagLib::String m_Title;
        TagLib::String m_Album;
        TagLib::String m_FilePath;
        unsigned int m_Len;
};

#endif // TRACK_H
