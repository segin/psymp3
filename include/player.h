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

#include "font.h"

#include "font.h"

class MPRIS;

// A struct to hold options parsed from the command line.
// This provides a clean way to pass configuration into the Player class.
struct PlayerOptions {
    int scalefactor = 2;
    float decayfactor = 1.0f;
    FFTMode fft_mode = FFTMode::Original;
    bool automated_test_mode = false;
    std::vector<std::string> files;
};

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
        friend class MPRIS;
        Player();
        ~Player();
        void Run(const PlayerOptions& options);
        static Uint32 AppLoopTimer(Uint32 interval, void* param);
        static Uint32 AutomatedTestTimer(Uint32 interval, void* param);
        static Uint32 AutomatedQuitTimer(Uint32 interval, void* param);
        /* SDL event synthesis */
        static void synthesizeUserEvent(int uevent, void *data1, void *data2);
        static void synthesizeKeyEvent(SDLKey kpress);
        /* state management */

        // Data structure for asynchronous track loading results
        enum class LoadRequestType {
            PlayNow,        // Standard load and play
            Preload,        // Load but don't play
            PreloadChained  // Load a chain but don't play
        };

        struct TrackLoadRequest {
            LoadRequestType type;
            TagLib::String path; // For PlayNow and Preload
            std::vector<TagLib::String> paths; // For PreloadChained
        };

        struct TrackLoadResult {
            LoadRequestType request_type; // What kind of request was this?
            Stream* stream;
            TagLib::String error_message;
            size_t num_chained_tracks; // How many tracks are in this stream (for playlist advancement)
        };

        // Asynchronous track loading
        void requestTrackLoad(TagLib::String path);
        void requestTrackPreload(const TagLib::String& path);
        void requestChainedStreamLoad(const std::vector<TagLib::String>& paths);
        void loaderThreadLoop();

        void nextTrack(size_t advance_count = 1);
        void prevTrack(void);
        bool stop(void);
        bool pause(void);
        bool play(void);
        bool playPause(void);
        void openTrack(TagLib::String path);
        void seekTo(unsigned long pos);
        static bool guiRunning;
        
        // Robust playlist handling
        bool handleUnplayableTrack();
        bool findFirstPlayableTrack(); 
    protected:
        PlayerState state;
        PlayerState m_state_before_seek;
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
        void showToast(const std::string& message, Uint32 duration_ms = 2000);
        void updateInfo(bool is_loading = false, const TagLib::String& error_msg = "");

        uint8_t m_seek_direction = 0;
        std::unique_ptr<Display> screen;
        std::unique_ptr<Surface> graph;
        std::unique_ptr<Playlist> playlist;
        std::unique_ptr<Font> font;
        std::unique_ptr<Font> m_large_font;
        
        Stream* stream;
        std::unique_ptr<Stream> m_next_stream; // Slot for the pre-loaded next track
        size_t m_num_tracks_in_next_stream = 0; // How many playlist entries the next stream represents
        size_t m_num_tracks_in_current_stream = 0; // How many playlist entries the current stream represents

        std::unique_ptr<Audio> audio;
        std::unique_ptr<FastFourier> fft;
        std::unique_ptr<std::mutex> mutex;
        std::unique_ptr<System> system;
        MPRIS* mpris;
        struct atdata ATdata;
        int scalefactor = 2;
        float decayfactor = 1.0f;
        // For progress bar dragging
        bool m_is_dragging = false;
        Uint32 m_drag_start_time = 0;
        Uint16 m_drag_start_x = 0;
        unsigned long m_drag_position_ms = 0;
        unsigned long m_seek_position_ms = 0; // For keyboard seeking

        // UI Widget tree
        std::unique_ptr<Widget> m_ui_root;
        std::map<std::string, Label*> m_labels; // Non-owning pointers for quick access

        // Overlay widgets
        std::unique_ptr<ToastNotification> m_toast;
        std::unique_ptr<Label> m_pause_indicator;
        std::unique_ptr<FadingWidget> m_seek_left_indicator;
        std::unique_ptr<FadingWidget> m_seek_right_indicator;
        LoopMode m_loop_mode;

        int m_navigation_direction = 1; // 1 for fwd, -1 for back
        int m_skip_attempts = 0;

        // Asynchronous loader thread members
        std::thread m_loader_thread;
        std::mutex m_loader_queue_mutex;
        std::condition_variable m_loader_queue_cv;
        std::queue<TrackLoadRequest> m_loader_queue; // Queue of load requests
        std::atomic<bool> m_loader_active;
        std::atomic<bool> m_loading_track; // Flag to prevent multiple simultaneous loads
        std::atomic<bool> m_preloading_track; // Flag to prevent multiple preloads
        bool m_automated_test_mode;
        int m_automated_test_track_count;
        SDL_TimerID m_automated_test_timer_id = 0;
        SDL_TimerID m_automated_quit_timer_id = 0;

        // New thread for populating playlist from command line
        std::thread m_playlist_populator_thread;
        void playlistPopulatorLoop(std::vector<std::string> args);

        // Precomputed colors for spectrum analyzer
        std::vector<uint32_t> m_spectrum_colors;
};

#endif // PLAYER_H
