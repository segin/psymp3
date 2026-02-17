/*
 * playlist.h - Playlist class header
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
#ifndef PLAYLIST_H
#define PLAYLIST_H

class Playlist
{
    public:
        // Explicitly delete copy operations as std::vector<track> is non-copyable
        Playlist(const Playlist&) = delete;
        Playlist& operator=(const Playlist&) = delete; // Keep copy assignment deleted
        // Move operations are implicitly deleted due to std::recursive_mutex
        // Playlist(Playlist&&) = default; // Removed: Cannot be defaulted due to non-movable member
        // Playlist& operator=(Playlist&&) = default; // Removed: Cannot be defaulted due to non-movable member
        Playlist();
        bool addFile(TagLib::String path, TagLib::String artist, TagLib::String title, long duration);
        ~Playlist();
        bool addFile(TagLib::String path);
        long getPosition();
        long entries();
        bool setPosition(long position);
        TagLib::String setPositionAndJump(long position);
        TagLib::String getTrack(long position) const;
        TagLib::String next();
        TagLib::String prev();
        TagLib::String peekNext() const;
        const track* getTrackInfo(long position) const;
        void setShuffle(bool enabled);
        bool isShuffle() const;
        static std::unique_ptr<Playlist> loadPlaylist(TagLib::String path);
        void savePlaylist(TagLib::String path);
    protected:
    private:
        // Private unlocked versions of public methods (assumes locks are already held)
        // These methods should be used when calling from within already-locked contexts
        // to prevent deadlocks and improve performance
        bool setPosition_unlocked(long position);
        TagLib::String getTrack_unlocked(long position) const;
        void repopulateShuffleIndices();
        
        std::vector<track> tracks;
        long m_position = 0;

        // Shuffle support
        bool m_shuffle = false;
        std::vector<long> m_shuffled_indices;
        long m_shuffle_index = 0;

        mutable std::recursive_mutex m_mutex;
};

#endif // PLAYLIST_H
