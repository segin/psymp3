/*
 * player.h - player singleton class header.
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

#ifndef PLAYER_H
#define PLAYER_H

struct atdata {
    Stream *stream;
    FastFourier *fft;
    std::mutex *mutex;
};

/* PsyMP3 main class! */

class Player
{
    public:
        friend class System;
        Player();
        ~Player();
        void Run(std::vector<std::string> args);
        static float logarithmicScale(const int f, float x);
        static Uint32 AppLoopTimer(Uint32 interval, void* param);
        /* SDL event synthesis */
        static void synthesizeUserEvent(int uevent, void *data1, void *data2);
        static void synthesizeKeyEvent(SDLKey kpress);
        /* state management */

        // Data structure for asynchronous track loading results
        struct TrackLoadResult {
            Stream* stream;
            TagLib::String error_message;
        };
        // Asynchronous track loading
        void requestTrackLoad(TagLib::String path);
        void loaderThreadLoop();

        bool nextTrack(void);
        bool prevTrack(void);
        bool stop(void);
        bool pause(void);
        bool play(void);
        bool playPause(void);
        void openTrack(TagLib::String path);
        void seekTo(unsigned long pos);
        static bool guiRunning; 
    protected:
        PlayerState state;
        void renderSpectrum(Surface *graph);
        void precomputeSpectrumColors();
    private:
        bool updateGUI();
        bool handleKeyPress(const SDL_keysym& keysym);
        void handleMouseButtonDown(const SDL_MouseButtonEvent& event);
        void handleMouseMotion(const SDL_MouseMotionEvent& event);
        void handleMouseButtonUp(const SDL_MouseButtonEvent& event);
        bool handleUserEvent(const SDL_UserEvent& event);
        void handleKeyUp(const SDL_keysym& keysym);
        void updateInfo(void);

        uint8_t m_seek_direction = 0;
        std::unique_ptr<Display> screen;
        std::unique_ptr<Surface> graph;
        std::unique_ptr<Playlist> playlist;
        std::unique_ptr<Font> font;
        std::unique_ptr<Stream> stream;
        std::unique_ptr<Audio> audio;
        std::unique_ptr<FastFourier> fft;
        std::unique_ptr<std::mutex> mutex;
        std::unique_ptr<System> system;
        struct atdata ATdata;
        int scalefactor = 2;
        float decayfactor = 1.0f;
        // For progress bar dragging
        bool m_is_dragging = false;
        Uint32 m_last_seek_time = 0;
        Uint32 m_drag_start_time = 0;
        Uint16 m_drag_start_x = 0;
        unsigned long m_drag_position_ms = 0;

        // UI Widget tree
        std::unique_ptr<Widget> m_ui_root;
        Label* m_artist_label = nullptr; // Non-owning pointers for quick access
        Label* m_title_label = nullptr;
        Label* m_album_label = nullptr;
        Label* m_playlist_label = nullptr;
        Label* m_position_label = nullptr;
        Label* m_scale_label = nullptr;
        Label* m_decay_label = nullptr;
        Label* m_fft_mode_label = nullptr;

        // Asynchronous loader thread members
        std::thread m_loader_thread;
        std::mutex m_loader_queue_mutex;
        std::condition_variable m_loader_queue_cv;
        std::queue<TagLib::String> m_loader_queue; // Queue of paths to load
        std::atomic<bool> m_loader_active;
        std::atomic<bool> m_loading_track; // Flag to prevent multiple simultaneous loads

        // New thread for populating playlist from command line
        std::thread m_playlist_populator_thread;
        void playlistPopulatorLoop(std::vector<std::string> args);

        // Precomputed colors for spectrum analyzer
        std::vector<uint32_t> m_spectrum_colors;
};

#endif // PLAYER_H
