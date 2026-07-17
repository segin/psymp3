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
        struct Entry {
            TagLib::String path;
            TagLib::String artist;
            TagLib::String title;
            long duration = 0;
        };

        // Immutable snapshot of a track's metadata, copied out under the lock.
        // Returned by value so callers never hold a pointer into the tracks
        // vector, which the populator thread can reallocate via emplace_back.
        struct TrackInfo {
            TagLib::String artist;
            TagLib::String title;
            TagLib::String album;
            TagLib::String path;
            unsigned int length_seconds = 0;
        };

        // Explicitly delete copy operations as std::vector<track> is non-copyable
        Playlist(const Playlist&) = delete;
        Playlist& operator=(const Playlist&) = delete; // Keep copy assignment deleted
        // Move operations are implicitly deleted due to std::recursive_mutex
        // Playlist(Playlist&&) = default; // Removed: Cannot be defaulted due to non-movable member
        // Playlist& operator=(Playlist&&) = default; // Removed: Cannot be defaulted due to non-movable member
        Playlist();
        bool addFile(TagLib::String path, TagLib::String artist, TagLib::String title, long duration);
        bool addEntry(const Entry& entry);
        // Insert entries at the given index, shifting existing tracks (and the
        // current-position cursor and shuffle order) down. Used by the "I" key.
        bool insertEntries(long position, const std::vector<Entry>& entries);
        // Remove every track and reset the position/shuffle state to empty.
        void clear();
        // Remove the track at the given index, keeping the position cursor on the
        // same logical track (or the one that shifts into its slot). Returns false
        // if the index is out of range.
        bool removeTrack(long index);
        // Move the track at `from` to index `to`, shifting the tracks in between
        // and keeping the position cursor on the same logical track. Returns false
        // for out-of-range indices or a no-op move.
        bool moveTrack(long from, long to);
        ~Playlist() = default;
        bool addFile(TagLib::String path);
        long getPosition() const;
        long entries() const;
        bool setPosition(long position);
        TagLib::String setPositionAndJump(long position);
        TagLib::String getTrack(long position) const;
        TagLib::String next();
        TagLib::String prev();
        TagLib::String peekNext() const;
        std::optional<TrackInfo> getTrackInfo(long position) const;
        // True if advancing forward by advance_count would run past the end of
        // the current play order (sequential or shuffle) and wrap. Used to honor
        // LoopMode::None in both orders.
        bool advanceWouldWrap(size_t advance_count) const;
        void setShuffle(bool enabled);
        bool isShuffle() const;
        static std::vector<Entry> loadPlaylistEntries(TagLib::String path);
        static std::vector<Entry> resolveInlineSources(const std::vector<TagLib::String>& sources);
        static std::unique_ptr<Playlist> loadPlaylist(TagLib::String path);
        void savePlaylist(TagLib::String path);
        // Monotonic counter bumped on every change to the track list (add / insert
        // / remove / move / clear). Lets an observer (the Playlist Manager window)
        // cheaply detect external edits and refresh.
        uint64_t generation() const { return m_generation.load(std::memory_order_relaxed); }
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

        std::atomic<uint64_t> m_generation{0}; // bumped on track-list changes

        mutable std::recursive_mutex m_mutex;
};

#endif // PLAYLIST_H
