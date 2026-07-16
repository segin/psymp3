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

#include <random>

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
    bool show_mpris_errors = true;
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
        // Returns true if the event was queued; false if SDL dropped it (e.g.
        // queue full). Callers that pass a heap-owned payload must check this.
        static bool synthesizeUserEvent(int uevent, void *data1, void *data2);
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
            std::vector<int16_t> primed_samples;
            bool primed_eof = false;
        };

        // Asynchronous track loading
        void requestTrackLoad(TagLib::String path);
        void requestTrackPreload(const TagLib::String& path);
        void requestChainedStreamLoad(const std::vector<TagLib::String>& paths);
        void loaderThreadLoop();
        void playlistPopulatorLoop(const std::vector<std::string>& args);

        void nextTrack(size_t advance_count = 1);
        void prevTrack(void);
        bool canGoNext() const;
        bool canGoPrevious() const;
        bool stop(void);
        bool pause(void);
        bool play(void);
        bool playPause(void);
        void setLoopMode(LoopMode mode);
        LoopMode getLoopMode() const;
        void openTrack(TagLib::String path);
        void seekTo(unsigned long pos);
        bool canSeek() const;
        static std::atomic<bool> guiRunning;
        // Set while a modal native file chooser is up. The dialog blocks the main
        // thread outside updateGUI(), so guiRunning stays false and cannot stop
        // AppLoopTimer from backlogging RUN_GUI_ITERATION events; this flag does.
        static std::atomic<bool> dialogOpen;
        // App-loop timer period in ms, returned by AppLoopTimer so the redraw
        // cadence (target FPS) can be changed live. 33ms ~= 30 FPS by default.
        static std::atomic<Uint32> s_app_loop_interval_ms;
        
        // MPRIS Error Notification
        void toggleMPRISErrorNotifications();

        enum class NotificationType {
            Info,
            Warning,
            Error,
            MPRISError
        };

        void showNotification(const std::string& message, NotificationType type);

        // Robust playlist handling
        bool handleUnplayableTrack();
        bool findFirstPlayableTrack();
        
        // Last.fm scrobbling helpers
        void checkScrobbling();
        void startTrackScrobbling();
        void submitNowPlaying(); 

        void setVolume(double volume);
        double getVolume() const;

        void setShuffle(bool shuffle);
        bool getShuffle() const;

        // Named player actions shared by the keyboard shortcuts and the menu
        // bar. Each corresponds to a former inline case in handleKeyPress; the
        // menu invokes these directly rather than injecting synthetic key events.
        void volumeUp();                    // +5% (Up / Playback>Volume Up)
        void volumeDown();                  // -5% (Down / Playback>Volume Down)
        void setIntensity(int factor);      // spectrum scale factor (1-4 keys)
        void setDelay(float factor);        // spectrum decay factor (Z/X/C)
        void setFFTMode(FFTMode mode);      // set + toast + refresh
        void setTargetFps(int fps);         // redraw cadence: 30/60/120 + persist
        void cycleFFTMode();                // F key: advance to the next mode
        void cycleLoopMode();               // E key: None -> One -> All -> None
        void toggleZoom();                  // G key: 1x <-> 2x pixel doubling

        // --- Playlist Manager (Shift+P test window) backing operations ---
        // These are public because the window's client widget (defined outside
        // the Player class) drives them.
        // Display labels ("artist - title", or the file's basename) for every
        // playlist entry, in playlist order.
        std::vector<TagLib::String> playlistManagerLabels() const;
        // The playlist's current position, or -1 when the playlist is empty.
        long playlistManagerCurrentIndex() const;
        // Remove / reorder a playlist entry and refresh the "Playlist n/N" label.
        void playlistManagerRemove(long index);
        void playlistManagerMove(long from, long to);
        // Open the file chooser and queue the chosen track(s) next / at the end.
        // No-ops with a toast when built without native file-dialog support.
        void playlistManagerAddNext();
        void playlistManagerAddEnd();

    protected:
        PlayerState state;
        PlayerState m_state_before_seek;
        void precomputeSpectrumColors();
    private:

        void updateState(Stream*& current_stream, unsigned long& current_pos_ms, unsigned long& total_len_ms, TagLib::String& artist, TagLib::String& title);
        void renderSpectrum();
        void renderOverlay(Stream* current_stream, unsigned long current_pos_ms);

        bool updateGUI();
        bool handleWindowEvent(const SDL_WindowEvent& event);
        bool handleKeyPress(const SDL_keysym& keysym);
#ifdef HAVE_FILEDIALOG
        // Ctrl+O: multi-select native chooser; REPLACE the playlist with the
        // chosen tracks and play from the first one.
        void openTracksReplacingPlaylist();
        // "I": queue the chosen track(s) to play right after the current track.
        void queueTracksNext();
        // Queue the chosen track(s) at the end of the playlist.
        void queueTracksEnd();
        // "L": single-select native chooser; play the chosen file in place of the
        // current track without modifying the playlist (forgotten on next change).
        void openTemporaryTrackDialog();
        // Shared chooser for the queue actions: insert the chosen (playlist-
        // expanded) tracks at `insert_at`; if nothing is playing, start with the
        // first queued track, otherwise leave the current track playing.
        void queueTracks(long insert_at, const char* dialog_title);
#endif
        // Empty the playlist and stop playback.
        void clearPlaylist();
#ifdef _WIN32
        // Native Win32 menu bar (Windows build only). File / Settings menus
        // mirroring the I, L, F, ZXC and 1-4 keys.
        void installWin32Menu();
        void handleWin32MenuCommand(unsigned int cmd_id);
        void syncWin32MenuState(); // reflect current fft mode / delay / intensity as radio checks
#endif
        // Reflect Playing vs. not on the Windows taskbar thumbnail-toolbar
        // play/pause button. Compiles to a no-op off Windows, so call sites need
        // no #ifdef. Cheap and safe to call on every play-state transition.
        void updateTaskbarPlayState();
        void handleMouseButtonDown(const SDL_MouseButtonEvent& event);
        void handleMouseMotion(const SDL_MouseMotionEvent& event);
        void handleMouseButtonUp(const SDL_MouseButtonEvent& event);
        bool handleUserEvent(const SDL_UserEvent& event);
        void handleStartFirstTrackEvent();
        void handleDoNextTrackEvent();
        void handleDoPrevTrackEvent();
        void handleTrackLoadSuccessEvent(TrackLoadResult* result);
        void handleTrackLoadFailureEvent(TrackLoadResult* result);
        void handleTrackPreloadSuccessEvent(TrackLoadResult* result);
        void handleTrackPreloadFailureEvent(TrackLoadResult* result);
        void handleRunGuiIterationEvent();
        void handleTrackSeamlessSwapEvent();
        void handleDoSavePlaylistEvent();
        void handleShowNotificationEvent(std::pair<std::string, NotificationType>* data);
        void handleDoSetLoopModeEvent(LoopMode mode);
        void handleKeyUp(const SDL_keysym& keysym);
        void showToast(const std::string& message, Uint32 duration_ms = 2000);
        void seekToInternal(unsigned long pos, bool monitor_seek_errors);
        void updateInfo(bool is_loading = false, const TagLib::String& error_msg = "");
        
        // Initialization and cleanup
        bool Initialize(const PlayerOptions& options);
        void EventLoop();
        void Cleanup();

        // Window management methods
        void renderWindows();
        bool handleWindowMouseEvents(const SDL_Event& event);
        bool windowOwnsMouseCapture() const;
        void toggleTestWindowH();
        void toggleTestWindowB();
        void toggleTestWindowP();
        void createRandomWindows();

        std::unique_ptr<Display> screen;
        std::unique_ptr<Surface> graph;
        std::unique_ptr<Playlist> playlist;
        std::unique_ptr<Font> font;
        std::unique_ptr<Font> m_large_font;
        
        Stream* stream = nullptr;
        std::unique_ptr<Stream> m_next_stream; // Slot for the pre-loaded next track
        std::vector<int16_t> m_next_stream_primed_samples;
        bool m_next_stream_primed_eof = false;
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
#ifdef _WIN32
        // HMENU handles for the three Settings submenus (kept as void* so the
        // header needn't pull in <windows.h>); used to update the radio checks.
        void* m_win32_fft_menu = nullptr;
        void* m_win32_delay_menu = nullptr;
        void* m_win32_intensity_menu = nullptr;
#endif

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
        MenuBarWidget* m_menu_bar = nullptr; // in-app menu bar (non-owning; owned by ApplicationWidget)
        
        // Progress bar widget (non-owning pointer - owned by progress frame widget which is owned by ApplicationWidget)
        PlayerProgressBarWidget* m_progress_widget;

        // Overlay widgets
        LyricsWidget* m_lyrics_widget = nullptr;
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
        std::atomic<LoopMode> m_loop_mode;
        std::vector<Uint32> m_spectrum_colors;
        bool m_use_widget_mouse_handling = true;
        float m_volume = 0.75f;   // default 75%; overridden by psymp3.conf if present

        // Automated testing members
        bool m_automated_test_mode;
        bool m_unattended_quit;
        bool m_show_mpris_errors;
        int m_automated_test_track_count;
        SDL_TimerID m_automated_test_timer_id = 0;
        SDL_TimerID m_automated_quit_timer_id = 0;
        SDL_TimerID m_app_loop_timer_id = 0;

        // Random number generator
        std::mt19937 m_rng;

        // Test windows
        std::unique_ptr<WindowFrameWidget> m_test_window_h;
        std::unique_ptr<WindowFrameWidget> m_test_window_b;
        std::unique_ptr<WindowFrameWidget> m_test_window_p;
        std::vector<std::unique_ptr<WindowFrameWidget>> m_random_windows;

        // Equalizer window (hosted in m_random_windows so it reuses the frame
        // render/mouse machinery; these are non-owning back-pointers) plus the
        // canonical EQ state, re-applied to each Audio like m_volume.
        WindowFrameWidget* m_eq_window = nullptr;
        EqualizerWindow*   m_eq_client = nullptr;
        std::array<float, Equalizer::kNumBands> m_eq_gains{};
        bool m_eq_enabled = false;
        // Display logical scale (1x/2x) loaded from psymp3.conf; applied to the
        // Display once it is created (loadSettings runs before that).
        int  m_pending_scale = 1;
        int  m_target_fps = 30;   // redraw cadence; persisted in psymp3.conf
        void toggleEqualizerWindow();
        void applyEqStateToAudio();

        // Persistent user settings (volume, EQ gains/enabled) in psymp3.conf
        // under the config dir; loaded at construction, saved at shutdown.
        void loadSettings();
        void saveSettings() const;
        int m_random_window_counter = 0;

        // Deferred widget deletion
        std::vector<std::unique_ptr<Widget>> m_deferred_widgets;
        void processDeferredDeletions();
        void deferWidgetDeletion(std::unique_ptr<Widget> widget);
        
        /**
         * @brief Defer deletion of a widget owned by a unique_ptr member.
         * 
         * This method safely moves the widget from the specified unique_ptr into
         * the deferred deletion queue. The unique_ptr is reset to nullptr immediately,
         * but the actual deletion is deferred until the end of the current event loop
         * iteration, preventing use-after-free when a widget's callback triggers its
         * own destruction.
         * 
         * @tparam T Widget type (must derive from Widget)
         * @param ptr Reference to the unique_ptr member to be released
         */
        template<typename T>
        void deferDelete(std::unique_ptr<T>& ptr) {
            if (ptr) {
                // Move to deferred queue for safe deletion later
                m_deferred_widgets.push_back(std::move(ptr));
            }
        }
};

#endif // PLAYER_H
