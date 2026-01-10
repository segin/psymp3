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

// No direct includes - all includes should be in psymp3.h

#ifdef HAVE_DBUS
namespace PsyMP3 {
namespace MPRIS {
    class MPRISManager;
}
}
#endif

// A struct to hold options parsed from the command line.
// This provides a clean way to pass configuration into the Player class.
struct PlayerOptions {
    int scalefactor = 2;
    float decayfactor = 1.0f;
    FFTMode fft_mode = FFTMode::Original;
    bool automated_test_mode = false;
    bool unattended_quit = false;
    std::vector<std::string> files;
};

struct atdata {
    Stream *stream;
    FastFourier *fft;
    std::mutex *mutex;
};

/* PsyMP3 main class! 
 * 
 * The Player class is the central coordinator for PsyMP3, managing audio playback,
 * user interface, and external integrations including MPRIS (Media Player Remote
 * Interfacing Specification) for desktop media control integration.
 * 
 * MPRIS Integration:
 * - Automatically initializes MPRIS support when D-Bus is available
 * - Provides graceful degradation when D-Bus is unavailable
 * - Updates MPRIS state on playback changes (play, pause, stop, seek)
 * - Exposes metadata and position information to desktop media controls
 * - Thread-safe integration following the public/private lock pattern
 * 
 * The MPRIS integration is conditionally compiled based on HAVE_DBUS and will
 * not affect functionality when D-Bus support is not available.
 */

class Player
{
    public:
        friend class System;
#ifdef HAVE_DBUS
        friend class PsyMP3::MPRIS::MPRISManager;
#endif
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
        void playlistPopulatorLoop(std::vector<std::string> args);

        void nextTrack(size_t advance_count = 1);
        void prevTrack(void);
        bool stop(void);
        bool pause(void);
        bool play(void);
        bool playPause(void);
        void openTrack(TagLib::String path);
        void seekTo(unsigned long pos);
        static std::atomic<bool> guiRunning;
        
        // Robust playlist handling
        bool handleUnplayableTrack();
        bool findFirstPlayableTrack();
        
        // Last.fm scrobbling helpers
        void checkScrobbling();
        void startTrackScrobbling();
        void submitNowPlaying(); 

        void setVolume(double volume);
        double getVolume() const;

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
        
        // Window management methods
        void renderWindows();
        void handleWindowMouseEvents(const SDL_Event& event);
        void toggleTestWindowH();
        void toggleTestWindowB();
        void createRandomWindows();

        std::unique_ptr<Display> screen;
        std::unique_ptr<Surface> graph;
        std::unique_ptr<Playlist> playlist;
        std::unique_ptr<Font> font;
        std::unique_ptr<Font> m_large_font;
        
        Stream* stream = nullptr;
        std::unique_ptr<Stream> m_next_stream; // Slot for the pre-loaded next track
        size_t m_num_tracks_in_next_stream = 0; // How many playlist entries the next stream represents
        size_t m_num_tracks_in_current_stream = 0; // How many playlist entries the current stream represents

        std::unique_ptr<std::mutex> mutex;
        std::unique_ptr<FastFourier> fft;
        std::unique_ptr<Audio> audio;
        std::unique_ptr<System> system;
#ifdef HAVE_DBUS
        std::unique_ptr<PsyMP3::MPRIS::MPRISManager> m_mpris_manager;  // MPRIS integration for desktop media controls
#endif
        struct atdata ATdata;
        int scalefactor = 2;
        float decayfactor = 1.0f;
        
        // Last.fm scrobbling
        std::unique_ptr<LastFM> m_lastfm;
        Uint32 m_track_start_time = 0;  // SDL ticks when current track started
        bool m_track_scrobbled = false; // Flag to ensure we only scrobble once per track
        
        // For progress bar dragging
        bool m_is_dragging = false;
        Uint32 m_drag_start_time = 0;
        Uint16 m_drag_start_x = 0;
        unsigned long m_drag_position_ms = 0;
        
        // For keyboard seeking
        uint8_t m_seek_direction = 0;
        unsigned long m_seek_position_ms = 0;

        // UI Widget tree
        Widget* m_ui_root; // Reference to ApplicationWidget singleton
        std::map<std::string, Label*> m_labels; // Non-owning pointers for quick access (owned by ApplicationWidget)
        
        // Spectrum analyzer widget (non-owning pointer - owned by ApplicationWidget)
        SpectrumAnalyzerWidget* m_spectrum_widget;
        
        // Progress bar widget (non-owning pointer - owned by progress frame widget which is owned by ApplicationWidget)
        PlayerProgressBarWidget* m_progress_widget;

        // Overlay widgets
        std::unique_ptr<ToastNotification> m_toast;
        // Toast queue for smooth replacement transitions
        struct PendingToast {
            std::string message;
            Uint32 duration_ms;
        };
        std::queue<PendingToast> m_toast_queue;
        static constexpr size_t MAX_TOAST_QUEUE_SIZE = 10;
        std::unique_ptr<LyricsWidget> m_lyrics_widget;
        std::unique_ptr<Label> m_pause_indicator;
        FadingWidget* m_seek_left_indicator = nullptr;
        FadingWidget* m_seek_right_indicator = nullptr;

        // Loader thread members
        std::thread m_loader_thread;
        std::atomic<bool> m_loader_active;
        std::queue<TrackLoadRequest> m_loader_queue;
        std::mutex m_loader_queue_mutex;
        std::condition_variable m_loader_queue_cv;
        std::atomic<bool> m_loading_track;
        std::atomic<bool> m_preloading_track;
        std::thread m_playlist_populator_thread;
        int m_navigation_direction = 1;
        int m_skip_attempts = 0;
        LoopMode m_loop_mode;
        std::vector<Uint32> m_spectrum_colors;
        bool m_use_widget_mouse_handling = true;
        float m_volume = 1.0f;

        // Automated testing members
        bool m_automated_test_mode;
        bool m_unattended_quit;
        int m_automated_test_track_count;
        SDL_TimerID m_automated_test_timer_id = 0;
        SDL_TimerID m_automated_quit_timer_id = 0;
        
        // Test windows
        std::unique_ptr<WindowFrameWidget> m_test_window_h;
        std::unique_ptr<WindowFrameWidget> m_test_window_b;
        std::vector<std::unique_ptr<WindowFrameWidget>> m_random_windows;
        int m_random_window_counter = 0;
};

#endif // PLAYER_H