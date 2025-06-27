/*
 * player.cpp - class implementation for player class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

std::string convertInt(long number) {
   std::stringstream ss;
   ss << number;
   return ss.str();
}

std::string convertInt2(long number) {
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
    screen = nullptr;
    playlist = nullptr;
    graph = nullptr;
    font = nullptr;
    stream = nullptr;
    audio = nullptr;
    fft = nullptr;
    mutex = nullptr;
    system = nullptr;

    m_loader_active = true;
    m_loading_track = false;
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

    if (screen)
        delete screen;
    if (graph)
        delete graph;
    if (playlist)
        delete playlist;
    if (font)
        delete font;
    if (audio)
        delete audio;
    if (stream)
        delete stream;
    if (fft)
        delete fft;
    if (mutex)
        delete mutex;
    if (system)
        delete system;
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
    // Clear current track info immediately to show "loading" state
    info["artist"] = font->Render("Artist: Loading...");
    info["title"] = font->Render("Title: Loading...");
    info["album"] = font->Render("Album: Loading...");
    info["position"] = font->Render("Position: --:--.-- / --:--.--");
    screen->SetCaption(TagLib::String("PsyMP3 ") + PSYMP3_VERSION + " -:[ Loading... ]:-", TagLib::String("PsyMP3 ") + PSYMP3_VERSION);
    
    // Push request to queue and notify loader thread
    {
        std::lock_guard<std::mutex> lock(m_loader_queue_mutex);
        m_loader_queue.push(path);
    }
    m_loader_queue_cv.notify_one();
}

void Player::loaderThreadLoop() {
    System::setThisThreadName("track-loader");
    while (m_loader_active) {
        TagLib::String path_to_load;
        {
            std::unique_lock<std::mutex> lock(m_loader_queue_mutex);
            m_loader_queue_cv.wait(lock, [this]{
                return !m_loader_queue.empty() || !m_loader_active;
            });
            if (!m_loader_active) break; // Exit condition
            path_to_load = m_loader_queue.front();
            m_loader_queue.pop();
        } // Unlock mutex before blocking I/O

        Stream* new_stream = nullptr;
        TagLib::String error_msg;
        try {
            new_stream = MediaFile::open(path_to_load);
        } catch (const std::exception& e) {
            error_msg = e.what();
            new_stream = nullptr; // Ensure null if exception
        }

        // Synthesize event back to main thread
        TrackLoadResult* result = new TrackLoadResult(); // Allocated on heap, freed by main thread
        result->stream = new_stream;
        result->error_message = error_msg;
        
        synthesizeUserEvent(new_stream ? TRACK_LOAD_SUCCESS : TRACK_LOAD_FAILURE, result, nullptr);
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
                playlist = loaded_playlist.release(); // Transfer ownership
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
bool Player::nextTrack(void) {
    TagLib::String nextfile = playlist->next();
    if (nextfile == "") {
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
    state = STOPPED;
    if (stream) { 
        delete stream; 
        stream = nullptr;
    }
    return true;
}

bool Player::pause(void) {
    if (state != STOPPED) {
        audio->play(false);
        state = PAUSED;
        return true;
    } else {
        return false;
    }
}

bool Player::play(void) {
    audio->play(true);
    state = PLAYING;
    return true;
}

bool Player::playPause(void) {
    switch(state) {
        case STOPPED:
        case PAUSED:
        {
            play();
            break;
        }
        case PLAYING:
        {
            pause();
            break;
        }
        default:
            return false;
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

float Player::logarithmicScale(const int f, float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    if(f) 
        for (auto i=0;i<f;i++) 
            x = std::log10(1.0f + 9.0f * x);
    return x;
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

    for(uint16_t x=0; x < 320; x++) {
        // graph->rectangle(x * 2, (int16_t) 350 - (spectrum[x] * 350.0f * 4) , (x * 2) + 1 , 350, 0xFFFFFFFF);
        if (x > 213) {
            graph->rectangle(x * 2, (int16_t) 350 - (logarithmicScale(scalefactor, spectrum[x]) * 350.0f) , (x * 2) + 1, 350, (uint8_t) ((x - 214) * 2.4), 0, 255, 255);
        } else if (x < 106) {
            graph->rectangle(x * 2, (int16_t) 350 - (logarithmicScale(scalefactor, spectrum[x]) * 350.0f) , (x * 2) + 1, 350, 128, 255, (uint8_t) (x * 2.398), 255);
        } else {
            graph->rectangle(x * 2, (int16_t) 350 - (logarithmicScale(scalefactor, spectrum[x]) * 350.0f) , (x * 2) + 1, 350, (uint8_t) (128 - ((x - 106) * 1.1962615)), (uint8_t) (255 - ((x - 106) * 2.383177)), 255, 255);
        }
    };
}

void Player::updateInfo(void) {
} 

/* Main player functionality */

void Player::Run(std::vector<std::string> args) {
    if((args.size() > 1) && args[1] == "--version") {
        about_console();
        return;
    }
    unsigned char seek = 0;
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
    Libmpg123::init();
    // FastFourier::init();

    // Initialize UI and essential components first to show the window quickly.
    screen = new Display();
    system = new System();
#if defined(_WIN32)
    font = new Font("./vera.ttf");
#else
    font = new Font(PSYMP3_DATADIR "/vera.ttf");
#endif // _WIN32
    std::cout << "font->isValid(): " << font->isValid() << std::endl;
    graph = new Surface(640, 400);

    // Create an empty playlist. It will be populated in the background.
    playlist = new Playlist();

    fft = new FastFourier();
    mutex = new std::mutex();

    stream = nullptr; // No stream initially
    audio = nullptr;  // No audio initially

    ATdata.fft = fft;
    ATdata.stream = stream;
    ATdata.mutex = mutex;

    // If command line arguments are provided, start populating the playlist
    // and load the first track in a background thread.
    if (args.size() > 1) {
        m_playlist_populator_thread = std::thread(&Player::playlistPopulatorLoop, this, args);
        state = STOPPED; // Will transition to playing when track loads
    } else {
        // No files, start with stopped state and an empty screen.
        state = STOPPED;
        info["artist"] = font->Render("Artist: ");
        info["title"] = font->Render("Title: ");
        info["album"] = font->Render("Album: ");
        info["playlist"] = font->Render("Playlist: 0/0");
        info["position"] = font->Render("Position: --:--.-- / --:--.--");
        info["scale"] = font->Render(std::string("log scale = ") + std::to_string(scalefactor));
        info["decay"] = font->Render("decay = " + std::to_string(decayfactor));
        info["fft_mode"] = font->Render("FFT Mode: " + fft->getFFTModeName());
        screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ]:-", "PsyMP3 " PSYMP3_VERSION);
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
            // check for messages
            switch (event.type) {
                // exit if the window is closed
            case SDL_QUIT:
                done = true;
                break;

                // check for keypresses
            case SDL_KEYDOWN:
            {
                // exit if ESCAPE is pressed
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:
                    done = true;
                    break;
                case SDLK_n:
                {
                    done = !nextTrack();
                    break;
                }
                case SDLK_p:
                {
                    prevTrack();
                    break;
                }
                case SDLK_0:
                {
                    scalefactor = 0;
                    info["scale"] = font->Render("log scale = " + std::to_string(scalefactor));
                    break;
                }
                case SDLK_1:
                {
                    scalefactor = 1;
                    info["scale"] = font->Render("log scale = " + std::to_string(scalefactor));
                    break;
                }
                case SDLK_2:
                {
                    scalefactor = 2;
                    info["scale"] = font->Render("log scale = " + std::to_string(scalefactor));
                    break;
                }
                case SDLK_3:
                {
                    scalefactor = 3;
                    info["scale"] = font->Render("log scale = " + std::to_string(scalefactor));
                    break;
                }
                case SDLK_4:
                {
                    scalefactor = 4;
                    info["scale"] = font->Render("log scale = " + std::to_string(scalefactor));
                    break;
                }
                case SDLK_z:
                {
                    decayfactor = 0.5f;
                    info["decay"] = font->Render("decay = " + std::to_string(decayfactor));
                    break;
                }
                case SDLK_x:
                {
                    decayfactor = 1.0f;
                    info["decay"] = font->Render("decay = " + std::to_string(decayfactor));
                    break;
                }
                case SDLK_c:
                {
                    decayfactor = 2.0f;
                    info["decay"] = font->Render("decay = " + std::to_string(decayfactor));
                    break;
                }
                case SDLK_LEFT:
                {
                    seek = 1;
                    break;
                }
                case SDLK_RIGHT:
                {
                    seek = 2;
                    break;
                }
                case SDLK_SPACE:
                {
                    playPause();
                    break;
                }
                case SDLK_r:
                    this->seekTo(0);
                    break;
                case SDLK_f:
                {
                    // Cycle through FFT modes
                    FFTMode current_mode = fft->getFFTMode();
                    FFTMode next_mode;
                    switch (current_mode) {
                        case FFTMode::Original: next_mode = FFTMode::Optimized; break;
                        case FFTMode::Optimized: next_mode = FFTMode::NeomatIn; break;
                        case FFTMode::NeomatIn: next_mode = FFTMode::NeomatOut; break;
                        case FFTMode::NeomatOut: next_mode = FFTMode::Original; break;
                        default: next_mode = FFTMode::Original; break; // Should not happen
                    }
                    fft->setFFTMode(next_mode);
                    info["fft_mode"] = font->Render("FFT Mode: " + fft->getFFTModeName());
                    break;
                }
                default:
                    break;
            }
            break;
        }
            case SDL_MOUSEBUTTONDOWN:
            {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Check if click is within progress bar area
                    // Progress bar X: 400 to 620 (width 220)
                    // Progress bar Y: 370 to 385
                    if (event.button.x >= 400 && event.button.x <= 620 &&
                        event.button.y >= 370 && event.button.y <= 385) {
                        if (stream) {
                            m_is_dragging = true;
                            // Don't seek here, just update the visual position and start the drag.
                            int relative_x = event.button.x - 400;
                            double progress_ratio = static_cast<double>(relative_x) / 220.0;
                            m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress_ratio);
                            synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr);
                            m_is_dragging = true;
                            m_drag_start_x = event.button.x;
                            m_drag_start_time = SDL_GetTicks();
                        }
                    }
                }
                break;
            }
            case SDL_MOUSEMOTION:
            {
                if (m_is_dragging && stream && stream->getLength() > 0 &&
                    event.motion.x >= 400 && event.motion.x <= 620 &&
                    event.motion.y >= 370 && event.motion.y <= 385) {
                    int relative_x = event.motion.x - 400; // 0 to 220
                    double progress_ratio = static_cast<double>(relative_x) / 220.0;
                    m_drag_position_ms = static_cast<unsigned long>(stream->getLength() * progress_ratio);
                    synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr); // Update display during drag
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                if (event.button.button == SDL_BUTTON_LEFT && m_is_dragging) {
                    this->seekTo(m_drag_position_ms);
                    m_is_dragging = false;
                }
                break;
            }
            case SDL_KEYUP:
            {
                switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    seek = 0;
                    break;
                default:
                    break;
               }
            }
            case SDL_USEREVENT:
                switch(event.user.code) {
                    case START_FIRST_TRACK:
                    {
                        if (playlist->entries() > 0) {
                            requestTrackLoad(playlist->getTrack(0));
                        }
                        break;
                    }
                    case DO_NEXT_TRACK:
                    {
                        done = !nextTrack();
                        break;
                    }
                    case DO_PREV_TRACK:
                    {
                        prevTrack();
                        break;
                    }
                    case TRACK_LOAD_SUCCESS:
                        {
                            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.user.data1);
                            Stream* new_stream = result->stream;
                            delete result; // Free the result struct

                            m_loading_track = false; // Loading complete

                            // If an audio device exists, lock it before we swap streams.
                            if (audio) {
                                audio->lock();
                            }

                            if (stream) {
                                delete stream; // Delete old stream
                            }
                            stream = new_stream;
                            ATdata.stream = stream;

                            if (stream) {
                                if (!audio) {
                                    // First track loaded, create the audio device.
                                    audio = new Audio(&ATdata);
                                } else {
                                    // Audio device exists, check if we need to re-open it for the new format.
                                    if ((audio->getRate() != stream->getRate()) || (audio->getChannels() != stream->getChannels())) {
                                        pause(); // Pause to avoid race condition during audio device re-open
                                        audio->unlock(); // Unlock before deleting/recreating Audio
                                        delete audio;
                                        audio = new Audio(&ATdata);
                                    } else {
                                        // Format is the same, just unlock.
                                        audio->unlock();
                                    }
                                }

                                // Render text surfaces and store them in the map.
                                info["artist"] = font->Render("Artist: " + stream->getArtist());
                                info["title"] = font->Render("Title: " + stream->getTitle());
                                info["album"] = font->Render("Album: " + stream->getAlbum());
                                info["playlist"] = font->Render("Playlist: " + convertInt(playlist->getPosition() + 1) + "/" + convertInt(playlist->entries()));
                                info["scale"] = font->Render(std::string("log scale = ") + std::to_string(scalefactor));
                                info["decay"] = font->Render("decay = " + std::to_string(decayfactor));
                                info["fft_mode"] = font->Render("FFT Mode: " + fft->getFFTModeName());
                                play();
                            } else {
                                // If stream is null (e.g., MediaFile::open returned nullptr for some reason not caught by exception)
                                state = STOPPED;
                                screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ Error loading track ]:-", "PsyMP3 " PSYMP3_VERSION);
                                info["artist"] = font->Render("Artist: N/A");
                                info["title"] = font->Render("Title: Error loading track");
                                info["album"] = font->Render("Album: N/A");
                                info["playlist"] = font->Render("Playlist: N/A");
                                info["position"] = font->Render("Position: --:--.-- / --:--.--");
                            }
                        }
                        break;
                    case TRACK_LOAD_FAILURE:
                        {
                            TrackLoadResult* result = static_cast<TrackLoadResult*>(event.user.data1);
                            TagLib::String error_msg = result->error_message;
                            delete result; // Free the result struct

                            m_loading_track = false; // Loading complete

                            std::cerr << "Failed to load track: " << error_msg << std::endl;
                            // Handle error: stop playback, display error message
                            stop(); // Stop current playback
                            screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ Error: " + error_msg.to8Bit(true) + " ]:-", "PsyMP3 " PSYMP3_VERSION);
                            info["artist"] = font->Render("Artist: N/A");
                            info["title"] = font->Render("Title: Error: " + error_msg);
                            info["album"] = font->Render("Album: N/A");
                            info["playlist"] = font->Render("Playlist: N/A");
                            info["position"] = font->Render("Position: --:--.-- / --:--.--");
                        }
                        break;
                    case RUN_GUI_ITERATION:
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
                            this->renderSpectrum(graph);
                        }
                        // --- End of critical section ---

                        // Now use the copied data for rendering, outside the lock.
                        if(stream) {
                            info["position"] = font->Render("Position: " + convertInt(current_pos_ms / 60000)
                                                    + ":" + convertInt2((current_pos_ms / 1000) % 60)
                                                    + "." + convertInt2((current_pos_ms / 10) % 100)
                                                    + "/" + convertInt(total_len_ms / 60000)
                                                    + ":" + convertInt2((total_len_ms / 1000) % 60)
                                                    + "." + convertInt2((total_len_ms / 10) % 100));
                            screen->SetCaption("PsyMP3 " PSYMP3_VERSION +
                                           (std::string) " -:[ " + artist.to8Bit(true) + " ]:- -- -:[ " +
                                           title.to8Bit(true) + " ]:-", "PsyMP3 " PSYMP3_VERSION);
                        } else {
                            info["position"] = font->Render("Position: -:--.-- / -:--.--");
                            screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ]:-", "PsyMP3 " PSYMP3_VERSION);
                        }
                        
                        // draw progress bar on the graph surface
                        graph->vline(399, 370, 385, 0xFFFFFFFF);
                        graph->vline(621, 370, 385, 0xFFFFFFFF);
                        graph->hline(399, 402, 370, 0xFFFFFFFF);
                        graph->hline(399, 402, 385, 0xFFFFFFFF);
                        graph->hline(618, 621, 370, 0xFFFFFFFF);
                        graph->hline(618, 621, 385, 0xFFFFFFFF);
                        if (seek == 1) {
                            if (stream)
                                stream->seekTo((long long) stream->getPosition() > 1500? (long long) stream->getPosition() - 1500 : 0);
                        } else if (seek == 2) {
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
                        // 3. Draw all static text labels on top
                        if(info["artist"].isValid()) screen->Blit(info["artist"], Rect(1, 354, info["artist"].width(), info["artist"].height()));
                        if(info["playlist"].isValid()) screen->Blit(info["playlist"], Rect(270, 354, info["playlist"].width(), info["playlist"].height()));
                        if(info["title"].isValid()) screen->Blit(info["title"], Rect(1, 369, info["title"].width(), info["title"].height()));
                        if(info["album"].isValid()) screen->Blit(info["album"], Rect(1, 384, info["album"].width(), info["album"].height()));
                        if(info["position"].isValid()) screen->Blit(info["position"], Rect(400, 353, info["position"].width(), info["position"].height()));
                        if(info["scale"].isValid()) screen->Blit(info["scale"], Rect(550, 0, info["scale"].width(), info["scale"].height())); 
                        if(info["fft_mode"].isValid()) screen->Blit(info["fft_mode"], Rect(550, 30, info["fft_mode"].width(), info["fft_mode"].height())); // This line is duplicated below, will be overwritten
                        if(info["decay"].isValid()) screen->Blit(info["decay"], Rect(550, 15, info["decay"].width(), info["decay"].height()));

                        // finally, update the screen :)
                        screen->Flip();
                        // and if end of stream...
                        if (stream)
                            sdone = stream->eof();
                        else
                            sdone = false; 
                        Player::guiRunning = false;
                        break;
                    }
                }
                break;
            } // end switch
            if (sdone) {
                done = !nextTrack();
                sdone = false;
            }
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
