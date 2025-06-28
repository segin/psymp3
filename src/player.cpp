/*
 * player.cpp - class implementation for player class
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

#include "psymp3.h"

bool Player::guiRunning;

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
    //ctor - you'd think we'd initialize our object pointers here,
    // but no, some depend on SDL, which is initialized in Run()
    // -- but we will delete them in ~Player()
    // So, instead, print a startup banner to the console.
    std::cout << "PsyMP3 version " << PSYMP3_VERSION << "." << std::endl;

    m_loader_active = true;
    m_loading_track = false;
    m_preloading_track = false;
    m_loader_thread = std::thread(&Player::loaderThreadLoop, this);
}

Player::~Player() {
    // Signal threads to stop and wait for them to finish *before*
    // destroying any resources they might be using. This prevents use-after-free.
    if (m_loader_active) {
        m_loader_active = false;
        m_loader_queue_cv.notify_one(); // Wake up loader thread to exit
        if (m_loader_thread.joinable()) {
            m_loader_thread.join();
        }
    }

    // Join the playlist populator thread.
    // This thread might be accessing 'playlist', so we must join it before deleting.
    if (m_playlist_populator_thread.joinable()) {
        m_playlist_populator_thread.join();
    }
}

/* SDL event synthesis */
void Player::synthesizeKeyEvent(SDLKey kpress) {
    SDL_Event event;
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = kpress;
    SDL_PushEvent(&event);
    event.type = SDL_KEYUP;
    SDL_PushEvent(&event);
}

void Player::synthesizeUserEvent(int code, void *data1, void* data2) {
    SDL_Event event;

    event.type = SDL_USEREVENT;
    event.user.code = code;
    event.user.data1 = data1;
    event.user.data2 = data2;

    SDL_PushEvent(&event);
}

Uint32 Player::AppLoopTimer(Uint32 interval, void* param) {
    if (!Player::guiRunning)
        synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr);
    else
        std::cout << "timer: skipped" << std::endl;

    return interval;
}

/* If we're stopped or paused when a new track is requested, we should
 * switch to playing - this is consistent with the majority of players
 * on the market.
 */
void Player::requestTrackLoad(TagLib::String path) {
    if (m_loading_track) {
        std::cerr << "Already loading a track, ignoring new request: " << path << std::endl;
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

void Player::requestTrackPreload(const TagLib::String& path) {
    if (m_loading_track || m_preloading_track || m_next_stream) return;
    m_preloading_track = true;
    {
        std::lock_guard<std::mutex> lock(m_loader_queue_mutex);
        m_loader_queue.push({LoadRequestType::Preload, path, {}});
    }
    m_loader_queue_cv.notify_one();
}

void Player::requestChainedStreamLoad(const std::vector<TagLib::String>& paths) {
    if (m_loading_track || m_preloading_track || m_next_stream) return;
    m_preloading_track = true;
    {
        std::lock_guard<std::mutex> lock(m_loader_queue_mutex);
        m_loader_queue.push({LoadRequestType::PreloadChained, "", paths});
    }
    m_loader_queue_cv.notify_one();
}

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
                    new_stream = MediaFile::open(request.path);
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

void Player::playlistPopulatorLoop(std::vector<std::string> args) {
    System::setThisThreadName("playlist-populator");

    if (args.size() <= 1) return; // Nothing to do

    // Check if the first argument is a playlist file (M3U/M3U8)
    TagLib::String first_arg(args[1], TagLib::String::UTF8);
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
                std::cerr << "Failed to load or empty playlist: " << first_arg << std::endl;
            }
        }
    }

    // If not a playlist or loading failed, treat arguments as individual files
    for (size_t i = 1; i < args.size(); ++i) {
        playlist->addFile(TagLib::String(args[i], TagLib::String::UTF8));
        if (i == 1) synthesizeUserEvent(START_FIRST_TRACK, nullptr, nullptr); // Start first track
    }
}

/* Player control functions */
bool Player::nextTrack(size_t advance_count) {
    if (advance_count == 0) advance_count = 1; // Must advance at least once.

    TagLib::String nextfile;
    for(size_t i = 0; i < advance_count; ++i) {
        nextfile = playlist->next();
    }
    if (nextfile.isEmpty()) {
        return false;
    } else {
        requestTrackLoad(nextfile);
    }
    return true;
}

bool Player::prevTrack(void) {
    requestTrackLoad(playlist->prev());
    return true;
}

bool Player::stop(void) {
    state = PlayerState::Stopped;
    stream.reset();
    return true;
}

bool Player::pause(void) {
    if (state != PlayerState::Stopped) {
        audio->play(false);
        state = PlayerState::Paused;
        return true;
    } else {
        return false;
    }
}

bool Player::play(void) {
    audio->play(true);
    state = PlayerState::Playing;
    return true;
}

bool Player::playPause(void) {
    if (state == PlayerState::Playing) {
        pause();
    } else {
        // This handles both Stopped and Paused states correctly.
        play();
    }
    return true;
}

void Player::seekTo(unsigned long pos)
{
    std::lock_guard<std::mutex> lock(*mutex);
    if (stream) {
        stream->seekTo(pos);
    }
}

void Player::precomputeSpectrumColors() {
    if (!graph) return;

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
        m_spectrum_colors[x] = graph->MapRGBA(r, g, b, 255);
    }
}

/* Internal UI compartments */

void Player::renderSpectrum(Surface *graph) {
    float *spectrum = fft->getFFT();

    // --- Fade effect implementation ---
    // Create a temporary surface for the fade effect.
    // It's crucial that this surface supports alpha (e.g., 32-bit).
    // Using 'static' to avoid repeated allocation/deallocation per frame.
    static std::unique_ptr<Surface> fade_surface_ptr; // Declared static to persist across calls
    if (!fade_surface_ptr || fade_surface_ptr->width() != 640 || fade_surface_ptr->height() != 350) {
        fade_surface_ptr = std::make_unique<Surface>(640, 350); // Creates a 32-bit surface for FFT area only
    }
    Surface& fade_surface = *fade_surface_ptr;

    // Calculate alpha for the fade (0-255). decayfactor from 0.5 to 2.0
    uint8_t fade_alpha = (uint8_t)(255 * (decayfactor / 4.0f)); // Even slower fade: divisor increased from 3.0 to 4.0

    // Fill the fade surface with black, using the calculated alpha
    fade_surface.FillRect(fade_surface.MapRGB(0, 0, 0)); // Fill with opaque black

    // Set alpha blending for the fade surface (source surface for blitting)
    fade_surface.SetAlpha(SDL_SRCALPHA, fade_alpha);
    
    // Blit the semi-transparent black fade_surface onto the graph surface at (0,0)
    Rect blit_dest_rect(0, 0, 640, 350); // Blit only to the FFT area
    graph->Blit(fade_surface, blit_dest_rect); // This will blend if SDL_SRCALPHA is set on src

    // --- End Fade effect implementation ---

    // float *spectrum = fft->getFFT(); // Removed redundant declaration
    for(uint16_t x=0; x < 320; x++) {
        // Calculate the bar's height
        int16_t y_start = static_cast<int16_t>(350 - (Util::logarithmicScale(scalefactor, spectrum[x]) * 350.0f));
        // Draw the rectangle using the precomputed color
        graph->rectangle(x * 2, y_start, (x * 2) + 1, 350, m_spectrum_colors[x]);
    };
}

void Player::updateInfo(bool is_loading, const TagLib::String& error_msg)
{
    if (is_loading) {
        m_labels.at("artist")->setText("Artist: Loading...");
        m_labels.at("title")->setText("Title: Loading...");
        m_labels.at("album")->setText("Album: Loading...");
        m_labels.at("position")->setText("Position: --:--.-- / --:--.--");
        screen->SetCaption(TagLib::String("PsyMP3 ") + PSYMP3_VERSION + " -:[ Loading... ]:-", TagLib::String("PsyMP3 ") + PSYMP3_VERSION);
    } else if (!error_msg.isEmpty()) {
        m_labels.at("artist")->setText("Artist: N/A");
        m_labels.at("title")->setText("Title: Error: " + error_msg);
        m_labels.at("album")->setText("Album: N/A");
        m_labels.at("playlist")->setText("Playlist: N/A");
        m_labels.at("position")->setText("Position: --:--.-- / --:--.--");
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ Error: " + error_msg.to8Bit(true) + " ]:-", "PsyMP3 " PSYMP3_VERSION);
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
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ]:-", "PsyMP3 " PSYMP3_VERSION);
    }

    // These are always updated based on player settings, not track info
    m_labels.at("scale")->setText("log scale = " + std::to_string(scalefactor));
    m_labels.at("decay")->setText("decay = " + std::to_string(decayfactor));
    m_labels.at("fft_mode")->setText("FFT Mode: " + fft->getFFTModeName());
}

bool Player::updateGUI()
{
    Player::guiRunning = true;
    unsigned long current_pos_ms = 0;
    unsigned long total_len_ms = 0;
    TagLib::String artist, title;

    // --- GUI Update Logic ---
    // --- Start of critical section ---
    // Lock the mutex only while accessing shared data (stream, fft).
    {
        std::lock_guard<std::mutex> lock(*mutex);

        // The fade effect in renderSpectrum handles clearing the spectrum area.
        // We only need to manually clear the bottom part of the graph surface
        // where the progress bar and other info will be drawn.
        Rect bottom_clear_rect(0, 351, graph->width(), graph->height() - 351);
        graph->box(bottom_clear_rect.x(), bottom_clear_rect.y(),
                    bottom_clear_rect.x() + bottom_clear_rect.width() - 1,
                    bottom_clear_rect.y() + bottom_clear_rect.height() - 1,
                    graph->MapRGB(0, 0, 0));

        // Copy data from stream object while locked
        if (stream) {
            current_pos_ms = stream->getPosition();
            total_len_ms = stream->getLength();
            artist = stream->getArtist();
            title = stream->getTitle();
        }

        // Draw the spectrum analyzer on the graph surface
        this->renderSpectrum(graph.get());
    }
    // --- End of critical section ---

    // Now use the copied data for rendering, outside the lock.
    if(stream) {
        m_labels.at("position")->setText("Position: " + convertInt(current_pos_ms / 60000)
                                + ":" + convertInt2((current_pos_ms / 1000) % 60)
                                + "." + convertInt2((current_pos_ms / 10) % 100)
                                + "/" + convertInt(total_len_ms / 60000)
                                + ":" + convertInt2((total_len_ms / 1000) % 60)
                                + "." + convertInt2((total_len_ms / 10) % 100));
        screen->SetCaption("PsyMP3 " PSYMP3_VERSION +
                        (std::string) " -:[ " + artist.to8Bit(true) + " ]:- -- -:[ " +
                        title.to8Bit(true) + " ]:-", "PsyMP3 " PSYMP3_VERSION);
    } else {
        m_labels.at("position")->setText("Position: -:--.-- / -:--.--");
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ]:-", "PsyMP3 " PSYMP3_VERSION);
    }
    
    // --- Pre-loading logic ---
    const Uint32 PRELOAD_THRESHOLD_MS = 5000; // 5 seconds
    if (stream && total_len_ms > 0 && !m_next_stream && !m_loading_track && !m_preloading_track &&
        (total_len_ms - current_pos_ms < PRELOAD_THRESHOLD_MS))
    {
        long playlist_size = playlist->entries();
        if (playlist_size > 0) {
            long next_pos_idx = (playlist->getPosition() + 1) % playlist_size;
            const track* next_track_info = playlist->getTrackInfo(next_pos_idx);

            if (next_track_info) {
                // Check for "hidden track" sequences (short tracks <= 5s)
                if (next_track_info->GetLen() > 0 && next_track_info->GetLen() <= 5) {
                    std::vector<TagLib::String> track_chain;
                    long total_chain_duration_sec = 0;
                    long lookahead_idx = next_pos_idx;

                    // Scan forward for a chain of short tracks, but don't wrap around the playlist.
                    while (lookahead_idx < playlist_size && total_chain_duration_sec < 120) {
                        const track* current_lookahead_track = playlist->getTrackInfo(lookahead_idx);
                        if (current_lookahead_track && current_lookahead_track->GetLen() > 0 && current_lookahead_track->GetLen() <= 5) {
                            track_chain.push_back(current_lookahead_track->GetFilePath());
                            total_chain_duration_sec += current_lookahead_track->GetLen();
                            lookahead_idx++;
                        } else {
                            break; // The chain of short tracks ends here.
                        }
                    }
                    
                    if (!track_chain.empty()) {
                        requestChainedStreamLoad(track_chain);
                    }
                } else {
                    // It's a normal track, use the standard preload.
                    TagLib::String next_path = playlist->peekNext();
                    if (!next_path.isEmpty()) {
                        requestTrackPreload(next_path);
                    }
                }
            }
        }
    }

    // draw progress bar on the graph surface
    graph->vline(399, 370, 385, 0xFFFFFFFF);
    graph->vline(621, 370, 385, 0xFFFFFFFF);
    graph->hline(399, 402, 370, 0xFFFFFFFF);
    graph->hline(399, 402, 385, 0xFFFFFFFF);
    graph->hline(618, 621, 370, 0xFFFFFFFF);
    graph->hline(618, 621, 385, 0xFFFFFFFF);
    if (m_seek_direction == 1) {
        if (stream)
            stream->seekTo((long long) stream->getPosition() > 1500? (long long) stream->getPosition() - 1500 : 0);
    } else if (m_seek_direction == 2) {
        if (stream)
            stream->seekTo((long long) stream->getPosition() + 1500);
    }
    double t;
    if(total_len_ms > 0)
        t = ((double) current_pos_ms / (double) total_len_ms) * 220;
    else   
        t = 0.0f;
    // Draw progress bar fill on graph surface
    for(double x = 0; x < t; x++) {
        if (x > 146) {
            graph->vline(x + 400, 373, 382, (uint8_t) ((x - 146) * 3.5), 0, 255, 255);
        } else if (x < 73) {
            graph->vline(x + 400, 373, 382, 128, 255, (uint8_t) (x * 3.5), 255);
        } else {
            graph->vline(x + 400, 373, 382, (uint8_t) (128-((x-73)*1.75)), (uint8_t) (255-((x-73)*3.5)), 255, 255);
        }
    };

    // If dragging, use the drag position; otherwise, use the actual stream position
    unsigned long display_position_ms = m_is_dragging ? m_drag_position_ms : (stream ? stream->getPosition() : 0);


    // --- Final Scene Composition ---
    // 1. Clear the main screen
    screen->FillRect(screen->MapRGB(0, 0, 0));
    // 2. Blit the entire dynamic buffer (graph) to the screen
    screen->Blit(*graph, Rect(0, 0, graph->width(), graph->height()));
    // 3. Blit the entire UI widget tree. This will render all the labels.
    m_ui_root->BlitTo(*screen);

    // finally, update the screen :)
    screen->Flip();
    
    Player::guiRunning = false;
    // and if end of stream...
    return stream ? stream->eof() : false;
}

bool Player::handleKeyPress(const SDL_keysym& keysym)
{
    switch (keysym.sym) {
        case SDLK_ESCAPE: // NOLINT(bugprone-branch-clone)
        case SDLK_q:
            return true; // Signal to exit

        case SDLK_n:
            return !nextTrack(); // Signal to exit if no next track

        case SDLK_p:
            prevTrack();
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
            m_seek_direction = 1;
            break;

        case SDLK_RIGHT:
            m_seek_direction = 2;
            break;

        case SDLK_SPACE:
            playPause();
            break;

        case SDLK_r:
            this->seekTo(0);
            break;

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
            updateInfo();
            break;
        }

        default:
            // No action for other keys
            break;
    }

    return false; // Do not exit
}

void Player::handleMouseButtonDown(const SDL_MouseButtonEvent& event)
{
    if (event.button == SDL_BUTTON_LEFT) {
        // Check if click is within progress bar area
        // Progress bar X: 400 to 620 (width 220)
        // Progress bar Y: 370 to 385
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

void Player::handleMouseButtonUp(const SDL_MouseButtonEvent& event)
{
    if (event.button == SDL_BUTTON_LEFT && m_is_dragging) {
        this->seekTo(m_drag_position_ms);
        m_is_dragging = false;
    }
}

void Player::handleKeyUp(const SDL_keysym& keysym)
{
    switch (keysym.sym) {
        case SDLK_LEFT:
        case SDLK_RIGHT:
            m_seek_direction = 0;
            break;
        default:
            break;
    }
}

bool Player::handleUserEvent(const SDL_UserEvent& event)
{
    switch(event.code) {
        case START_FIRST_TRACK:
        {
            if (playlist->entries() > 0) {
                requestTrackLoad(playlist->getTrack(0));
            }
            break;
        }
        case DO_NEXT_TRACK:
        {
            return !nextTrack(m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1);
        }
        case DO_PREV_TRACK:
        {
            prevTrack();
            break;
        }
        case TRACK_LOAD_SUCCESS:
        {
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            Stream* new_stream = result->stream;
            m_num_tracks_in_current_stream = result->num_chained_tracks;
            delete result; // Free the result struct

            m_loading_track = false; // Loading complete

            // If an audio device exists, we need to handle the transition carefully
            // to avoid race conditions with the audio decoder thread.
            if (audio) {
                // Check if we need to recreate the audio device because the format changed,
                // or if the new stream failed to load (new_stream is null).
                bool recreate_audio = (!new_stream || audio->getRate() != new_stream->getRate() || audio->getChannels() != new_stream->getChannels());
                if (recreate_audio) {
                    // Destroy the old audio object *before* we change the stream.
                    // This stops its thread, which might be using the old stream.
                    audio.reset();
                }
            }

            // Now it's safe to update the stream pointer, which destroys the old stream.
            {
                std::lock_guard<std::mutex> lock(*mutex);
                stream.reset(new_stream);
                ATdata.stream = stream.get();
            }

            if (stream) {
                // If audio was destroyed or never existed, create it now.
                if (!audio) {
                    audio = std::make_unique<Audio>(&ATdata);
                }

                updateInfo();
                play();
            } else {
                // If the new stream is null, audio would have been reset above.
                // If stream is null (e.g., MediaFile::open returned nullptr for some reason not caught by exception)
                state = PlayerState::Stopped;
                updateInfo(false, "Error loading track");
            }
            break;
        }
        case TRACK_LOAD_FAILURE:
        {
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            TagLib::String error_msg = result->error_message;
            delete result; // Free the result struct

            m_loading_track = false; // Loading complete

            std::cerr << "Failed to load track: " << error_msg << std::endl;
            // Handle error: stop playback, display error message
            stop(); // Stop current playback
            updateInfo(false, error_msg);
            break;
        }
        case TRACK_PRELOAD_SUCCESS:
        {
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            m_preloading_track = false;
            // If a "play now" request started while we were preloading, discard this result.
            if (m_loading_track) {
                delete result->stream;
            } else {
                m_next_stream.reset(result->stream);
                m_num_tracks_in_next_stream = result->num_chained_tracks;
            }
            delete result;
            break;
        }
        case TRACK_PRELOAD_FAILURE:
        {
            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.data1);
            m_preloading_track = false;
            std::cerr << "Failed to preload track: " << result->error_message << std::endl;
            delete result;
            break;
        }
        case RUN_GUI_ITERATION:
        {
            if (updateGUI()) {
                // Track ended. Check for a preloaded stream.
                if (m_next_stream) {
                    // Seamless swap logic
                    bool recreate_audio = (!audio || audio->getRate() != m_next_stream->getRate() || audio->getChannels() != m_next_stream->getChannels());
                    if (recreate_audio) audio.reset();

                    {
                        std::lock_guard<std::mutex> lock(*mutex);
                        // Advance playlist for the track(s) that just finished
                        for (size_t i = 0; i < (m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1); ++i) {
                            playlist->next();
                        }
                        stream = std::move(m_next_stream);
                        m_num_tracks_in_current_stream = m_num_tracks_in_next_stream;
                        m_num_tracks_in_next_stream = 0;
                        ATdata.stream = stream.get();
                    }

                    if (!audio) {
                        audio = std::make_unique<Audio>(&ATdata);
                    }
                    updateInfo();
                    play();
                } else {
                    // No preloaded track, use the old method.
                    return !nextTrack(m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1);
                }
            }
            break;
        }
    }
    return false; // Do not exit
}

/* Main player functionality */

void Player::Run(std::vector<std::string> args) {
    if((args.size() > 1) && args[1] == "--version") {
        about_console();
        return;
    }

    // initialize SDL video
    if ( SDL_Init( SDL_INIT_EVERYTHING ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);

    std::cout << "System::getStoragePath: " << System::getStoragePath().to8Bit(true) << std::endl;
    std::cout << "System::getUser: " << System::getUser().to8Bit(true) << std::endl;
    std::cout << "System::getHome: " << System::getHome().to8Bit(true) << std::endl;
#ifdef _WIN32
    std::cout << "System::getHwnd: " << std::hex << System::getHwnd() << std::endl;
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
    font = std::make_unique<Font>("./vera.ttf");
#else
    font = std::make_unique<Font>(PSYMP3_DATADIR "/vera.ttf");
#endif // _WIN32
    std::cout << "font->isValid(): " << font->isValid() << std::endl;
    graph = std::make_unique<Surface>(640, 400);
    precomputeSpectrumColors();

    // Initialize the UI widget tree
    m_ui_root = std::make_unique<Widget>();
    // Helper lambda to reduce boilerplate when creating and adding labels
    auto add_label = [&](const std::string& key, const Rect& pos) {
        auto label = std::make_unique<Label>(font.get(), pos);
        m_labels[key] = label.get(); // Store non-owning pointer in map
        m_ui_root->addChild(std::move(label)); // Transfer ownership to UI tree
    };

    add_label("artist",   Rect(1, 354, 0, 0));
    add_label("title",    Rect(1, 369, 0, 0));
    add_label("album",    Rect(1, 384, 0, 0));
    add_label("playlist", Rect(270, 354, 0, 0));
    add_label("position", Rect(400, 353, 0, 0));
    add_label("scale",    Rect(550, 0, 0, 0));
    add_label("decay",    Rect(550, 15, 0, 0));
    add_label("fft_mode", Rect(550, 30, 0, 0));

    // Create an empty playlist. It will be populated in the background.
    playlist = std::make_unique<Playlist>();

    fft = std::make_unique<FastFourier>();
    mutex = std::make_unique<std::mutex>();

    ATdata.fft = fft.get();
    ATdata.stream = stream.get();
    ATdata.mutex = mutex.get();

    // If command line arguments are provided, start populating the playlist
    // and load the first track in a background thread.
    if (args.size() > 1) {
        m_playlist_populator_thread = std::thread(&Player::playlistPopulatorLoop, this, args);
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
    SDL_TimerID timer = SDL_AddTimer(33, AppLoopTimer, nullptr);
    while (!done) {
        bool sdone = false;
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
                handleMouseButtonDown(event.button);
                break;
            }
            case SDL_MOUSEMOTION:
            {
                handleMouseMotion(event.motion);
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                handleMouseButtonUp(event.button);
                break;
            }
            case SDL_KEYUP:
            {
                switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    m_seek_direction = 0;
                    break;
                default:
                    break;
               }
               break;
            }
            case SDL_USEREVENT:
            {
                done = handleUserEvent(event.user);
                break;
            }
            } // end switch (event.type)
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
    printf("Exited cleanly\n");
    return;
}
