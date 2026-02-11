/*
 * player.cpp - class implementation for player class
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

#include "psymp3.h"

std::atomic<bool> Player::guiRunning{false};

static std::string convertInt(long number) {
   std::stringstream ss;
   ss << number;
   return ss.str();
}

static std::string convertInt2(long number) {
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << number;
    return ss.str();
}

Player::Player() {
    Debug::log("player", "PsyMP3 version ", PSYMP3_VERSION, ".");

    m_loader_active = true;
    m_loading_track = false;
    m_preloading_track = false;
    m_automated_test_mode = false;
    m_automated_test_track_count = 0;
    m_show_mpris_errors = true;
    m_loader_thread = std::thread(&Player::loaderThreadLoop, this);
    
    // Initialize Last.fm scrobbling
    m_lastfm = std::make_unique<LastFM>();
    m_track_start_time = 0;
    m_track_scrobbled = false;
    m_volume = 1.0f;
    
#ifdef HAVE_DBUS
    m_mpris_manager = std::make_unique<PsyMP3::MPRIS::MPRISManager>(this);
    auto init_result = m_mpris_manager->initialize();
    if (!init_result.isSuccess()) {
        Debug::log("mpris", "MPRIS initialization failed: ", init_result.getError());
        // Continue without MPRIS - graceful degradation
        m_mpris_manager.reset();
    } else {
        Debug::log("mpris", "MPRIS initialized successfully");
    }
#endif
}

Player::~Player() {
    // Stop audio first to join decoder threads before deleting other members
    audio.reset();

    // Notify all windows that the application is shutting down
    ApplicationWidget::getInstance().notifyShutdown();
    
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->shutdown();
        m_mpris_manager.reset();
    }
#endif
    if (m_loader_active) {
        m_loader_active = false;
        m_loader_queue_cv.notify_one(); 
        if (m_loader_thread.joinable()) {
            m_loader_thread.join();
        }
    }

    if (m_playlist_populator_thread.joinable()) {
        m_playlist_populator_thread.join();
    }
#ifdef _WIN32
    if (system) system->clearNowPlaying();
#endif
}

/**
 * @brief Synthesizes and pushes a key down and key up event into the SDL event queue.
 * This is useful for programmatically triggering key press actions.
 * @param kpress The SDLKey symbol for the key to be pressed.
 */
void Player::synthesizeKeyEvent(SDLKey kpress) {
    SDL_Event event;
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = kpress;
    SDL_PushEvent(&event);
    event.type = SDL_KEYUP;
    SDL_PushEvent(&event);
}

/**
 * @brief Synthesizes and pushes a custom user event into the SDL event queue.
 * This is the primary mechanism for inter-thread communication, allowing background threads
 * to safely notify the main thread of completed work or required actions.
 * @param code The integer code identifying the user event.
 * @param data1 A pointer to the first data payload.
 * @param data2 A pointer to the second data payload.
 */
void Player::synthesizeUserEvent(int code, void *data1, void* data2) {
    SDL_Event event;

    event.type = SDL_USEREVENT;
    event.user.code = code;
    event.user.data1 = data1;
    event.user.data2 = data2;

    SDL_PushEvent(&event);
}

/**
 * @brief A static timer callback function for SDL_AddTimer.
 * This function is called periodically by an SDL timer to push a GUI update event
 * into the queue, ensuring the UI remains responsive even when no other events are occurring.
 * @param interval The timer interval.
 * @param param A user-defined parameter (unused).
 * @return The interval for the next timer call.
 */
Uint32 Player::AppLoopTimer(Uint32 interval, void* param) {
    if (!Player::guiRunning)
        Player::synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr);
    else
        Debug::log("timer", "skipped");

    return interval;
}

/**
 * @brief Requests the asynchronous loading of a track for immediate playback.
 * This method adds a 'PlayNow' request to a queue, which is processed by a background
 * loader thread. This prevents the UI from freezing during file I/O and decoding.
 * @param path The file path of the track to load and play.
 */
void Player::requestTrackLoad(TagLib::String path) {
    Debug::log("loader", "Player::requestTrackLoad(", path.to8Bit(true), ") called.");
    if (m_loading_track) {
        Debug::log("loader", "Already loading a track, ignoring new request: ", path.to8Bit(true));
        return; // Or queue it, depending on desired behavior
    }
    m_loading_track = true;
    m_preloading_track = false; // A "play now" request cancels any pending preload
    m_next_stream.reset(); // Clear any existing preloaded stream

    // Update UI to show "loading" state
    updateInfo(true);
    
    // Push request to queue and notify loader thread
    {
        std::lock_guard<std::mutex> lock(m_loader_queue_mutex);
        m_loader_queue.push({LoadRequestType::PlayNow, path, {}});
    }
    m_loader_queue_cv.notify_one();
}

/**
 * @brief Requests the asynchronous pre-loading of the next track in the playlist.
 * This is typically called when the current track is nearing its end. The pre-loaded
 * stream is held in `m_next_stream` for a seamless transition.
 * @param path The file path of the track to preload.
 */
void Player::requestTrackPreload(const TagLib::String& path) {
    if (m_loading_track || m_preloading_track || m_next_stream) return;
    m_preloading_track = true;
    {
        std::lock_guard<std::mutex> lock(m_loader_queue_mutex);
        m_loader_queue.push({LoadRequestType::Preload, path, {}});
    }
    m_loader_queue_cv.notify_one();
}

/**
 * @brief Requests the asynchronous loading of a ChainedStream.
 * This is used for "hidden track" sequences, where multiple short audio files
 * are treated as a single, continuous stream.
 * @param paths A vector of file paths to be chained together.
 */
void Player::requestChainedStreamLoad(const std::vector<TagLib::String>& paths) {
    if (m_loading_track || m_preloading_track || m_next_stream) return;
    m_preloading_track = true;
    {
        std::lock_guard<std::mutex> lock(m_loader_queue_mutex);
        m_loader_queue.push({LoadRequestType::PreloadChained, "", paths});
    }
    m_loader_queue_cv.notify_one();
}

/**
 * @brief The main loop for the background track loader thread.
 * This thread waits for load requests to appear in a queue. When a request is
 * received, it opens the corresponding media file (which can be a blocking operation)
 * and then posts a success or failure event back to the main thread with the result.
 * This design keeps the UI responsive.
 */
void Player::loaderThreadLoop() {
    System::setThisThreadName("track-loader");
    while (m_loader_active) {
        TrackLoadRequest request;
        {
            std::unique_lock<std::mutex> lock(m_loader_queue_mutex);
            m_loader_queue_cv.wait(lock, [this]{
                return !m_loader_queue.empty() || !m_loader_active;
            });
            if (!m_loader_active) break; // Exit condition
            request = m_loader_queue.front();
            m_loader_queue.pop();
        } // Unlock mutex before blocking I/O

        Stream* new_stream = nullptr;
        TagLib::String error_msg;
        size_t num_chained = 1;

        try {
            switch (request.type) {
                case LoadRequestType::PlayNow:
                case LoadRequestType::Preload:
                    new_stream = MediaFile::open(request.path).release();
                    num_chained = 1;
                    break;
                case LoadRequestType::PreloadChained:
                    new_stream = new ChainedStream(request.paths);
                    num_chained = request.paths.size();
                    break;
            }
        } catch (const std::exception& e) {
            error_msg = e.what();
            new_stream = nullptr; // Ensure null if exception
        }

        // Synthesize event back to main thread
        auto* result = new TrackLoadResult(); // Allocated on heap, freed by main thread
        result->request_type = request.type;
        result->stream = new_stream;
        result->error_message = error_msg;
        result->num_chained_tracks = num_chained;

        int success_event = (request.type == LoadRequestType::PlayNow) ? TRACK_LOAD_SUCCESS : TRACK_PRELOAD_SUCCESS;
        int failure_event = (request.type == LoadRequestType::PlayNow) ? TRACK_LOAD_FAILURE : TRACK_PRELOAD_FAILURE;

        synthesizeUserEvent(new_stream ? success_event : failure_event, result, nullptr);
    }
}

/**
 * @brief The main loop for the background playlist populator thread.
 * This thread is responsible for parsing command-line arguments and adding them
 * to the playlist. This is done in the background to allow the main window to
 * appear immediately on startup, without waiting for file system access.
 * @param args The vector of command-line arguments passed to the application.
 */
void Player::playlistPopulatorLoop(std::vector<std::string> args) {
    System::setThisThreadName("playlist-populator");

    if (args.empty()) return; // Nothing to do

    // Check if the first argument is a playlist file (M3U/M3U8)
    TagLib::String first_arg(args[0], TagLib::String::UTF8);
    std::string first_arg_str = first_arg.to8Bit(true);
    size_t dot_pos = first_arg_str.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = first_arg_str.substr(dot_pos + 1);
        if (ext == "m3u" || ext == "m3u8") {
            // Load the playlist file
            auto loaded_playlist = Playlist::loadPlaylist(first_arg);
            if (loaded_playlist && loaded_playlist->entries() > 0) {
                // Replace the current playlist with the loaded one
                playlist = std::move(loaded_playlist); // Transfer ownership
                // Start playing the first track from the loaded playlist
                synthesizeUserEvent(START_FIRST_TRACK, nullptr, nullptr);
                return; // We've handled the playlist, so exit
            } else {
                Debug::log("playlist", "Failed to load or empty playlist: ", first_arg.to8Bit(true));
            }
        }
    }

    // If not a playlist or loading failed, treat arguments as individual files
    for (size_t i = 0; i < args.size(); ++i) {
        try {
            playlist->addFile(TagLib::String(args[i], TagLib::String::UTF8));
            if (i == 0) synthesizeUserEvent(START_FIRST_TRACK, nullptr, nullptr); // Start first track
        } catch (const std::exception& e) {
            Debug::log("playlist", "Player::playlistPopulatorLoop(): Failed to add file ", args[i], ": ", e.what());
        }
    }
}

void Player::toggleMPRISErrorNotifications() {
    m_show_mpris_errors = !m_show_mpris_errors;
    std::string state = m_show_mpris_errors ? "ON" : "OFF";
    showToast("MPRIS Errors: " + state);
}

void Player::showMPRISError(const std::string& message) {
    auto* msg_ptr = new std::string(message);
    synthesizeUserEvent(SHOW_MPRIS_ERROR, msg_ptr, nullptr);
}

/**
 * @brief Advances to the next track in the playlist.
 * @param advance_count The number of tracks to advance by. Defaults to 1.
 * @return `true` if a track was successfully loaded, `false` if the playlist is empty
 * or the end was reached without wrapping.
 */
void Player::nextTrack(size_t advance_count) {
    m_navigation_direction = 1;
    if (advance_count == 0) advance_count = 1;
    if (!playlist || playlist->entries() == 0) {
        stop();
        return;
    }

    long new_pos = playlist->getPosition();
    for (size_t i = 0; i < advance_count; ++i) {
        new_pos++;
    }

    if (new_pos >= playlist->entries()) { // Reached or passed the end
        if (m_loop_mode == LoopMode::All) {
            new_pos = 0; // Wrap
        } else { // LoopMode::None
            if (m_unattended_quit) {
                synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
                return;
            }
            stop();
            updateInfo();
            return;
        }
    }

    playlist->setPosition(new_pos);
    m_skip_attempts = 0; // Reset skip counter for new navigation
    requestTrackLoad(playlist->getTrack(new_pos));
}

/**
 * @brief Moves to the previous track in the playlist.
 * @return `true` always.
 */
void Player::prevTrack(void) {
    m_navigation_direction = -1;
    long new_pos = playlist->getPosition() - 1;

    if (new_pos < 0) {
        if (m_loop_mode == LoopMode::All) {
            new_pos = playlist->entries() - 1; // Wrap to end
        } else { // LoopMode::None
            seekTo(0); // Go to start of current track
            return;
        }
    }

    playlist->setPosition(new_pos);
    m_skip_attempts = 0; // Reset skip counter for new navigation
    requestTrackLoad(playlist->getTrack(new_pos));
}

/**
 * @brief Stops playback completely.
 * Resets the stream and audio device.
 * @return `true` always.
 */
bool Player::stop(void) {
    state = PlayerState::Stopped;
    m_pause_indicator.reset();
    // Safely signal to the audio thread that the stream is gone before we destroy it.
    if (audio) {
        audio->setStream(nullptr);
    }
    audio.reset(); // Destroy the audio object when stopping.
    stream = nullptr;

    if (m_lyrics_widget) {
        m_lyrics_widget->clearLyrics();
    }

#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus::Stopped);
    }
#endif
#ifdef _WIN32
    if (system) system->clearNowPlaying();
#endif
    // Clear Last.fm now playing status when stopping
    if (m_lastfm) {
        m_lastfm->unsetNowPlaying();
    }
    return true;
}

/**
 * @brief Pauses playback.
 * If already playing, it pauses the audio output. Does nothing if stopped.
 * @return `true` if paused successfully, `false` if the player was stopped.
 */
bool Player::pause(void) {
    if (state != PlayerState::Stopped) {
        audio->play(false);
        state = PlayerState::Paused;
#ifdef HAVE_DBUS
        if (m_mpris_manager) {
            m_mpris_manager->updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus::Paused);
        }
#endif
        // Clear Last.fm now playing status when pausing
        if (m_lastfm) {
            m_lastfm->unsetNowPlaying();
        }
        if (!m_pause_indicator) {
            SDL_Color pause_color = {255, 255, 255, 180}; // Semi-transparent white
            m_pause_indicator = std::make_unique<Label>(m_large_font.get(), Rect(0,0,0,0), "PAUSED", pause_color);
        }
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Starts or resumes playback.
 * @return `true` always.
 */
bool Player::play(void) {
    // If we are stopped, we can't just play. A track must be loaded first.
    // The track loading process will call play() again once it's ready.
    if (state == PlayerState::Stopped) {
        if (playlist && playlist->entries() > 0) {
            // Request the current track. If it's the same, it will just restart.
            requestTrackLoad(playlist->getTrack(playlist->getPosition()));
        }
    } else { // Paused or already Playing
        PlayerState previous_state = state;
        m_pause_indicator.reset();
        if (audio) audio->play(true);
        state = PlayerState::Playing;
#ifdef HAVE_DBUS
        if (m_mpris_manager) {
            m_mpris_manager->updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus::Playing);
        }
#endif
        // Re-set Last.fm now playing status when resuming from pause
        if (previous_state == PlayerState::Paused && m_lastfm) {
            submitNowPlaying();
        }
    }
    return true;
}

/**
 * @brief Toggles between play and pause states.
 * @return `true` always.
 */
bool Player::playPause(void) {
    if (state == PlayerState::Playing) {
        pause();
    } else {
        // This handles both Stopped and Paused states correctly.
        play();
    }
    return true;
}

/**
 * @brief Seeks the current stream to a specific position.
 * This is a thread-safe operation that locks the player mutex.
 * @param pos The target position in milliseconds.
 */
void Player::seekTo(unsigned long pos)
{
    std::lock_guard<std::mutex> lock(*mutex);
    if (stream) {
        if (audio) {
            audio->resetBuffer();
            audio->setSamplesPlayed((pos * audio->getRate()) / 1000);
        }
        stream->seekTo(pos);
        
#ifdef HAVE_DBUS
        // Notify MPRIS about the seek operation (convert ms to microseconds)
        if (m_mpris_manager) {
            m_mpris_manager->notifySeeked(static_cast<uint64_t>(pos) * 1000);
        }
#endif
    }
}

/**
 * @brief Pre-calculates the color gradient for the spectrum analyzer.
 * This is done once at startup to avoid expensive color calculations in the main render loop.
 */
void Player::precomputeSpectrumColors() {
    Debug::log("player", "precomputeSpectrumColors called.");
    if (!graph) {
        Debug::log("player", "graph is null!");
        return;
    }
    Debug::log("player", "graph is valid.");

    m_spectrum_colors.resize(320);
    for (uint16_t x = 0; x < 320; ++x) {
        uint8_t r, g, b;
        if (x > 213) {
            r = static_cast<uint8_t>((x - 214) * 2.4);
            g = 0;
            b = 255;
        } else if (x < 106) {
            r = 128;
            g = 255;
            b = static_cast<uint8_t>(x * 2.398);
        } else {
            r = static_cast<uint8_t>(128 - ((x - 106) * 1.1962615));
            g = static_cast<uint8_t>(255 - ((x - 106) * 2.383177));
            b = 255;
        }
        // Debug::log("player", "x: ", x, " r: ", (int)r, " g: ", (int)g, " b: ", (int)b);
        m_spectrum_colors[x] = graph->MapRGBA(r, g, b, 255);
    }
    Debug::log("player", "precomputeSpectrumColors finished.");
}

/**
 * @brief Renders the spectrum analyzer visualization onto a given surface.
 * This includes the falling-bar visualization and the "ghosting" or fade effect,
 * which is achieved by blitting a semi-transparent black rectangle over the previous frame.
 * @param graph The destination surface to draw the spectrum on.
 */
void Player::renderSpectrum(Surface *graph) {
    float *spectrum = fft->getFFT();
    
    // DEBUG: Print some spectrum values to understand the data range
    static int debug_counter = 0;
    if (debug_counter++ % 60 == 0) { // Print every 60 frames (~1 second at 60fps)
        std::stringstream ss;
        ss << "DEBUG: Spectrum values [0-9]: ";
        for (int i = 0; i < 10; i++) {
            ss << spectrum[i] << " ";
        }
        ss << "| scale=" << scalefactor;
        Debug::log("spectrum", ss.str());
    }

    // --- Fade effect implementation ---
    static std::unique_ptr<Surface> fade_surface_ptr; // Declared static to persist across calls
    static uint8_t cached_fade_alpha = 255; // Cache the last alpha value to avoid redundant SetAlpha calls
    
    // The fade surface is created once and reused. It's filled with opaque black
    // only upon creation, avoiding a FillRect call on every frame.
    if (!fade_surface_ptr || fade_surface_ptr->width() != 640 || fade_surface_ptr->height() != 350) {
        fade_surface_ptr = std::make_unique<Surface>(640, 350); // Creates a 32-bit surface for FFT area only
        // Fill the surface with opaque black. This only needs to be done once.
        fade_surface_ptr->FillRect(fade_surface_ptr->MapRGBA(0, 0, 0, 255));
        cached_fade_alpha = 255; // Reset cache when surface is recreated
    }
    Surface& fade_surface = *fade_surface_ptr;

    // Calculate alpha for the fade (0-255). decayfactor from 0.5 to 2.0
    uint8_t fade_alpha = (uint8_t)(255 * (decayfactor / 8.0f)); // Reduced fade strength: divisor increased from 4.0 to 8.0

    // Only call SetAlpha if the fade_alpha has changed to avoid redundant SDL calls
    if (fade_alpha != cached_fade_alpha) {
        fade_surface.SetAlpha(SDL_SRCALPHA, fade_alpha);
        cached_fade_alpha = fade_alpha;
    }
    
    // Use static rect to avoid repeated object construction
    static const Rect blit_dest_rect(0, 0, 640, 350); // Blit only to the FFT area
    graph->Blit(fade_surface, blit_dest_rect); // This will blend if SDL_SRCALPHA is set on src

    // --- End Fade effect implementation ---

    // Cache constants to reduce repeated calculations
    constexpr int16_t spectrum_height = 350;
    constexpr int16_t spectrum_bottom = 349; // Bottom of the FFT area (0-349 for 350 pixels)
    constexpr uint16_t spectrum_bins = 320;
    
    for(uint16_t x = 0; x < spectrum_bins; x++) {
        // Calculate the bar's height with fewer intermediate calculations
        // Apply gain to amplify weak signals before logarithmic scaling
        const float gained_amplitude = spectrum[x] * 5.0f; // Amplify by 5x
        const float scaled_amplitude = Util::logarithmicScale(scalefactor, gained_amplitude);
        const int16_t y_start = static_cast<int16_t>(spectrum_bottom - scaled_amplitude * spectrum_height);
        const int16_t x_pos = x * 2;
        
        // DEBUG: Print some bar calculations
        if (debug_counter % 60 == 1 && x < 5) { // Different frame to avoid overlap, first 5 bars
            Debug::log("spectrum", "DEBUG bar ", x, ": raw=", spectrum[x], 
                      " gained=", gained_amplitude,
                      " scaled=", scaled_amplitude, 
                      " y_start=", y_start);
        }
        
        // Use box() instead of rectangle() for better performance (SDL_FillRect vs hline loop)
        graph->box(x_pos, y_start, x_pos + 1, spectrum_bottom, m_spectrum_colors[x]);
    };
}

/**
 * @brief Updates the text of all on-screen labels.
 * This function centralizes the logic for displaying track metadata, error messages,
 * and player settings. It is called whenever the player's state changes.
 * @param is_loading `true` to display a "Loading..." state.
 * @param error_msg A string to display if an error has occurred.
 */
void Player::updateInfo(bool is_loading, const TagLib::String& error_msg)
{
    if (is_loading) {
        m_labels.at("artist")->setText("Artist: Loading...");
        m_labels.at("title")->setText("Title: Loading...");
        m_labels.at("album")->setText("Album: Loading...");
        m_labels.at("position")->setText("Position: --:--.-- / --:--.--");
        screen->SetCaption(TagLib::String("PsyMP3 ") + PSYMP3_VERSION + " -:[ Loading... ] :-", TagLib::String("PsyMP3 ") + PSYMP3_VERSION);
    } else if (!error_msg.isEmpty()) {
        m_labels.at("artist")->setText("Artist: N/A");
        m_labels.at("title")->setText("Title: Error: " + error_msg);
        m_labels.at("album")->setText("Album: N/A");
        m_labels.at("playlist")->setText("Playlist: N/A");
        m_labels.at("position")->setText("Position: --:--.-- / --:--.--");
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ Error: " + error_msg.to8Bit(true) + " ] :-", "PsyMP3 " PSYMP3_VERSION);
    } else if (stream) {
        m_labels.at("artist")->setText("Artist: " + stream->getArtist());
        m_labels.at("title")->setText("Title: " + stream->getTitle());
        m_labels.at("album")->setText("Album: " + stream->getAlbum());
        m_labels.at("playlist")->setText("Playlist: " + convertInt(playlist->getPosition() + 1) + "/" + convertInt(playlist->entries()));
    } else {
        // Default empty state
        m_labels.at("artist")->setText("Artist: ");
        m_labels.at("title")->setText("Title: ");
        m_labels.at("album")->setText("Album: ");
        m_labels.at("playlist")->setText("Playlist: 0/0");
        m_labels.at("position")->setText("Position: --:--.-- / --:--.--");
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ] :-", "PsyMP3 " PSYMP3_VERSION);
    }

    // These are always updated based on player settings, not track info
    m_labels.at("scale")->setText("log scale = " + std::to_string(scalefactor));
    m_labels.at("decay")->setText("decay = " + std::to_string(decayfactor));
    m_labels.at("fft_mode")->setText("FFT Mode: " + fft->getFFTModeName());
}

/**
 * @brief The main GUI update function, called on every frame.
 * This function is responsible for orchestrating all rendering. It locks shared data,
 * renders the spectrum, updates and renders overlay widgets (toasts, pause indicators),
 * handles pre-loading logic, draws the progress bar, and finally composes the scene
 * and flips the screen buffer.
 * @return `true` if the current track has ended, `false` otherwise.
 */
bool Player::updateGUI()
{
    Player::guiRunning = true;
    unsigned long current_pos_ms = 0;
    unsigned long total_len_ms = 0;
    TagLib::String artist = "";
    TagLib::String title = "";
    Stream* current_stream = nullptr; // Declare here

    // --- GUI Update Logic ---
    // --- Start of critical section ---
    // Lock the mutex only while accessing shared data (stream, fft).
    {
        std::lock_guard<std::mutex> lock(*mutex);

        // Don't clear the graph surface - widgets will draw their own backgrounds

        // Copy data from stream object while locked
        if (audio && audio->getCurrentStream()) {
            current_stream = audio->getCurrentStream(); // Assign here
            // During a keyboard seek, we use our manually-controlled position for
            // instant visual feedback. Otherwise, get the position from the stream.
            if (m_seek_direction != 0) {
                current_pos_ms = m_seek_position_ms;
            } else {
                if (audio) {
                    current_pos_ms = (audio->getSamplesPlayed() * 1000) / audio->getRate();
                } else {
                    current_pos_ms = 0;
                }
                Debug::log("player", "Player: User visible position=", current_pos_ms, "ms, total_len=", current_stream->getLength(), "ms");
            }
            total_len_ms = current_stream->getLength();
            artist = current_stream->getArtist();
            title = current_stream->getTitle();
            
            // Check if we should scrobble this track (only check every 30 seconds to avoid spam)
            if (state == PlayerState::Playing) {
                static Uint32 last_scrobble_check = 0;
                Uint32 current_time = SDL_GetTicks();
                if (current_time - last_scrobble_check > 30000) { // Check every 30 seconds
                    checkScrobbling();
                    last_scrobble_check = current_time;
                }
            }

            // Trigger preloading when near the end of the track (last 10 seconds)
            if (!m_next_stream && !m_preloading_track && total_len_ms > 0 && 
                (total_len_ms - current_pos_ms) < 10000 && playlist && 
                playlist->getPosition() < playlist->entries() - 1) {
                
                // Look ahead for sequences of short tracks and automatically chain them
                std::vector<TagLib::String> short_track_chain;
                long current_playlist_pos = playlist->getPosition();
                long look_ahead_pos = current_playlist_pos + 1;
                
                // Scan ahead for consecutive short tracks (< 10 seconds each)
                while (look_ahead_pos < playlist->entries()) {
                    TagLib::String candidate_path = playlist->getTrack(look_ahead_pos);
                    if (candidate_path.isEmpty()) break;
                    
                    // Use cached track metadata instead of blocking MediaFile::open()
                    const track* track_info = playlist->getTrackInfo(look_ahead_pos);
                    if (track_info) {
                        long track_length = track_info->GetLen() * 1000; // Convert seconds to milliseconds
                        if (track_length > 0 && track_length < 10000) {
                            // Track is short, add to chain
                            short_track_chain.push_back(candidate_path);
                            look_ahead_pos++;
                        } else if (track_length > 0) {
                            // Found a normal-length track - include it to complete the transition
                            short_track_chain.push_back(candidate_path);
                            break;
                        } else {
                            // Unknown length (cached metadata may be unavailable), stop scanning to be safe
                            break;
                        }
                    } else {
                        // No track info available, stop scanning
                        break;
                    }
                }
                
                if (short_track_chain.size() >= 2) {
                    // Use ChainedStream for sequences of 2+ tracks
                    Debug::log("playlist", "Detected sequence of ", short_track_chain.size(), 
                              " tracks for chaining, starting with: ", 
                              short_track_chain[0].to8Bit(true));
                    requestChainedStreamLoad(short_track_chain);
                } else {
                    // Single track or no short tracks found, use normal preloading
                    TagLib::String next_path = playlist->peekNext();
                    if (!next_path.isEmpty()) {
                        Debug::log("loader", "Preloading next track for seamless transition: ", next_path.to8Bit(true));
                        requestTrackPreload(next_path);
                    }
                }
            }
        }

        // Update spectrum data in the widget - it will render itself via the widget tree
        float *spectrum = fft->getFFT();
        if (m_spectrum_widget) {
            // Use 320 bands like the original renderSpectrum (first 320 of 512 FFT values)
            // Pass live scalefactor and decayfactor values so keypress changes propagate
            m_spectrum_widget->updateSpectrum(spectrum, 320, scalefactor, decayfactor);
        }
    }

#ifdef HAVE_DBUS
    // Update MPRIS position (outside of Player mutex to avoid deadlocks)
    if (m_mpris_manager && current_stream && state == PlayerState::Playing) {
        // Update position periodically (convert ms to microseconds)
        static Uint32 last_position_update = 0;
        Uint32 current_time = SDL_GetTicks();
        if (current_time - last_position_update > 1000) { // Update every second
            m_mpris_manager->updatePosition(static_cast<uint64_t>(current_pos_ms) * 1000);
            last_position_update = current_time;
        }
    }
#endif

    // --- Lyrics Widget Update ---
    if (m_lyrics_widget && m_lyrics_widget->hasLyrics() && current_stream) {
        m_lyrics_widget->updatePosition(current_pos_ms);
    }

    // --- Pause Indicator Rendering ---
    // This should be drawn after the toast so it appears on top if both are active.
    if (m_pause_indicator) {
        // Center the indicator in the graph area
        const Rect& current_pos = m_pause_indicator->getPos();
        Rect new_pos = current_pos;
        new_pos.x((graph->width() - current_pos.width()) / 2);
        new_pos.y((350 - current_pos.height()) / 2); // Center in the 350px FFT area
        m_pause_indicator->setPos(new_pos);
        m_pause_indicator->BlitTo(*graph);
    }

    // --- UI Widget Tree Rendering ---
    // Clear areas where UI widgets will render to prevent ghosting
    // Note: Don't clear spectrum area (0,0,640,350) - SpectrumAnalyzerWidget handles its own background
    // Bottom area for track info labels (below spectrum area)
    graph->box(0, 350, 640, 400, 0, 0, 0, 255);
    // Top-right area for debug labels (over spectrum area)  
    graph->box(545, 0, 640, 48, 0, 0, 0, 255);
    
    // Render ApplicationWidget hierarchy (background + windows)
    if (m_ui_root) {
        m_ui_root->BlitTo(*graph);
    }
    
    // --- Window Rendering ---
    // Update windows (handle auto-dismiss for toasts, etc.)
    ApplicationWidget::getInstance().updateWindows();
    // Render floating windows on top of everything else
    renderWindows();


    // --- End of critical section ---

    // Now use the copied data for rendering, outside the lock.
    if(current_stream) {
        m_labels.at("position")->setText("Position: " + convertInt(current_pos_ms / 60000)
                                + ":" + convertInt2((current_pos_ms / 1000) % 60)
                                + "." + convertInt2((current_pos_ms / 10) % 100)
                                + "/" + convertInt(total_len_ms / 60000)
                                + ":" + convertInt2((total_len_ms / 1000) % 60)
                                + "." + convertInt2((total_len_ms / 10) % 100));
        screen->SetCaption("PsyMP3 " PSYMP3_VERSION +
                        (std::string) " -:[ " + artist.to8Bit(true) + " ] :-" + " -- -:[ " +
                        title.to8Bit(true) + " ] :-", "PsyMP3 " PSYMP3_VERSION);
    } else {
        m_labels.at("position")->setText("Position: -:--.-- / -:--.--");
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ] :-", "PsyMP3 " PSYMP3_VERSION);
    }
    
    // Progress bar frame and fill are now handled by ProgressBarFrameWidget hierarchy

    // --- Continuous Keyboard Seeking ---
    if (m_seek_direction != 0 && stream && !m_is_dragging) {
        // This implements a continuous seek while arrow keys are held down.
        const long long seek_increment_ms = 500; // Seek by 500ms per frame

        if (m_seek_direction == 1) { // backwards
            long long signed_pos = m_seek_position_ms;
            signed_pos -= seek_increment_ms;
            m_seek_position_ms = (signed_pos < 0) ? 0 : static_cast<unsigned long>(signed_pos);
        } else if (m_seek_direction == 2) { // forwards
            m_seek_position_ms += seek_increment_ms;
            if (total_len_ms > 0 && m_seek_position_ms > total_len_ms) {
                m_seek_position_ms = total_len_ms;
            }
        }
        seekTo(m_seek_position_ms); // Request the asynchronous seek
        current_pos_ms = m_seek_position_ms; // Update local var for instant visual feedback
    }

    // Update progress bar widget
    if (m_progress_widget) {
        if (total_len_ms > 0) {
            unsigned long display_position_ms = m_is_dragging ? m_drag_position_ms : current_pos_ms;
            double progress = static_cast<double>(display_position_ms) / static_cast<double>(total_len_ms);
            if (m_is_dragging) {
                m_progress_widget->setDragProgress(progress);
            } else {
                m_progress_widget->setProgress(progress);
            }
        } else {
            // No track loaded, set progress to 0
            m_progress_widget->setProgress(0.0);
        }
    }

    // --- Final Scene Composition ---
    // 1. Clear the main screen
    screen->FillRect(screen->MapRGB(0, 0, 0));
    // 2. Blit the entire dynamic buffer (graph) to the screen
    screen->Blit(*graph, Rect(0, 0, graph->width(), graph->height()));

    // finally, update the screen :)
    screen->Flip();
    
    Player::guiRunning = false;
    // and if end of stream...
    // Do not signal end-of-track if we are in the middle of loading a new one.
    if (m_loading_track || m_preloading_track) {
        return false;
    }
    return audio ? audio->isFinished() : false;
}

/**
 * @brief Handles key press events.
 * This function contains the main logic for all keyboard shortcuts.
 * @param keysym The SDL_keysym structure for the pressed key.
 * @return `true` if the event signals that the application should exit, `false` otherwise.
 */
bool Player::handleKeyPress(const SDL_keysym& keysym)
{
    switch (keysym.sym) {
        case SDLK_ESCAPE: // NOLINT(bugprone-branch-clone)
        case SDLK_q:
            return true; // Signal to exit

        case SDLK_n:
            nextTrack();
            break;

        case SDLK_p:
            prevTrack();
            break;

        case SDLK_s:
            if (keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) {
                synthesizeUserEvent(DO_SAVE_PLAYLIST, nullptr, nullptr);
            }
            break;

        case SDLK_UP:
            setVolume(getVolume() + 0.05);
            break;

        case SDLK_DOWN:
            setVolume(getVolume() - 0.05);
            break;

        case SDLK_0:
            scalefactor = 0;
            updateInfo();
            break;
        case SDLK_1:
            scalefactor = 1;
            updateInfo();
            break;
        case SDLK_2:
            scalefactor = 2;
            updateInfo();
            break;
        case SDLK_3:
            scalefactor = 3;
            updateInfo();
            break;
        case SDLK_4:
            scalefactor = 4;
            updateInfo();
            break;

        case SDLK_z: decayfactor = 0.5f; updateInfo(); break;
        case SDLK_x: decayfactor = 1.0f; updateInfo(); break;
        case SDLK_c: decayfactor = 2.0f; updateInfo(); break;

        case SDLK_LEFT:
            // On the initial key press, capture the current position to seek from.
            if (m_seek_direction == 0 && stream) {
                m_seek_position_ms = stream->getPosition();
            }
            m_seek_direction = 1;
            if (!m_seek_left_indicator) {
                auto sfc = std::make_unique<Surface>(11, 7);
                sfc->line(0, 3, 10, 3, 255, 0, 0, 255); // shaft
                sfc->line(0, 3, 3, 0, 255, 0, 0, 255); // top arrowhead
                sfc->line(0, 3, 3, 6, 255, 0, 0, 255); // bottom arrowhead
                auto fading_widget = std::make_unique<FadingWidget>();
                m_seek_left_indicator = fading_widget.get();
                fading_widget->setSurface(std::move(sfc));
                fading_widget->setPos(Rect(380, 374, 11, 7));
                ApplicationWidget::getInstance().addWindow(std::move(fading_widget), ZOrder::UI);
            }
            m_seek_left_indicator->fadeIn();
            break;

        case SDLK_RIGHT:
            // On the initial key press, capture the current position to seek from.
            if (m_seek_direction == 0 && stream) {
                m_seek_position_ms = stream->getPosition();
            }
            m_seek_direction = 2;
            if (!m_seek_right_indicator) {
                auto sfc = std::make_unique<Surface>(11, 7);
                sfc->line(0, 3, 10, 3, 0, 255, 0, 255); // shaft
                sfc->line(10, 3, 7, 0, 0, 255, 0, 255); // top arrowhead
                sfc->line(10, 3, 7, 6, 0, 255, 0, 255); // bottom arrowhead
                auto fading_widget = std::make_unique<FadingWidget>();
                m_seek_right_indicator = fading_widget.get();
                fading_widget->setSurface(std::move(sfc));
                fading_widget->setPos(Rect(628, 374, 11, 7));
                ApplicationWidget::getInstance().addWindow(std::move(fading_widget), ZOrder::UI);
            }
            m_seek_right_indicator->fadeIn();
            break;

        case SDLK_SPACE:
            playPause();
            break;

        case SDLK_r:
            this->seekTo(0);
            break;

        case SDLK_e:
        {
            LoopMode next_mode = LoopMode::None;
            switch (m_loop_mode) {
                case LoopMode::None: next_mode = LoopMode::One; break;
                case LoopMode::One:  next_mode = LoopMode::All; break;
                case LoopMode::All:  next_mode = LoopMode::None; break;
                default: next_mode = LoopMode::None; break;
            }
            setLoopMode(next_mode);
            // Persist this setting for the next session
            break;
        }

        case SDLK_f:
        {
            // Cycle through FFT modes
            FFTMode current_mode = fft->getFFTMode();
            FFTMode next_mode;
            switch (current_mode) {
                case FFTMode::Original:  next_mode = FFTMode::Optimized; break;
                case FFTMode::Optimized: next_mode = FFTMode::NeomatIn;  break;
                case FFTMode::NeomatIn:  next_mode = FFTMode::NeomatOut; break;
                case FFTMode::NeomatOut: next_mode = FFTMode::Original;  break;
                default:                 next_mode = FFTMode::Original;  break; // Should not happen
            }
            fft->setFFTMode(next_mode);
            showToast("FFT Mode: " + fft->getFFTModeName());
            updateInfo();
            break;
        }
        
        case SDLK_h:
        {
            toggleTestWindowH();
            break;
        }
        
        case SDLK_j:
        {
            createRandomWindows();
            break;
        }
        
        case SDLK_b:
        {
            toggleTestWindowB();
            break;
        }

        case SDLK_m:
        {
            if (keysym.mod & KMOD_SHIFT) {
                toggleMPRISErrorNotifications();
            } else {
                // Toggle between widget-based and legacy mouse handling
                m_use_widget_mouse_handling = !m_use_widget_mouse_handling;
                if (m_use_widget_mouse_handling) {
                    showToast("Mouse: Widget-based handling");
                } else {
                    showToast("Mouse: Legacy handling");
                }
            }
            break;
        }

        case SDLK_d:
        {
            // Toggle widget blitting debug output
            // This is a temporary mechanism. A proper debug console or UI will replace this.
            static bool widget_debug_on = false;
            widget_debug_on = !widget_debug_on;
            std::vector<std::string> channels;
            if (widget_debug_on) {
                channels.push_back("widget");
                showToast("Debug: Widget blitting enabled");
            } else {
                showToast("Debug: Widget blitting disabled");
            }
            Debug::init("", channels); // Re-init to update channels
            break;
        }

        default:
            // No action for other keys
            break;
    }

    return false; // Do not exit
}

/**
 * @brief Displays a short-lived "toast" notification on the screen.
 * Forces immediate fade-out of existing toast, then smooth fade-in of new toast.
 * @param message The text message to display.
 * @param duration_ms The duration in milliseconds for the toast to be visible.
 */
void Player::showToast(const std::string& message, Uint32 duration_ms)
{
    // Remove all existing toasts first (like Android behavior)
    ApplicationWidget::getInstance().removeAllToasts();
    
    // Convert to new ToastWidget system
    auto toast = std::make_unique<ToastWidget>(message, font.get(), static_cast<int>(duration_ms));
    
    // Position toast at center-bottom of screen
    Rect toast_pos = toast->getPos();
    toast_pos.x((640 - toast_pos.width()) / 2);  // Center horizontally
    toast_pos.y(350 - toast_pos.height() - 40);   // 40px above bottom of FFT area
    toast->setPos(toast_pos);
    
    // Add to ApplicationWidget with maximum Z-order (always on top)
    ApplicationWidget::getInstance().addWindow(std::move(toast), ZOrder::MAX);
}

/**
 * @brief Handles mouse button down events.
 * This is primarily used to detect when the user clicks on the progress bar to initiate a seek drag.
 * @param event The SDL_MouseButtonEvent structure.
 */
void Player::handleMouseButtonDown(const SDL_MouseButtonEvent& event)
{
    if (event.button == SDL_BUTTON_LEFT) {
        if (event.x >= 400 && event.x <= 620 &&
            event.y >= 370 && event.y <= 385) {
            if (stream) {
                m_is_dragging = true;
                // Don't seek here, just update the visual position and start the drag.
                int relative_x = event.x - 400;
                double progress_ratio = static_cast<double>(relative_x) / 220.0;
                m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress_ratio);
                synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr);
                m_drag_start_x = event.x;
                m_drag_start_time = SDL_GetTicks();
            }
        }
    }
}

/**
 * @brief Handles mouse motion events.
 * If a drag-seek is in progress, this updates the visual position of the progress bar.
 * @param event The SDL_MouseMotionEvent structure.
 */
void Player::handleMouseMotion(const SDL_MouseMotionEvent& event)
{
    if (m_is_dragging && stream && stream->getLength() > 0 &&
        event.x >= 400 && event.x <= 620 &&
        event.y >= 370 && event.y <= 385) {
        int relative_x = event.x - 400; // 0 to 220
        double progress_ratio = static_cast<double>(relative_x) / 220.0;
        m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress_ratio);
        synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr); // Update display during drag
    }
}

/**
 * @brief Handles mouse button up events.
 * If a drag-seek was in progress, this finalizes the seek by sending the actual seek command.
 * @param event The SDL_MouseButtonEvent structure.
 */
void Player::handleMouseButtonUp(const SDL_MouseButtonEvent& event)
{
    if (event.button == SDL_BUTTON_LEFT && m_is_dragging) {
        // The actual seek happens here, on mouse release.
        if (stream) {
            seekTo(m_drag_position_ms);
        }
        m_is_dragging = false;
    }
}

/**
 * @brief Handles key release events.
 * This is used to stop continuous actions, such as keyboard seeking.
 * @param keysym The SDL_keysym structure for the released key.
 */
void Player::handleKeyUp(const SDL_keysym& keysym)
{
    switch (keysym.sym) {
        case SDLK_LEFT:
            if (m_seek_left_indicator) {
                m_seek_left_indicator->fadeOut();
            }
            m_seek_direction = 0;
            break;
        case SDLK_RIGHT:
            if (m_seek_right_indicator) {
                m_seek_right_indicator->fadeOut();
            }
            m_seek_direction = 0;
            break;
        default:
            break;
    }
}

/**
 * @brief Handles custom SDL user events.
 * This is the main entry point for processing events sent from background threads,
 * such as track loading results, or from the GUI timer.
 * @param event The SDL_UserEvent structure.
 * @return `true` if the event signals that the application should exit, `false` otherwise.
 */
bool Player::handleUserEvent(const SDL_UserEvent& event)
{
    switch(event.code) {
        case START_FIRST_TRACK: 
        {
            if (playlist->entries() > 0) {
                // Use findFirstPlayableTrack to locate the first playable track
                if (!findFirstPlayableTrack()) {
                    // No playable tracks found
                    stop();
                    updateInfo(false, "No playable tracks found in playlist.");
                }
            }
            break;
        }
        case DO_NEXT_TRACK: 
        {
            nextTrack(m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1);
            break;
        }
        case DO_PREV_TRACK:
        {
            prevTrack();
            break;
        }
        case TRACK_LOAD_SUCCESS: 
        {
            Debug::log("loader", "Player::handleUserEvent(TRACK_LOAD_SUCCESS) called.");
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            m_skip_attempts = 0; // Reset skip counter on a successful load.
            Stream* new_stream = result->stream;
            m_num_tracks_in_current_stream = result->num_chained_tracks;
            delete result; // Free the result struct

            m_loading_track = false; // Loading complete

            // If we were stopped, audio is null. If playing, check if format changed.
            // Per request, always re-initialize the audio object on a new track.
            // This is simpler and more robust than trying to reuse it.
            audio.reset();

            // Take ownership of the new stream from the loader thread.
            std::unique_ptr<Stream> owned_new_stream(new_stream); // Take ownership immediately

            // Create a new audio object, which takes ownership of the stream.
            audio = std::make_unique<Audio>(std::move(owned_new_stream), fft.get(), mutex.get());
            audio->setVolume(m_volume);

            // Update the player's current stream pointer to reflect the one now owned by Audio
            // This is a raw pointer, for read-only access by Player.
            
            // Update lyrics widget with new track's lyrics
            if (m_lyrics_widget && new_stream) {
                auto lyrics = new_stream->getLyrics();
                if (lyrics && lyrics->hasLyrics()) {
                    Debug::log("lyrics", "Player: Setting lyrics widget with ", lyrics->getLines().size(), " lyric lines");
                } else {
                    Debug::log("lyrics", "Player: No lyrics available for current track");
                }
                m_lyrics_widget->setLyrics(lyrics);
            }
            stream = audio->getCurrentStream();

            updateInfo();
            // Ensure audio is unpaused after everything is set up
            if (audio) audio->play(true);
            state = PlayerState::Playing;
            
            // Start scrobbling for the new track
            startTrackScrobbling();
            
#ifdef HAVE_DBUS
            if (m_mpris_manager) {
                m_mpris_manager->updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus::Playing);
                if (stream) {
                    m_mpris_manager->updateMetadata(
                        stream->getArtist().to8Bit(true), 
                        stream->getTitle().to8Bit(true), 
                        stream->getAlbum().to8Bit(true)
                    );
                }
            }
#endif
#ifdef _WIN32
            if (system) system->announceNowPlaying(stream->getArtist(), stream->getTitle(), stream->getAlbum());
#endif
            break;
        }
        case TRACK_LOAD_FAILURE:
        {
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            TagLib::String error_msg = result->error_message;
            delete result; // Free the result struct

            m_loading_track = false; // Loading complete

            Debug::log("loader", "Player: Failed to load track: ", error_msg.to8Bit(true));

            // Robust playlist handling: skip unplayable tracks
            if (!handleUnplayableTrack()) {
                // All tracks exhausted or stopping due to end of playlist
                stop();
                updateInfo(false, "All tracks in playlist are unplayable.");
            }
            break;
        }
        case TRACK_PRELOAD_SUCCESS:
        {
            // Store the preloaded stream for seamless transition
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            m_preloading_track = false;
            m_next_stream.reset(result->stream); // Take ownership of the preloaded stream
            Debug::log("loader", "Track preloaded successfully for seamless transition.");
            delete result; // Free the result struct but keep the stream
            break;
        }
        case TRACK_PRELOAD_FAILURE:
        {
            // Handle preload failure - no seamless transition possible
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            m_preloading_track = false;
            Debug::log("loader", "Failed to preload track: ", result->error_message.to8Bit(true));
            delete result;
            break;
        }
        case RUN_GUI_ITERATION:
        {
            if (updateGUI()) {
                // Track has ended.
                if (m_loop_mode == LoopMode::One) {
                    // Loop current track by seeking to the beginning.
                    seekTo(0);
                } else if (m_next_stream) {
                    // A track was preloaded, perform seamless swap.
                    synthesizeUserEvent(TRACK_SEAMLESS_SWAP, nullptr, nullptr);
                } else {
                    // No preloaded track, use the old method.
                    nextTrack(m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1);
                }
            }
            break;
        }
        case TRACK_SEAMLESS_SWAP:
        {
            // This event is triggered when a track ends and a preloaded track is ready.
            // Check if we need to recreate the Audio object or can reuse it
            bool recreate_audio = (!audio || (m_next_stream && 
                (static_cast<unsigned int>(audio->getRate()) != m_next_stream->getRate() || 
                 static_cast<unsigned int>(audio->getChannels()) != m_next_stream->getChannels())));

            if (recreate_audio) {
                // Different audio format, need to recreate Audio object
                Debug::log("audio", "Audio format changed, recreating Audio object for seamless transition.");
                audio.reset();
                auto owned_stream = std::move(m_next_stream);
                audio = std::make_unique<Audio>(std::move(owned_stream), fft.get(), mutex.get());
                audio->setVolume(m_volume);
            } else {
                // Same audio format, can seamlessly switch streams
                Debug::log("audio", "Performing seamless stream transition.");
                auto owned_stream = std::move(m_next_stream);
                audio->setStream(std::move(owned_stream));
            }

            // Advance the playlist for the track(s) that just finished
            for (size_t i = 0; i < (m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1); ++i) {
                playlist->next();
            }
            m_num_tracks_in_current_stream = 0;
            
            // Update stream pointer and start scrobbling for new track
            stream = audio->getCurrentStream();
            startTrackScrobbling();

            // Update GUI and lyrics for the new stream
            updateInfo(false, "");
            if (m_lyrics_widget) {
                if (stream) {
                    m_lyrics_widget->setLyrics(stream->getLyrics());
                } else {
                    m_lyrics_widget->clearLyrics();
                }
            }
            break;
        }
        case DO_SAVE_PLAYLIST:
        {
            if (playlist) {
                System::createStoragePath(); // Ensure the directory exists before writing.
                TagLib::String save_path = System::getStoragePath() + "/playlist.m3u";
                playlist->savePlaylist(save_path);
                showToast("Current playlist saved!");
            }
            break;
        }
        case SHOW_MPRIS_ERROR:
        {
            std::string* msg_ptr = static_cast<std::string*>(event.data1);
            if (msg_ptr) {
                if (m_show_mpris_errors) {
                    showToast("MPRIS Error:\n" + *msg_ptr, 4000);
                }
                delete msg_ptr;
            }
            break;
        }
        case DO_SET_LOOP_MODE:
        {
            LoopMode mode = static_cast<LoopMode>(reinterpret_cast<intptr_t>(event.data1));
            m_loop_mode = mode;
            std::string toastMsg = "Loop: ";
            std::string mprisStatus;
            PsyMP3::MPRIS::LoopStatus mprisEnum = PsyMP3::MPRIS::LoopStatus::None;
            switch(m_loop_mode) {
                case LoopMode::None:
                    toastMsg += "None";
                    mprisEnum = PsyMP3::MPRIS::LoopStatus::None;
                    break;
                case LoopMode::One:
                    toastMsg += "One";
                    mprisEnum = PsyMP3::MPRIS::LoopStatus::Track;
                    break;
                case LoopMode::All:
                    toastMsg += "All";
                    mprisEnum = PsyMP3::MPRIS::LoopStatus::Playlist;
                    break;
            }
            showToast(toastMsg);
#ifdef HAVE_DBUS
            if (m_mpris_manager) {
                 m_mpris_manager->updateLoopStatus(mprisEnum);
            }
#endif
            break;
        }
    }
    return false; // Do not exit
}

/**
 * @brief The main entry point and run loop for the Player.
 * This function initializes SDL and all major components (display, fonts, UI),
 * starts the main event loop, and handles cleanup on exit.
 * @param args The vector of command-line arguments passed to the application.
 */
void Player::Run(const PlayerOptions& options) {
    // Apply settings from the parsed options.
    scalefactor = options.scalefactor;
    decayfactor = options.decayfactor;
    m_automated_test_mode = options.automated_test_mode;
    m_unattended_quit = options.unattended_quit;
    m_show_mpris_errors = options.show_mpris_errors;

    // initialize SDL video
    if ( SDL_Init( SDL_INIT_EVERYTHING ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return;
    }



    Debug::log("system", "System::getStoragePath: ", System::getStoragePath().to8Bit(true));
    Debug::log("system", "System::getUser: ", System::getUser().to8Bit(true));
    Debug::log("system", "System::getHome: ", System::getHome().to8Bit(true));
#ifdef _WIN32
    Debug::log("system", "System::getHwnd: ", std::hex, System::getHwnd());
#endif /* _WIN32 */

    TrueType::Init();
    // FastFourier::init();

    // Initialize UI and essential components first to show the window quickly.
    screen = std::make_unique<Display>();
    system = std::make_unique<System>();
#ifdef _WIN32
    system->InitializeIPC(this);
#endif
#if defined(_WIN32)
    // Try multiple paths on Windows: current directory first, then res/ subdirectory
    font = std::make_unique<Font>("./vera.ttf");
    if (!font->isValid()) {
        font = std::make_unique<Font>("./res/vera.ttf");
    }
#else
    font = std::make_unique<Font>(TagLib::String(PSYMP3_DATADIR "/vera.ttf"), 12);
#endif // _WIN32
    
    // Create a larger font for status indicators like the pause message.
#if defined(_WIN32)
    // Try multiple paths on Windows for large font too
    m_large_font = std::make_unique<Font>("./vera.ttf", 36);
    if (!m_large_font->isValid()) {
        m_large_font = std::make_unique<Font>("./res/vera.ttf", 36);
    }
#else
    m_large_font = std::make_unique<Font>(TagLib::String(PSYMP3_DATADIR "/vera.ttf"), 36);
#endif // _WIN32
    Debug::log("font", "font->isValid(): ", font->isValid());
    
    graph = std::make_unique<Surface>(640, 400);
    // Enable alpha blending for the graph surface itself. This is crucial for it to be a valid
    // destination for other alpha-blended surfaces (like the fade effect, toasts, etc.).
    graph->SetAlpha(SDL_SRCALPHA, 255);
    precomputeSpectrumColors();

    // Create an empty playlist. It will be populated in the background.
    playlist = std::make_unique<Playlist>();

    fft = std::make_unique<FastFourier>();
    mutex = std::make_unique<std::mutex>();

    // Set FFT mode after FFT object is created
    fft->setFFTMode(options.fft_mode);

    // Initialize the ApplicationWidget as the root of the widget tree
    ApplicationWidget& app_widget = ApplicationWidget::getInstance(*screen);
    m_ui_root = &app_widget; // Reference to singleton - not owned
    
    // Add UI elements directly to ApplicationWidget (acts as the desktop/background)
    
    // Create a spectrum analyzer widget and add it to ApplicationWidget
    auto spectrum_widget = std::make_unique<SpectrumAnalyzerWidget>(640, 350);
    spectrum_widget->setPos(Rect(0, 0, 640, 350));
    m_spectrum_widget = spectrum_widget.get(); // Keep raw pointer for updates
    app_widget.addChild(std::move(spectrum_widget)); // ApplicationWidget takes ownership
    
    // Create progress bar frame widget with hierarchical structure and add to ApplicationWidget
    auto progress_frame_widget = std::make_unique<ProgressBarFrameWidget>();
    progress_frame_widget->setPos(Rect(399, 370, 222, 16)); // Original frame position
    m_progress_widget = progress_frame_widget->getProgressBar(); // Get nested progress bar
    app_widget.addChild(std::move(progress_frame_widget)); // ApplicationWidget takes ownership
    
    // Set up progress bar callbacks for seeking
    m_progress_widget->setOnSeekStart([this](double progress) {
        m_is_dragging = true;
        if (stream) {
            m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress);
        }
    });
    
    m_progress_widget->setOnSeekUpdate([this](double progress) {
        if (stream && m_is_dragging) {
            m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress);
        }
    });
    
    m_progress_widget->setOnSeekEnd([this](double progress) {
        if (stream && m_is_dragging) {
            m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress);
            seekTo(m_drag_position_ms);
        }
        m_is_dragging = false;
    });
    
    // Helper lambda to reduce boilerplate when creating labels and adding them to ApplicationWidget
    auto add_label = [&](const std::string& key, const Rect& pos) {
        auto label = std::make_unique<Label>(font.get(), pos);
        m_labels[key] = label.get(); // Store non-owning pointer in map
        app_widget.addChild(std::move(label)); // ApplicationWidget takes ownership
    };

    add_label("artist",   Rect(1, 354, 200, 16));
    add_label("title",    Rect(1, 369, 200, 16));
    add_label("album",    Rect(1, 384, 200, 16));
    add_label("playlist", Rect(270, 354, 120, 16));
    add_label("position", Rect(400, 353, 150, 16));
    add_label("scale",    Rect(545, 0, 95, 16));
    add_label("decay",    Rect(545, 15, 95, 16));
    add_label("fft_mode", Rect(545, 30, 95, 16));
    m_loop_mode = LoopMode::None; // Default loop mode on startup

    // Initialize lyrics widget and add to application window system
    auto lyrics_widget = std::make_unique<LyricsWidget>(font.get(), 640);
    m_lyrics_widget = lyrics_widget.get();
    app_widget.addWindow(std::move(lyrics_widget), ZOrder::UI);

    // Set up the shared data struct for the audio thread.
    // The stream pointer will be null initially.
    

    // If command line arguments are provided, start populating the playlist
    // and load the first track in a background thread.
    if (!options.files.empty()) {
        m_playlist_populator_thread = std::thread(&Player::playlistPopulatorLoop, this, options.files);
        state = PlayerState::Stopped; // Will transition to playing when track loads
    } else {
        // No files, start with stopped state and an empty screen.
        state = PlayerState::Stopped;
        updateInfo();
        // Force one GUI update to show the initial empty state
        synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr);
    }
    bool done = false;
    // if (system) system->progressState(TBPF_NORMAL);
    if (m_automated_test_mode) {
        Debug::log("test", "Automated test mode enabled.");
    }
    SDL_TimerID timer = SDL_AddTimer(33, Player::AppLoopTimer, nullptr);
    if (m_automated_test_mode) {
        m_automated_test_timer_id = SDL_AddTimer(1000, Player::AutomatedTestTimer, this);
    }
    while (!done) {
        // message processing loop
        SDL_Event event;
        while (SDL_WaitEvent(&event)) {
            switch (event.type) {
                // exit if the window is closed
            case SDL_QUIT:
                done = true;
                break;

                // check for keypresses
            case SDL_KEYDOWN:
            {
                done = handleKeyPress(event.key.keysym);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                handleWindowMouseEvents(event);
                
                if (m_use_widget_mouse_handling) {
                    // Try widget tree first
                    bool handled = false;
                    if (m_ui_root) {
                        handled = m_ui_root->handleMouseDown(event.button, event.button.x, event.button.y);
                    }
                    
                    // Fall back to old handler if widget tree didn't handle it
                    if (!handled) {
                        handleMouseButtonDown(event.button);
                    }
                } else {
                    // Legacy mode: use only old handlers
                    handleMouseButtonDown(event.button);
                }
                break;
            }
            case SDL_MOUSEMOTION:
            {
                handleWindowMouseEvents(event);
                
                if (m_use_widget_mouse_handling) {
                    // Try widget tree first
                    bool handled = false;
                    if (m_ui_root) {
                        handled = m_ui_root->handleMouseMotion(event.motion, event.motion.x, event.motion.y);
                    }
                    
                    // Fall back to old handler if widget tree didn't handle it
                    if (!handled) {
                        handleMouseMotion(event.motion);
                    }
                } else {
                    // Legacy mode: use only old handlers
                    handleMouseMotion(event.motion);
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                handleWindowMouseEvents(event);
                
                if (m_use_widget_mouse_handling) {
                    // Try widget tree first
                    bool handled = false;
                    if (m_ui_root) {
                        handled = m_ui_root->handleMouseUp(event.button, event.button.x, event.button.y);
                    }
                    
                    // Fall back to old handler if widget tree didn't handle it
                    if (!handled) {
                        handleMouseButtonUp(event.button);
                    }
                } else {
                    // Legacy mode: use only old handlers
                    handleMouseButtonUp(event.button);
                }
                break;
            }
            case SDL_KEYUP:
            {
                handleKeyUp(event.key.keysym);
                break;
            }
            case SDL_USEREVENT:
            {
                if (event.user.code == AUTOMATED_SKIP_TRACK) {
                    nextTrack();
                } else if (event.user.code == QUIT_APPLICATION) {
                    done = true;
                } else {
                    done = handleUserEvent(event.user);
                }
                break;
            }
            } // end switch (event.type)
            processDeferredDeletions();
            if (done) break;

        } // end of message processing

    } // end main loop

    SDL_RemoveTimer(timer);
#ifdef _WIN32
    if (system) system->progressState(TBPF_NOPROGRESS);
    if (system) system->updateProgress(0, 0);
#endif
    if (audio) audio->play(false);

    // all is well ;)
    Debug::log("player", "Exited cleanly");
    SDL_Quit();
    return;
}

/**
 * @brief Sets the player loop mode.
 * This method is thread-safe and dispatches an event to the main thread.
 * @param mode The new loop mode to set.
 */
void Player::setLoopMode(LoopMode mode) {
    synthesizeUserEvent(DO_SET_LOOP_MODE, reinterpret_cast<void*>(static_cast<intptr_t>(mode)), nullptr);
}

/**
 * @brief Gets the current loop mode.
 * This method is thread-safe as m_loop_mode is atomic.
 * @return The current LoopMode.
 */
LoopMode Player::getLoopMode() const {
    return m_loop_mode;
}

// Static member function definitions for automated testing
Uint32 Player::AutomatedTestTimer(Uint32 interval, void* param) {
    Player* player = static_cast<Player*>(param);
    if (player) {
        player->synthesizeUserEvent(AUTOMATED_SKIP_TRACK, nullptr, nullptr);
        player->m_automated_test_track_count++;
        if (player->m_automated_test_track_count >= 5) { // Skip 5 tracks then quit
            SDL_RemoveTimer(player->m_automated_test_timer_id);
            player->m_automated_test_timer_id = 0;
            player->m_automated_quit_timer_id = SDL_AddTimer(1000, Player::AutomatedQuitTimer, player);
        }
    }
    return interval;
}

Uint32 Player::AutomatedQuitTimer(Uint32 interval, void* param) {
    Player* player = static_cast<Player*>(param);
    if (player) {
        player->synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
    }
    return interval;
}

bool Player::handleUnplayableTrack() {
    if (!playlist || playlist->entries() == 0) {
        return false; // No playlist
    }

    m_skip_attempts++;
    if (m_skip_attempts >= playlist->entries()) {
        // We've tried every track and failed. Give up.
        m_skip_attempts = 0; // Reset for next time
        return false;
    }

    // Try the next track in the current direction
    if (m_navigation_direction > 0) {
        // Moving forward
        long new_pos = playlist->getPosition() + 1;
        if (new_pos >= playlist->entries()) {
            // Reached end of playlist
            if (m_loop_mode == LoopMode::All) {
                new_pos = 0; // Wrap to beginning
            } else {
                // End of playlist in LoopMode::None - stop
                m_skip_attempts = 0;
                return false;
            }
        }
        playlist->setPosition(new_pos);
        requestTrackLoad(playlist->getTrack(new_pos));
    } else {
        // Moving backward
        long new_pos = playlist->getPosition() - 1;
        if (new_pos < 0) {
            // Reached beginning of playlist
            if (m_loop_mode == LoopMode::All) {
                new_pos = playlist->entries() - 1; // Wrap to end
            } else {
                // Beginning of playlist in LoopMode::None - stop
                m_skip_attempts = 0;
                return false;
            }
        }
        playlist->setPosition(new_pos);
        requestTrackLoad(playlist->getTrack(new_pos));
    }

    return true; // Continue trying
}

bool Player::findFirstPlayableTrack() {
    if (!playlist || playlist->entries() == 0) {
        return false; // No playlist
    }

    // Try to find the first playable track starting from position 0
    for (size_t i = 0; i < static_cast<size_t>(playlist->entries()); ++i) {
        playlist->setPosition(i);
        TagLib::String track_path = playlist->getTrack(i);
        
        // Try to create a stream for this track using MediaFile::open
        try {
            auto test_stream = MediaFile::open(track_path);
            if (test_stream) {
                // Found a playable track - unique_ptr handles cleanup automatically
                m_skip_attempts = 0;
                requestTrackLoad(track_path);
                return true;
            }
        } catch (...) {
            // Track failed to load, continue to next
        }
    }

    // No playable tracks found
    return false;
}

/**
 * @brief Renders all floating windows in z-order.
 */
void Player::renderWindows()
{
    // Sort windows by z-order (lowest to highest)
    std::vector<WindowFrameWidget*> sorted_windows;
    
    if (m_test_window_h) {
        sorted_windows.push_back(m_test_window_h.get());
    }
    
    if (m_test_window_b) {
        sorted_windows.push_back(m_test_window_b.get());
    }
    
    // Add all random windows
    for (const auto& window : m_random_windows) {
        sorted_windows.push_back(window.get());
    }
    
    std::sort(sorted_windows.begin(), sorted_windows.end(),
              [](const WindowFrameWidget* a, const WindowFrameWidget* b) {
                  return a->getZOrder() < b->getZOrder();
              });
    
    // Render windows in z-order
    for (auto* window : sorted_windows) {
        window->BlitTo(*graph);
    }
}

/**
 * @brief Handles mouse events for windows.
 * @param event The SDL event to handle
 */
void Player::handleWindowMouseEvents(const SDL_Event& event)
{
    // Create list of windows sorted by z-order (highest first for event handling)
    std::vector<WindowFrameWidget*> sorted_windows;
    
    if (m_test_window_h) {
        sorted_windows.push_back(m_test_window_h.get());
    }
    
    if (m_test_window_b) {
        sorted_windows.push_back(m_test_window_b.get());
    }
    
    // Add all random windows
    for (const auto& window : m_random_windows) {
        sorted_windows.push_back(window.get());
    }
    
    std::sort(sorted_windows.begin(), sorted_windows.end(),
              [](const WindowFrameWidget* a, const WindowFrameWidget* b) {
                  return a->getZOrder() > b->getZOrder();
              });
    
    // Handle events for windows (front to back)
    for (auto* window : sorted_windows) {
        if (!window) continue;
        
        Rect window_rect = window->getPos();
        int mouse_x = 0, mouse_y = 0;
        
        // Get mouse coordinates based on event type
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            mouse_x = event.button.x;
            mouse_y = event.button.y;
        } else if (event.type == SDL_MOUSEMOTION) {
            mouse_x = event.motion.x;
            mouse_y = event.motion.y;
        } else {
            continue;
        }
        
        // Check if this window has mouse capture or if mouse is within window bounds
        bool has_capture = (Widget::getMouseCapturedWidget() == window);
        bool in_bounds = (mouse_x >= window_rect.x() && mouse_x < window_rect.x() + window_rect.width() &&
                         mouse_y >= window_rect.y() && mouse_y < window_rect.y() + window_rect.height());
        
        if (has_capture || in_bounds) {
            int relative_x = mouse_x - window_rect.x();
            int relative_y = mouse_y - window_rect.y();
            
            bool handled = false;
            
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                handled = window->handleMouseDown(event.button, relative_x, relative_y);
            } else if (event.type == SDL_MOUSEMOTION) {
                handled = window->handleMouseMotion(event.motion, relative_x, relative_y);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                handled = window->handleMouseUp(event.button, relative_x, relative_y);
            }
            
            if (handled) {
                break; // Event consumed by this window
            }
        }
    }
}

/**
 * @brief Toggles the test window H (160x120 white window).
 */
void Player::toggleTestWindowH()
{
    if (m_test_window_h) {
        // Close the window
        m_test_window_h.reset();
        showToast("Test Window H: Closed");
    } else {
        // Open the window (client area is 160x120)
        m_test_window_h = std::make_unique<WindowFrameWidget>(160, 120, "Test Window H");
        
        // Only set position, keep the calculated size from constructor
        Rect calculated_size = m_test_window_h->getPos();
        m_test_window_h->setPos(Rect(150, 150, calculated_size.width(), calculated_size.height()));
        
        // Set up drag callbacks
        m_test_window_h->setOnDrag([this](int dx, int dy) {
            Rect current_pos = m_test_window_h->getPos();
            current_pos.x(current_pos.x() + dx);
            current_pos.y(current_pos.y() + dy);
            m_test_window_h->setPos(current_pos);
        });
        
        m_test_window_h->setOnDragStart([this]() {
            m_test_window_h->bringToFront();
        });
        
        // Set up window control callbacks
        m_test_window_h->setOnClose([this]() {
            if (m_test_window_h) {
                deferWidgetDeletion(std::move(m_test_window_h));
                m_test_window_h = nullptr;
            }
            showToast("Test Window H: Closed");
        });
        
        m_test_window_h->setOnMinimize([this]() {
            showToast("Test Window H: Minimized");
        });
        
        m_test_window_h->setOnMaximize([this]() {
            showToast("Test Window H: Maximized");
        });
        
        m_test_window_h->setOnControlMenu([this]() {
            showToast("Test Window H: Control Menu");
        });
        
        m_test_window_h->setOnResize([this](int new_width, int new_height) {
            showToast("Test Window H: Resized to " + std::to_string(new_width) + "x" + std::to_string(new_height));
        });
        
        showToast("Test Window H: Opened");
    }
}

/**
 * @brief Toggles the test window B (160x60 window).
 */
void Player::toggleTestWindowB()
{
    if (m_test_window_b) {
        // Close the window
        m_test_window_b.reset();
        showToast("Test Window B: Closed");
    } else {
        // Open the window (client area is 160x60)
        m_test_window_b = std::make_unique<WindowFrameWidget>(160, 60, "Test Window B");
        m_test_window_b->setResizable(false); // Make window B non-resizable (triggers refresh)
        
        // Only set position, keep the calculated size from setResizable(false)
        Rect calculated_size = m_test_window_b->getPos();
        m_test_window_b->setPos(Rect(200, 200, calculated_size.width(), calculated_size.height()));
        
        // Set up drag callbacks
        m_test_window_b->setOnDrag([this](int dx, int dy) {
            Rect current_pos = m_test_window_b->getPos();
            current_pos.x(current_pos.x() + dx);
            current_pos.y(current_pos.y() + dy);
            m_test_window_b->setPos(current_pos);
        });
        
        m_test_window_b->setOnDragStart([this]() {
            m_test_window_b->bringToFront();
        });
        
        // Set up window control callbacks
        m_test_window_b->setOnClose([this]() {
            showToast("Test Window B: Closed");
            this->deferDelete(m_test_window_b);
        });
        
        m_test_window_b->setOnMinimize([this]() {
            showToast("Test Window B: Minimized");
        });
        
        m_test_window_b->setOnMaximize([this]() {
            showToast("Test Window B: Maximized");
        });
        
        m_test_window_b->setOnControlMenu([this]() {
            showToast("Test Window B: Control Menu");
        });
        
        m_test_window_b->setOnResize([this](int new_width, int new_height) {
            showToast("Test Window B: Resized to " + std::to_string(new_width) + "x" + std::to_string(new_height));
        });
        
        showToast("Test Window B: Opened");
    }
}

void Player::createRandomWindows()
{
    // Create 5 random windows each time J is pressed
    for (int i = 0; i < 5; i++) {
        // Generate random window properties for client area
        int client_width = 100 + (rand() % 200);   // 100-300px wide client area
        int client_height = 80 + (rand() % 150);   // 80-230px tall client area
        int x = rand() % 400;                      // Random X position
        int y = rand() % 300;                      // Random Y position
        
        // Create WindowFrameWidget directly like H and B windows
        std::string title = "Random Window " + std::to_string(++m_random_window_counter);
        auto window = std::make_unique<WindowFrameWidget>(client_width, client_height, title);
        window->setPos(Rect(x, y, client_width + 8, client_height + 27)); // Include frame borders
        
        // Set up callbacks using the WindowFrameWidget system like H/B windows
        window->setOnClose([this, window_ptr = window.get()]() {
            // Remove from our vector and decrement counter
            auto it = std::find_if(m_random_windows.begin(), m_random_windows.end(),
                                  [window_ptr](const auto& w) { return w.get() == window_ptr; });
            if (it != m_random_windows.end()) {
                m_random_windows.erase(it);
            }
            m_random_window_counter--;
            showToast("Random window closed (Total: " + std::to_string(m_random_window_counter) + ")");
        });
        
        window->setOnDrag([window_ptr = window.get()](int dx, int dy) {
            Rect current_pos = window_ptr->getPos();
            current_pos.x(current_pos.x() + dx);
            current_pos.y(current_pos.y() + dy);
            window_ptr->setPos(current_pos);
        });
        
        window->setOnDragStart([window_ptr = window.get()]() {
            window_ptr->bringToFront();
        });
        
        // Add to our random windows vector (managed by Player like H/B windows)
        m_random_windows.push_back(std::move(window));
    }
    
    showToast("Created 5 random windows (Total: " + std::to_string(m_random_window_counter) + ")");
}

void Player::checkScrobbling()
{
    if (!stream || !m_lastfm || m_track_scrobbled || m_track_start_time == 0) {
        return;
    }
    
    Uint32 current_time = SDL_GetTicks();
    Uint32 elapsed_ms = current_time - m_track_start_time;
    
    // Scrobble criteria: track played >50% OR >4 minutes (240000ms)
    unsigned long track_length_ms = stream->getLength();
    unsigned long required_time = std::min(track_length_ms / 2, 240000UL);
    
    if (elapsed_ms >= required_time && track_length_ms > 30000) { // Only scrobble tracks >30 seconds
        // Create track object from stream data for scrobbling
        // Use a dummy path since we're creating this for metadata only
        track scrobble_track(TagLib::String(""), stream->getArtist(), stream->getTitle());
        scrobble_track.SetLen(static_cast<unsigned int>(track_length_ms / 1000)); // Convert to seconds
        
        if (m_lastfm->scrobbleTrack(scrobble_track)) {
            m_track_scrobbled = true;
            std::cout << "Player: Scrobbled track: " << stream->getArtist() << " - " << stream->getTitle() << std::endl;
        }
    }
}

void Player::startTrackScrobbling()
{
    m_track_start_time = SDL_GetTicks();
    m_track_scrobbled = false;
    
    // Submit "Now Playing" status
    submitNowPlaying();
}

void Player::submitNowPlaying()
{
    if (!stream || !m_lastfm) {
        return;
    }
    
    // Create track object from stream data
    // Use a dummy path since we're creating this for metadata only
    track now_playing_track(TagLib::String(""), stream->getArtist(), stream->getTitle());
    now_playing_track.SetLen(static_cast<unsigned int>(stream->getLength() / 1000)); // Convert to seconds
    
    m_lastfm->setNowPlaying(now_playing_track);
}

void Player::setVolume(double volume) {
    if (volume < 0.0) volume = 0.0;
    if (volume > 1.0) volume = 1.0;
    m_volume = static_cast<float>(volume);

    if (audio) {
        audio->setVolume(m_volume);
    }

    // Show toast with new volume percentage
    int percentage = static_cast<int>(m_volume * 100 + 0.5f); // Round to nearest integer
    showToast("Volume: " + std::to_string(percentage) + "%");

#ifdef HAVE_DBUS
    // TODO: Notify MPRIS about volume change (PropertiesChanged signal)
    // For now, MPRIS clients polling the property will see the new value
#endif
}

double Player::getVolume() const {
    return static_cast<double>(m_volume);
}

void Player::processDeferredDeletions() {
    m_deferred_widgets.clear();
}

void Player::deferWidgetDeletion(std::unique_ptr<Widget> widget) {
    if (widget) {
        m_deferred_widgets.push_back(std::move(widget));
    }
}
