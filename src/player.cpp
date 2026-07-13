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
#include <utility>
#include <random>
#include "core/SpectrumConfig.h"


std::atomic<bool> Player::guiRunning{false};

namespace {

bool widgetBelongsToWindow(const Widget* candidate, const WindowFrameWidget* window)
{
    const Widget* current = candidate;
    while (current) {
        if (current == window) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}

#if defined(_WIN32)
// Load the UI font, preferring the copy embedded in the exe (RCDATA
// IDR_VERA_TTF = 2000, see res/psymp3.rc) so the binary is self-contained;
// fall back to ./vera.ttf then ./res/vera.ttf relative to the CWD. Always
// returns a non-null Font (an invalid one as last resort) so callers keep the
// "font is never null" invariant. The file Font ctor throws, so those attempts
// are guarded.
std::unique_ptr<Font> loadUiFont(int ptsize)
{
    HMODULE mod = GetModuleHandleW(nullptr);
    if (HRSRC res = FindResourceW(mod, MAKEINTRESOURCEW(2000),
                                  reinterpret_cast<LPCWSTR>(RT_RCDATA))) {
        if (HGLOBAL h = LoadResource(mod, res)) {
            const void* data = LockResource(h);
            DWORD size = SizeofResource(mod, res);
            if (data && size) {
                auto f = std::make_unique<Font>(static_cast<const uint8_t*>(data),
                                                static_cast<size_t>(size), ptsize);
                if (f->isValid()) return f;
            }
        }
    }
    for (const char* path : {"./vera.ttf", "./res/vera.ttf"}) {
        try {
            auto f = std::make_unique<Font>(TagLib::String(path), ptsize);
            if (f->isValid()) return f;
        } catch (const std::exception&) { /* try next source */ }
    }
    return std::make_unique<Font>(nullptr, 0, ptsize); // invalid, but non-null
}

// Win32 menu command IDs. Ranges are contiguous so CheckMenuRadioItem can mark
// the active entry per submenu.
namespace {
enum Win32MenuId : unsigned int {
    IDM_FILE_INSERT     = 0xE100,
    IDM_FILE_TEMPLOAD   = 0xE101,
    IDM_FILE_EXIT       = 0xE102,
    IDM_FFT_FIRST       = 0xE110, // 4 modes: Original, Optimized, NeomatIn, NeomatOut
    IDM_DELAY_FIRST     = 0xE120, // 3 delays: 0.5, 1.0, 2.0 (Z, X, C)
    IDM_INTENSITY_FIRST = 0xE130, // 4 levels: scalefactor 1..4
};
} // namespace
#endif // _WIN32

bool canReuseAudioForStream(const Audio* audio, Stream* stream)
{
    if (!audio || !stream) {
        return false;
    }

    return static_cast<unsigned int>(audio->getRate()) == stream->getRate() &&
           static_cast<unsigned int>(audio->getChannels()) == stream->getChannels();
}

TagLib::String toUtf8TagString(const std::string& text)
{
    return TagLib::String(text, TagLib::String::UTF8);
}

constexpr unsigned long kSeekResetToStartThresholdMs = 250;
constexpr unsigned long kSeekNaturalEndToleranceMs = 2000;

void logSeekErrorEvent(Stream* stream,
                       const char* event_name,
                       unsigned long requested_pos_ms,
                       unsigned long actual_pos_ms,
                       unsigned long previous_pos_ms,
                       unsigned long total_len_ms,
                       bool stream_eof,
                       bool audio_finished)
{
    const std::string path = stream ? stream->getFilePath().to8Bit(true) : "<unknown>";
    Debug::log("seek_error",
               event_name,
               ": path=",
               path,
               ", requested_ms=",
               requested_pos_ms,
               ", actual_ms=",
               actual_pos_ms,
               ", previous_ms=",
               previous_pos_ms,
               ", length_ms=",
               total_len_ms,
               ", stream_eof=",
               stream_eof,
               ", audio_finished=",
               audio_finished);
}

bool seekUnexpectedlyJumpedToStart(unsigned long requested_pos_ms, unsigned long actual_pos_ms)
{
    return requested_pos_ms > kSeekResetToStartThresholdMs &&
           actual_pos_ms <= kSeekResetToStartThresholdMs;
}

bool seekWouldNaturallyEndTrack(unsigned long requested_pos_ms, unsigned long total_len_ms)
{
    return total_len_ms > 0 &&
           requested_pos_ms + kSeekNaturalEndToleranceMs >= total_len_ms;
}

size_t getPrimeSampleCount(Stream* stream)
{
    if (!stream) {
        return 0;
    }

    const size_t samples_per_half_second =
        (static_cast<size_t>(stream->getRate()) *
         static_cast<size_t>(stream->getChannels())) / 2;
    return std::max<size_t>(4096, samples_per_half_second);
}

std::pair<std::vector<int16_t>, bool> primeLoadedStream(Stream* stream)
{
    if (!stream) {
        return {{}, false};
    }

    const size_t prime_samples = getPrimeSampleCount(stream);
    std::vector<int16_t> primed_samples(prime_samples);
    const size_t bytes_read = stream->getData(prime_samples * sizeof(int16_t), primed_samples.data());
    primed_samples.resize(bytes_read / sizeof(int16_t));
    return {std::move(primed_samples), stream->eof()};
}

std::unique_ptr<Widget> createTestWindowHClient(Font* font)
{
    auto client = std::make_unique<LayoutWidget>(170, 142, false);
    client->setBackgroundColor(255, 255, 255);

    auto status_label = std::make_unique<Label>(
        font, Rect(12, 10, 120, 14), TagLib::String("Checked: No"),
        SDL_Color{0, 0, 0, 255}, SDL_Color{255, 255, 255, 255});
    auto* status_label_ptr = status_label.get();
    client->addChild(std::move(status_label));

    auto scroll_label = std::make_unique<Label>(
        font, Rect(12, 25, 120, 14), TagLib::String("Scroll: 50%"),
        SDL_Color{0, 0, 0, 255}, SDL_Color{255, 255, 255, 255});
    auto* scroll_label_ptr = scroll_label.get();
    client->addChild(std::move(scroll_label));

    auto input_label = std::make_unique<Label>(
        font, Rect(12, 40, 120, 14), TagLib::String("Input:"),
        SDL_Color{0, 0, 0, 255}, SDL_Color{255, 255, 255, 255});
    client->addChild(std::move(input_label));

    auto input_status_label = std::make_unique<Label>(
        font, Rect(12, 124, 120, 14), TagLib::String("Text: PsyMP3"),
        SDL_Color{0, 0, 0, 255}, SDL_Color{255, 255, 255, 255});
    auto* input_status_label_ptr = input_status_label.get();
    client->addChild(std::move(input_status_label));

    auto text_input = std::make_unique<TextInputWidget>(118, 20, font, TagLib::String("PsyMP3"));
    text_input->setPos(Rect(12, 56, 118, 20));
    text_input->setPlaceholder(TagLib::String("Type here"));
    text_input->setOnChange([input_status_label_ptr](const TagLib::String& text) {
        std::string rendered = text.to8Bit(true);
        if (rendered.empty()) {
            rendered = "<empty>";
        }
        input_status_label_ptr->setText(TagLib::String("Text: " + rendered));
    });
    client->addChild(std::move(text_input));

    auto checkbox = std::make_unique<CheckboxWidget>(110, 18, font, TagLib::String("Enable H"), false);
    auto* checkbox_ptr = checkbox.get();
    checkbox->setPos(Rect(12, 82, 110, 18));
    checkbox->setOnToggle([status_label_ptr](bool checked) {
        status_label_ptr->setText(TagLib::String(checked ? "Checked: Yes" : "Checked: No"));
    });
    client->addChild(std::move(checkbox));

    auto button = std::make_unique<ButtonWidget>(72, 22);
    button->setText(TagLib::String("Apply"), font);
    button->setPos(Rect(12, 104, 72, 22));
    button->setOnClick([checkbox_ptr]() {
        checkbox_ptr->setChecked(!checkbox_ptr->isChecked());
    });
    client->addChild(std::move(button));

    auto scrollbar = std::make_unique<ScrollbarWidget>(16, 124, ScrollbarOrientation::Vertical);
    scrollbar->setPos(Rect(142, 10, 16, 124));
    scrollbar->setOnChange([scroll_label_ptr](double value) {
        int percent = static_cast<int>(std::round(value * 100.0));
        scroll_label_ptr->setText(TagLib::String("Scroll: " + std::to_string(percent) + "%"));
    });
    client->addChild(std::move(scrollbar));

    return client;
}

}

/**
 * @brief Converts an integer to a zero-padded decimal string.
 * @param number Number to format.
 * @param width  Minimum field width; 0 means no padding.
 * @return Formatted string.
 */
static std::string convertInt(long number, int width = 0) {
    std::stringstream ss;
    if (width > 0) {
        ss << std::setw(width) << std::setfill('0');
    }
    ss << number;
    return ss.str();
}


/**
 * @brief Constructs the Player object.
 *
 * Starts the background track-loader thread, initialises the Last.fm
 * scrobbler, and on D-Bus builds, creates the MPRIS manager. The
 * application window and audio subsystem are not created until
 * `Initialize()` is called.
 */
Player::Player() : m_rng(std::random_device{}()) {
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
    loadSettings(); // volume + EQ state from psymp3.conf (applied to each Audio on creation)

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

/**
 * @brief Destroys the Player object.
 *
 * Resets the `Audio` subsystem (joining the audio thread), shuts down MPRIS,
 * joins the loader and playlist-populator threads, and clears the Windows
 * now-playing status.
 */
Player::~Player() {
    saveSettings(); // persist volume + EQ state before teardown

    // Stop audio first to join decoder threads before deleting other members
    audio.reset();

    // Notify all windows that the application is shutting down
    if (ApplicationWidget::isInitialized()) {
        ApplicationWidget::getInstance().notifyShutdown();
    }
    
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
    SDL_Event event{};
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
bool Player::synthesizeUserEvent(int code, void *data1, void* data2) {
    SDL_Event event{};

    event.type = SDL_USEREVENT;
    event.user.code = code;
    event.user.data1 = data1;
    event.user.data2 = data2;

    // SDL_PushEvent returns 1 when queued, 0 if filtered, negative on error.
    return SDL_PushEvent(&event) == 1;
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
    m_next_stream_primed_samples.clear();
    m_next_stream_primed_eof = false;

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
        std::vector<int16_t> primed_samples;
        bool primed_eof = false;

        try {
            // Own the stream via unique_ptr until priming succeeds, so a throw
            // from primeLoadedStream() frees it (and its open file handle)
            // instead of leaking the sole raw pointer.
            std::unique_ptr<Stream> stream_holder;
            switch (request.type) {
                case LoadRequestType::PlayNow:
                case LoadRequestType::Preload:
                    stream_holder = MediaFile::open(request.path);
                    num_chained = 1;
                    break;
                case LoadRequestType::PreloadChained:
                    stream_holder = std::make_unique<ChainedStream>(request.paths);
                    num_chained = request.paths.size();
                    break;
            }

            if (stream_holder) {
                auto primed = primeLoadedStream(stream_holder.get());
                primed_samples = std::move(primed.first);
                primed_eof = primed.second;
            }
            // Priming succeeded; hand ownership to the raw pointer the result
            // carries to the main thread.
            new_stream = stream_holder.release();
        } catch (const std::exception& e) {
            error_msg = e.what();
            new_stream = nullptr; // stream_holder freed by RAII during unwind
        }

        // Synthesize event back to main thread
        auto* result = new TrackLoadResult(); // Allocated on heap, freed by main thread
        result->request_type = request.type;
        result->stream = new_stream;
        result->error_message = error_msg;
        result->num_chained_tracks = num_chained;
        result->primed_samples = std::move(primed_samples);
        result->primed_eof = primed_eof;

        int success_event = (request.type == LoadRequestType::PlayNow) ? TRACK_LOAD_SUCCESS : TRACK_PRELOAD_SUCCESS;
        int failure_event = (request.type == LoadRequestType::PlayNow) ? TRACK_LOAD_FAILURE : TRACK_PRELOAD_FAILURE;

        if (!synthesizeUserEvent(new_stream ? success_event : failure_event, result, nullptr)) {
            // The event was dropped (e.g. queue full). The main thread will
            // never see it, so free what we own here and release the loading
            // latch — otherwise the stream/result leak and requestTrackLoad
            // would reject every future request for the rest of the session.
            delete result->stream;
            delete result;
            if (request.type == LoadRequestType::PlayNow) {
                m_loading_track = false;
            } else {
                m_preloading_track = false;
            }
        }
    }
}

/**
 * @brief The main loop for the background playlist populator thread.
 * This thread is responsible for parsing command-line arguments and adding them
 * to the playlist. This is done in the background to allow the main window to
 * appear immediately on startup, without waiting for file system access.
 * @param args The vector of command-line arguments passed to the application.
 */
void Player::playlistPopulatorLoop(const std::vector<std::string>& args) {
    System::setThisThreadName("playlist-populator");

    if (args.empty()) return; // Nothing to do

    bool started_first_track = false;

    for (const std::string& arg : args) {
        const TagLib::String source(arg, TagLib::String::UTF8);
        std::vector<Playlist::Entry> resolved_entries = Playlist::resolveInlineSources({source});

        if (resolved_entries.empty()) {
            Debug::log("playlist", "Player::playlistPopulatorLoop(): No playable entries resolved from ", source.to8Bit(true));
            continue;
        }

        for (const auto& entry : resolved_entries) {
            try {
                if (playlist->addEntry(entry) && !started_first_track) {
                    synthesizeUserEvent(START_FIRST_TRACK, nullptr, nullptr);
                    started_first_track = true;
                }
            } catch (const std::exception& e) {
                Debug::log("playlist", "Player::playlistPopulatorLoop(): Failed to add resolved entry ", entry.path.to8Bit(true), ": ", e.what());
            }
        }
    }
}

void Player::handleTrackSeamlessSwapEvent() {
    // This event is triggered when a track ends and a preloaded track is ready.
    // A queued event (e.g. an 'N' keypress -> requestTrackLoad) can reset
    // m_next_stream after this swap was posted, or a duplicate swap event can
    // arrive after the std::move below already consumed it. If it's gone, a
    // load is already in flight; do nothing rather than destroying the live
    // Audio and throwing in the Audio constructor on a null stream (which would
    // also desync the playlist via handleUnplayableTrack).
    if (!m_next_stream) {
        Debug::log("audio", "Player::handleTrackSeamlessSwapEvent(): m_next_stream is null, skipping swap");
        return;
    }

    const bool recreate_audio = !canReuseAudioForStream(audio.get(), m_next_stream.get());

    try {
        if (recreate_audio) {
            // Different audio format, need to recreate Audio object
            Debug::log("audio", "Audio format changed, recreating Audio object for seamless transition.");
            audio.reset();
            auto owned_stream = std::move(m_next_stream);
            audio = std::make_unique<Audio>(std::move(owned_stream),
                                            fft.get(),
                                            mutex.get(),
                                            std::move(m_next_stream_primed_samples),
                                            m_next_stream_primed_eof);
            audio->setVolume(m_volume);
            applyEqStateToAudio();
        } else {
            // Same audio format, can seamlessly switch streams
            Debug::log("audio", "Performing seamless stream transition.");
            auto owned_stream = std::move(m_next_stream);
            audio->setStream(std::move(owned_stream),
                             std::move(m_next_stream_primed_samples),
                             m_next_stream_primed_eof);
        }
    } catch (const std::exception& e) {
        const std::string error_message = std::string("Seamless audio transition failed: ") + e.what();
        Debug::log("audio", "Player::handleTrackSeamlessSwapEvent(): ", error_message);
        m_next_stream.reset();
        m_next_stream_primed_samples.clear();
        m_next_stream_primed_eof = false;
        showNotification(error_message, NotificationType::Error);
        if (!handleUnplayableTrack()) {
            stop();
            updateInfo(false, toUtf8TagString(error_message));
        }
        return;
    }

    m_next_stream_primed_samples.clear();
    m_next_stream_primed_eof = false;

    // Advance the playlist for the track(s) that just finished
    for (size_t i = 0; i < (m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1); ++i) {
        playlist->next();
    }
    // The swapped-in stream may itself be a ChainedStream representing N
    // playlist entries; carry its track count forward so its eventual end
    // advances the playlist by N, not 1. (Previously reset to 0, replaying the
    // chained tracks.)
    m_num_tracks_in_current_stream = m_num_tracks_in_next_stream;
    m_num_tracks_in_next_stream = 0;

    // Update stream pointer and start scrobbling for new track
    stream = audio->getCurrentStream();
    startTrackScrobbling();

    // Notify now-playing listeners of the new track. The seamless-swap path
    // (a track ending into a preloaded one) must do this just like the manual
    // load path in handleTrackLoadSuccessEvent(); otherwise MPRIS clients never
    // see the metadata change on natural track end.
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus::Playing);
        if (stream) {
            m_mpris_manager->updateMetadata(
                stream->getArtist().to8Bit(true),
                stream->getTitle().to8Bit(true),
                stream->getAlbum().to8Bit(true),
                static_cast<uint64_t>(stream->getLength()) * 1000
            );
        }
    }
#endif
#ifdef _WIN32
    if (system && stream) system->announceNowPlaying(stream->getArtist(), stream->getTitle(), stream->getAlbum());
#endif

    // Update GUI and lyrics for the new stream
    updateInfo(false, "");
    if (m_lyrics_widget) {
        if (stream) {
            m_lyrics_widget->setLyrics(stream->getLyrics());
        } else {
            m_lyrics_widget->clearLyrics();
        }
    }
}

void Player::handleDoSavePlaylistEvent() {
    if (playlist) {
        System::createStoragePath(); // Ensure the directory exists before writing.
        TagLib::String save_path = System::getStoragePath() + "/playlist.m3u";
        playlist->savePlaylist(save_path);
        showToast("Current playlist saved!");
    }
}

void Player::handleShowNotificationEvent(std::pair<std::string, NotificationType>* data) {
    if (data) {
        std::string msg = data->first;
        NotificationType type = data->second;

        bool show = true;
        Uint32 duration = 2000;
        std::string prefix = "";

        switch (type) {
            case NotificationType::Info:
                break;
            case NotificationType::Warning:
                prefix = "Warning: ";
                duration = 3000;
                break;
            case NotificationType::Error:
                prefix = "Error: ";
                duration = 4000;
                break;
            case NotificationType::MPRISError:
                if (!m_show_mpris_errors) show = false;
                prefix = "MPRIS Error:\n";
                duration = 4000;
                break;
        }

        if (show) {
            showToast(prefix + msg, duration);
        }
        delete data;
    }
}

void Player::handleDoSetLoopModeEvent(LoopMode mode) {
    m_loop_mode = mode;
    std::string toastMsg = "Loop: ";
    // Only consumed under HAVE_DBUS below; keep it set unconditionally (the enum
    // type is always available) but mark it maybe-unused for --disable-mpris.
    [[maybe_unused]] PsyMP3::MPRIS::LoopStatus mprisEnum = PsyMP3::MPRIS::LoopStatus::None;
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
}

/**
 * @brief Toggles whether MPRIS D-Bus errors are shown as on-screen toast notifications.
 *
 * Flips `m_show_mpris_errors` and shows a confirmation toast.
 */
void Player::toggleMPRISErrorNotifications() {
    m_show_mpris_errors = !m_show_mpris_errors;
    std::string state = m_show_mpris_errors ? "ON" : "OFF";
    showToast("MPRIS Errors: " + state);
}

/**
 * @brief Queues a notification message to be displayed on the main thread.
 *
 * The message is heap-allocated and handed to the SDL event queue as a
 * `SHOW_NOTIFICATION` user event.  The main thread frees the allocation.
 *
 * @param message Message text.
 * @param type    Severity / category (`Info`, `Warning`, `Error`, `MPRISError`).
 */
void Player::showNotification(const std::string& message, NotificationType type) {
    auto* data = new std::pair<std::string, NotificationType>(message, type);
    synthesizeUserEvent(SHOW_NOTIFICATION, data, nullptr);
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

    // End-of-playlist handling applies to both shuffle and sequential order:
    // unless we are looping the whole playlist, running past the end stops (or
    // quits when unattended) instead of wrapping.
    if (m_loop_mode != LoopMode::All && playlist->advanceWouldWrap(advance_count)) {
        if (m_unattended_quit) {
            synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
            return;
        }
        stop();
        updateInfo();
        return;
    }

    if (playlist->isShuffle()) {
        TagLib::String next_path;
        for (size_t i = 0; i < advance_count; ++i) {
            next_path = playlist->next();
        }
        m_skip_attempts = 0;
        requestTrackLoad(next_path);
        return;
    }

    long new_pos = playlist->getPosition();
    for (size_t i = 0; i < advance_count; ++i) {
        new_pos++;
    }

    if (new_pos >= playlist->entries()) { // LoopMode::All: wrap to the start
        new_pos = 0;
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
    if (!playlist || playlist->entries() == 0) return;

    if (playlist->isShuffle()) {
        TagLib::String prev_path = playlist->prev();
        m_skip_attempts = 0;
        requestTrackLoad(prev_path);
        return;
    }

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

#ifdef HAVE_FILEDIALOG
/**
 * @brief Ctrl+O: open a multi-select native chooser and REPLACE the running
 *        playlist with the chosen track(s), then start playing the first one.
 *        Cancelling the dialog leaves the current playlist untouched.
 */
void Player::openTracksReplacingPlaylist()
{
    if (!playlist) {
        return;
    }
    std::vector<std::string> paths = PsyMP3::Core::FileDialog::openFiles(
        true, "Open track(s)", MediaFile::getSupportedExtensions());
    if (paths.empty()) {
        return; // cancelled: keep the existing playlist and playback
    }

    std::vector<Playlist::Entry> entries;
    entries.reserve(paths.size());
    for (const std::string& p : paths) {
        entries.push_back(Playlist::Entry{
            TagLib::String(p, TagLib::String::UTF8), TagLib::String(), TagLib::String(), 0});
    }

    playlist->clear();
    playlist->insertEntries(0, entries);
    playlist->setPosition(0);
    m_skip_attempts = 0;
    requestTrackLoad(playlist->getTrack(0));
}

/**
 * @brief "I" key: open a multi-select native chooser and insert the chosen
 *        track(s) at the current playlist index, then start playing the first
 *        inserted track. The track that was current shifts down by N and plays
 *        after the inserts.
 */
void Player::openInsertDialog()
{
    if (!playlist) {
        return;
    }
    std::vector<std::string> paths = PsyMP3::Core::FileDialog::openFiles(
        true, "Insert track(s)", MediaFile::getSupportedExtensions());
    if (paths.empty()) {
        return;
    }

    std::vector<Playlist::Entry> entries;
    entries.reserve(paths.size());
    for (const std::string& p : paths) {
        entries.push_back(Playlist::Entry{
            TagLib::String(p, TagLib::String::UTF8), TagLib::String(), TagLib::String(), 0});
    }

    long pos = playlist->getPosition();
    if (pos < 0) {
        pos = 0;
    }
    playlist->insertEntries(pos, entries);
    // Jump to the first inserted track now (it now lives at `pos`).
    playlist->setPosition(pos);
    m_skip_attempts = 0;
    requestTrackLoad(playlist->getTrack(pos));
}

/**
 * @brief "L" key: open a single-select native chooser and play the chosen file
 *        in place of the current track WITHOUT modifying the playlist. The
 *        playlist cursor is untouched, so the next track change (natural end or
 *        user navigation) resumes normal playlist flow and this override is
 *        forgotten.
 */
void Player::openTemporaryTrackDialog()
{
    std::vector<std::string> paths = PsyMP3::Core::FileDialog::openFiles(
        false, "Load temporary track", MediaFile::getSupportedExtensions());
    if (paths.empty()) {
        return;
    }
    requestTrackLoad(TagLib::String(paths.front(), TagLib::String::UTF8));
}
#endif // HAVE_FILEDIALOG

/**
 * @brief Empty the playlist and stop playback (nothing left to play).
 */
void Player::clearPlaylist()
{
    if (!playlist) {
        return;
    }
    playlist->clear();
    stop();
    updateInfo();
}

#ifdef _WIN32
void Player::installWin32Menu()
{
    SDL_Window* win = screen ? screen->getWindowHandle() : nullptr;
    if (!win) {
        return;
    }
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(win, &wmi) || wmi.subsystem != SDL_SYSWM_WINDOWS) {
        return;
    }
    HWND hwnd = wmi.info.win.window;

    HMENU bar = CreateMenu();

    HMENU file = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, IDM_FILE_INSERT,   "&Insert Track(s)\tI");
    AppendMenuA(file, MF_STRING, IDM_FILE_TEMPLOAD, "&Temp Load Track\tL");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, IDM_FILE_EXIT,     "E&xit");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), "&File");

    HMENU settings = CreatePopupMenu();

    HMENU fft_menu = CreatePopupMenu();
    AppendMenuA(fft_menu, MF_STRING, IDM_FFT_FIRST + 0, "mat-og");
    AppendMenuA(fft_menu, MF_STRING, IDM_FFT_FIRST + 1, "vibe-1");
    AppendMenuA(fft_menu, MF_STRING, IDM_FFT_FIRST + 2, "neomat-in");
    AppendMenuA(fft_menu, MF_STRING, IDM_FFT_FIRST + 3, "neomat-out");
    AppendMenuA(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(fft_menu), "FFT &Mode");

    HMENU delay_menu = CreatePopupMenu();
    // Lower decayfactor = slower fade = longer trail. Z=0.5 is the LONG delay,
    // C=2.0 is the SHORT one.
    AppendMenuA(delay_menu, MF_STRING, IDM_DELAY_FIRST + 0, "Long (Z)");
    AppendMenuA(delay_menu, MF_STRING, IDM_DELAY_FIRST + 1, "Normal (X)");
    AppendMenuA(delay_menu, MF_STRING, IDM_DELAY_FIRST + 2, "Short (C)");
    AppendMenuA(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(delay_menu), "&Delay");

    HMENU intensity_menu = CreatePopupMenu();
    AppendMenuA(intensity_menu, MF_STRING, IDM_INTENSITY_FIRST + 0, "1");
    AppendMenuA(intensity_menu, MF_STRING, IDM_INTENSITY_FIRST + 1, "2");
    AppendMenuA(intensity_menu, MF_STRING, IDM_INTENSITY_FIRST + 2, "3");
    AppendMenuA(intensity_menu, MF_STRING, IDM_INTENSITY_FIRST + 3, "4");
    AppendMenuA(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(intensity_menu), "&Intensity");

    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(settings), "&Settings");

    m_win32_fft_menu = fft_menu;
    m_win32_delay_menu = delay_menu;
    m_win32_intensity_menu = intensity_menu;

    SetMenu(hwnd, bar);
    // Route native window messages (WM_COMMAND) to us via SDL, and re-assert the
    // client size so the menu bar doesn't eat into the visualization area.
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    screen->reapplyWindowSize();
    syncWin32MenuState();
}

void Player::handleWin32MenuCommand(unsigned int id)
{
    switch (id) {
        case IDM_FILE_INSERT:
#ifdef HAVE_FILEDIALOG
            openInsertDialog();
#endif
            break;
        case IDM_FILE_TEMPLOAD:
#ifdef HAVE_FILEDIALOG
            openTemporaryTrackDialog();
#endif
            break;
        case IDM_FILE_EXIT:
            synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
            break;
        default:
            if (id >= IDM_FFT_FIRST && id < IDM_FFT_FIRST + 4) {
                static const FFTMode modes[] = { FFTMode::Original, FFTMode::Optimized,
                                                 FFTMode::NeomatIn, FFTMode::NeomatOut };
                fft->setFFTMode(modes[id - IDM_FFT_FIRST]);
                showToast("FFT Mode: " + fft->getFFTModeName());
                updateInfo();
            } else if (id >= IDM_DELAY_FIRST && id < IDM_DELAY_FIRST + 3) {
                static const float delays[] = { 0.5f, 1.0f, 2.0f };
                decayfactor = delays[id - IDM_DELAY_FIRST];
                updateInfo();
            } else if (id >= IDM_INTENSITY_FIRST && id < IDM_INTENSITY_FIRST + 4) {
                scalefactor = static_cast<int>(id - IDM_INTENSITY_FIRST) + 1;
                updateInfo();
            }
            break;
    }
    syncWin32MenuState();
}

void Player::syncWin32MenuState()
{
    if (m_win32_fft_menu && fft) {
        unsigned int idx = 0;
        switch (fft->getFFTMode()) {
            case FFTMode::Original:  idx = 0; break;
            case FFTMode::Optimized: idx = 1; break;
            case FFTMode::NeomatIn:  idx = 2; break;
            case FFTMode::NeomatOut: idx = 3; break;
            default:                 idx = 0; break;
        }
        CheckMenuRadioItem(static_cast<HMENU>(m_win32_fft_menu),
                           IDM_FFT_FIRST, IDM_FFT_FIRST + 3, IDM_FFT_FIRST + idx, MF_BYCOMMAND);
    }
    if (m_win32_delay_menu) {
        unsigned int idx = (decayfactor <= 0.75f) ? 0 : (decayfactor >= 1.5f ? 2 : 1);
        CheckMenuRadioItem(static_cast<HMENU>(m_win32_delay_menu),
                           IDM_DELAY_FIRST, IDM_DELAY_FIRST + 2, IDM_DELAY_FIRST + idx, MF_BYCOMMAND);
    }
    if (m_win32_intensity_menu) {
        int idx = scalefactor - 1;
        if (idx < 0) idx = 0;
        if (idx > 3) idx = 3;
        CheckMenuRadioItem(static_cast<HMENU>(m_win32_intensity_menu),
                           IDM_INTENSITY_FIRST, IDM_INTENSITY_FIRST + 3,
                           IDM_INTENSITY_FIRST + static_cast<unsigned int>(idx), MF_BYCOMMAND);
    }
}
#endif // _WIN32

/**
 * @brief Returns whether the "Previous" navigation action is possible.
 *
 * Returns `false` if the playlist is empty. Otherwise, depends on
 * `m_loop_mode` and current position.
 *
 * @return `true` if going to the previous track is possible.
 */
bool Player::canGoPrevious() const {
    if (!playlist || playlist->entries() == 0) return false;

    if (m_loop_mode == LoopMode::All) return true;

    if (playlist->getPosition() > 0) return true;

    // At position 0, LoopMode::None.
    // Check if we can seek to 0 (restart).
    // This requires a stream.
    if (mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        return stream != nullptr;
    }

    return false;
}

/**
 * @brief Returns whether the "Next" navigation action is possible.
 *
 * Returns `false` if the playlist is empty. In `LoopMode::All`, always true.
 * In `LoopMode::None`, false when on the last track.
 *
 * @return `true` if moving to the next track is possible.
 */
bool Player::canGoNext() const {
    if (!playlist || playlist->entries() == 0) return false;

    if (m_loop_mode == LoopMode::All) return true;

    // LoopMode::None
    if (playlist->getPosition() < playlist->entries() - 1) return true;

    // At last track. Next stops playback.
    return false;
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
    m_next_stream.reset();
    m_next_stream_primed_samples.clear();
    m_next_stream_primed_eof = false;

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
        // Guard like play(): state can be Playing with audio == nullptr during a
        // failed track-swap recreate, where pressing Space would otherwise
        // dereference a null audio.
        if (audio) audio->play(false);
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
    seekToInternal(pos, true);
}

void Player::seekToInternal(unsigned long pos, bool monitor_seek_errors)
{
    std::lock_guard<std::mutex> lock(*mutex);
    if (stream) {
        const unsigned long previous_pos_ms = stream->getPosition();
        const unsigned long total_len_ms = stream->getLength();

        if (audio) {
            audio->resetBuffer();
            audio->setSamplesPlayed((pos * audio->getRate()) / 1000);
        }
        stream->seekTo(pos);

        if (monitor_seek_errors) {
            const unsigned long actual_pos_ms = stream->getPosition();
            const bool stream_eof = stream->eof();
            const bool audio_finished = audio ? audio->isFinished() : false;

            if (seekUnexpectedlyJumpedToStart(pos, actual_pos_ms)) {
                logSeekErrorEvent(stream,
                                  "seek_reset_to_start",
                                  pos,
                                  actual_pos_ms,
                                  previous_pos_ms,
                                  total_len_ms,
                                  stream_eof,
                                  audio_finished);
            }

            if (!seekWouldNaturallyEndTrack(pos, total_len_ms) &&
                (stream_eof || audio_finished)) {
                logSeekErrorEvent(stream,
                                  "seek_premature_end",
                                  pos,
                                  actual_pos_ms,
                                  previous_pos_ms,
                                  total_len_ms,
                                  stream_eof,
                                  audio_finished);
            }
        }
        
#ifdef HAVE_DBUS
        // Notify MPRIS about the seek operation (convert ms to microseconds)
        if (m_mpris_manager) {
            m_mpris_manager->notifySeeked(static_cast<uint64_t>(pos) * 1000);
        }
#endif
    }
}

/**
 * @brief Checks if the current stream supports seeking.
 * This is a thread-safe operation.
 * @return true if seeking is supported, false otherwise.
 */
bool Player::canSeek() const
{
    std::lock_guard<std::mutex> lock(*mutex);
    return stream && stream->canSeek();
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

    m_spectrum_colors.resize(PsyMP3::Core::SpectrumConfig::NumBands);
    for (uint16_t x = 0; x < PsyMP3::Core::SpectrumConfig::NumBands; ++x) {
        auto color = PsyMP3::Core::SpectrumConfig::getBarColor(x);
        // Debug::log("player", "x: ", x, " r: ", (int)color.r, " g: ", (int)color.g, " b: ", (int)color.b);
        m_spectrum_colors[x] = graph->MapRGBA(color.r, color.g, color.b, 255);
    }
    Debug::log("player", "precomputeSpectrumColors finished.");
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

#ifdef _WIN32
    // Keep the native menu's radio checks in sync with keyboard-driven changes
    // (F, Z/X/C, 1-4 all route through updateInfo()).
    syncWin32MenuState();
#endif
}


/**
 * @brief Renders the overlay elements (pause indicator, seek indicators, windows) to the graph surface.
 * @param current_stream The current stream pointer.
 * @param current_pos_ms The current playback position in milliseconds.
 */
void Player::renderOverlay(Stream* current_stream, unsigned long current_pos_ms)
{
    // Update lyrics widget with current playback position
    if (m_lyrics_widget) {
        m_lyrics_widget->updatePosition(static_cast<unsigned int>(current_pos_ms));
    }

    // Render the main widget tree
    if (m_ui_root) {
        m_ui_root->BlitTo(*graph);
    }
    
    // Render the pause indicator if we're paused
    if (state == PlayerState::Paused && m_pause_indicator) {
        // Center the pause indicator in the FFT area (0,0 to 640,350)
        Rect pos = m_pause_indicator->getPos();
        pos.x((640 - pos.width()) / 2);
        pos.y((350 - pos.height()) / 2);
        m_pause_indicator->setPos(pos);
        m_pause_indicator->BlitTo(*graph);
    }
    
    // Render floating windows (test windows, equalizer)
    renderWindows();

    // Always-on-top windows (toasts and the menu bar) are drawn in the
    // m_ui_root pass (before renderWindows), so re-blit them here — otherwise a
    // toast or an open dropdown would be hidden behind a floating window such as
    // the equalizer.
    ApplicationWidget::getInstance().blitTopWindows(*graph);
}

/**
 * @brief Updates all dynamic state (stream position, lyrics, MPRIS, preloading) for one GUI frame.
 *
 * Called within `updateGUI()` while the player mutex is held. Copies position
 * and metadata from the stream, triggers preloading when near the end of the
 * track, feeds the FFT widget, and updates MPRIS position and Last.fm status.
 *
 * @param current_stream  Reference to the current stream pointer (updated in place).
 * @param current_pos_ms  Reference to the current position in ms (updated in place).
 * @param total_len_ms    Reference to the total length in ms (updated in place).
 * @param artist          Reference to the artist string (updated in place).
 * @param title           Reference to the title string (updated in place).
 */
void Player::updateState(Stream*& current_stream, unsigned long& current_pos_ms, unsigned long& total_len_ms, TagLib::String& artist, TagLib::String& title)
{
    // Don't clear the graph surface - widgets will draw their own backgrounds

    // Copy data from stream object while locked
    if (audio && audio->getCurrentStream()) {
        current_stream = audio->getCurrentStream(); // Assign here
        // During a keyboard seek, we use our manually-controlled position for
        // instant visual feedback. Otherwise, get the position from the stream.
        if (m_seek_direction != 0) {
            current_pos_ms = m_seek_position_ms;
        } else {
            if (audio && audio->getRate() > 0) {
                current_pos_ms = (audio->getSamplesPlayed() * 1000) / audio->getRate();
            } else {
                // getRate() is 0 when audio setup failed (e.g. no audio backend).
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
                std::optional<Playlist::TrackInfo> track_info = playlist->getTrackInfo(look_ahead_pos);
                if (track_info) {
                    long track_length = static_cast<long>(track_info->length_seconds) * 1000; // Convert seconds to milliseconds
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
                    // No track info available, stop scanning to be safe
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

        // Update spectrum data in the widget - it will render itself via the widget tree
        if (m_spectrum_widget && audio) {
            std::lock_guard<std::mutex> fft_lock(audio->getFFTMutex());
            float *spectrum = fft->getFFT();
            // Use 320 bands like the original renderSpectrum (first 320 of 512 FFT values)
            // Pass live scalefactor and decayfactor values so keypress changes propagate
            m_spectrum_widget->updateSpectrum(spectrum, PsyMP3::Core::SpectrumConfig::NumBands, scalefactor, decayfactor);
        }

    }
}

/**
 * @brief Main GUI update function, called periodically from the event loop.
 * Orchestrates state updates and UI thread rendering.
 * @return true if the current track has finished, false otherwise.
 */
bool Player::updateGUI()
{
    // Coalesce GUI iterations: AppLoopTimer (the 33ms SDL timer) skips queuing a
    // new RUN_GUI_ITERATION while one is already being rendered, preventing the
    // event queue from backlogging when a frame takes longer than the period.
    // RAII so the flag is always cleared, even on an early return/exception.
    Player::guiRunning = true;
    struct GuiRunningResetter { ~GuiRunningResetter() { Player::guiRunning = false; } } gui_running_resetter;

    Stream* current_stream = nullptr;
    unsigned long current_pos_ms = 0;
    unsigned long total_len_ms = 0;
    TagLib::String artist = "";
    TagLib::String title = "";

    {
        std::lock_guard<std::mutex> lock(*mutex);
        updateState(current_stream, current_pos_ms, total_len_ms, artist, title);
    }

    // Render the overlay and widget tree regardless of stream state so
    // labels, test windows, and other UI remain visible when playback is
    // idle or between tracks.
    renderOverlay(current_stream, current_pos_ms);

    // Now use the copied data for stream-specific integration, outside the lock.
    if(current_stream) {
#ifdef HAVE_DBUS
        // Update MPRIS position (outside of Player mutex to avoid deadlocks)
        if (m_mpris_manager && state == PlayerState::Playing) {
            // Update position periodically (convert ms to microseconds)
            static Uint32 last_position_update = 0;
            Uint32 current_time = SDL_GetTicks();
            if (current_time - last_position_update > 1000) { // Update every second
                m_mpris_manager->updatePosition(static_cast<uint64_t>(current_pos_ms) * 1000);
                last_position_update = current_time;
            }
        }
#endif
    }
    // Now use the copied data for rendering, outside the lock.
    if(current_stream) {
        m_labels.at("position")->setText("Position: " + convertInt(current_pos_ms / 60000)
                                + ":" + convertInt((current_pos_ms / 1000) % 60, 2)
                                + "." + convertInt((current_pos_ms / 10) % 100, 2)
                                + "/" + convertInt(total_len_ms / 60000)
                                + ":" + convertInt((total_len_ms / 1000) % 60, 2)
                                + "." + convertInt((total_len_ms / 10) % 100, 2));
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

    // and if end of stream...
    // Do not signal end-of-track if we are in the middle of loading a new one.
    if (m_loading_track || m_preloading_track) {
        return false;
    }
    return audio ? audio->isFinished() : false;
}

// ---- Named player actions (shared by key shortcuts and the menu bar) -------

void Player::volumeUp()   { setVolume(getVolume() + 0.05); }
void Player::volumeDown() { setVolume(getVolume() - 0.05); }

void Player::setIntensity(int factor)
{
    scalefactor = factor;
    updateInfo();
}

void Player::setDelay(float factor)
{
    decayfactor = factor;
    updateInfo();
}

void Player::setFFTMode(FFTMode mode)
{
    fft->setFFTMode(mode);
    showToast("FFT Mode: " + fft->getFFTModeName());
    updateInfo();
}

void Player::cycleFFTMode()
{
    FFTMode next_mode;
    switch (fft->getFFTMode()) {
        case FFTMode::Original:  next_mode = FFTMode::Optimized; break;
        case FFTMode::Optimized: next_mode = FFTMode::NeomatIn;  break;
        case FFTMode::NeomatIn:  next_mode = FFTMode::NeomatOut; break;
        case FFTMode::NeomatOut: next_mode = FFTMode::Original;  break;
        default:                 next_mode = FFTMode::Original;  break; // Should not happen
    }
    setFFTMode(next_mode);
}

void Player::cycleLoopMode()
{
    LoopMode next_mode;
    switch (m_loop_mode) {
        case LoopMode::None: next_mode = LoopMode::One;  break;
        case LoopMode::One:  next_mode = LoopMode::All;  break;
        case LoopMode::All:  next_mode = LoopMode::None; break;
        default:             next_mode = LoopMode::None; break;
    }
    setLoopMode(next_mode);
}

void Player::toggleZoom()
{
    if (!screen) return;
    const int next = (screen->getLogicalScale() == 1) ? 2 : 1;
    screen->setLogicalScale(next);
    showToast(next == 1 ? "Scale: 1x" : "Scale: 2x (pixel-doubled)");
}

/**
 * @brief Handles key press events.
 * This function contains the main logic for all keyboard shortcuts.
 * @param keysym The SDL_keysym structure for the pressed key.
 * @return `true` if the event signals that the application should exit, `false` otherwise.
 */
bool Player::handleKeyPress(const SDL_keysym& keysym)
{
    if (TextInputWidget::handleFocusedKeyPress(keysym)) {
        return false;
    }

    // Route keys through the menu bar first. When closed it only claims
    // Alt+<mnemonic> (to open a menu); while open it captures all navigation
    // keys (arrows/Enter/Esc/mnemonics) so they don't fall through to the
    // global shortcuts below.
    if (m_menu_bar && m_menu_bar->handleKey(keysym)) {
        return false;
    }

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

#ifdef HAVE_FILEDIALOG
        case SDLK_o:
            if (keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) {
                openTracksReplacingPlaylist();
            }
            break;

        case SDLK_i:
            openInsertDialog();
            break;

        case SDLK_l:
            openTemporaryTrackDialog();
            break;
#endif

        case SDLK_UP:
            volumeUp();
            break;

        case SDLK_DOWN:
            volumeDown();
            break;

        case SDLK_0: setIntensity(0); break;
        case SDLK_1: setIntensity(1); break;
        case SDLK_2: setIntensity(2); break;
        case SDLK_3: setIntensity(3); break;
        case SDLK_4: setIntensity(4); break;

        case SDLK_z: setDelay(0.5f); break;
        case SDLK_x: setDelay(1.0f); break;
        case SDLK_c: setDelay(2.0f); break;

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
            cycleLoopMode();
            break;

        case SDLK_f:
            cycleFFTMode();
            break;

        case SDLK_g:
            toggleZoom();
            break;

        // The H, B, and J keys (test window H, test window B, random windows)
        // are deliberately disabled: their handlers are intentionally omitted
        // here. The toggleTestWindowH/toggleTestWindowB/createRandomWindows
        // methods are kept so the feature can be re-wired quickly if needed.

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

        default:
            // No action for other keys
            break;
    }

    return false; // Do not exit
}

/**
 * @brief Displays a short-lived "toast" notification on the screen.
 * Crossfades any existing toast into a newly created one.
 * @param message The text message to display.
 * @param duration_ms The duration in milliseconds for the toast to be visible.
 */
void Player::showToast(const std::string& message, Uint32 duration_ms)
{
    // Crossfade any existing toasts into the new one instead of snapping them
    // out immediately. The incoming toast uses its normal 350ms fade-in.
    ApplicationWidget::getInstance().removeAllToasts(ToastWidget::CROSSFADE_MS);
    
    // Convert to new ToastWidget system
    auto toast = std::make_unique<ToastWidget>(message, font.get(), static_cast<int>(duration_ms));
    
    // Position toast at center-bottom of screen
    Rect toast_pos = toast->getPos();
    toast_pos.x((640 - toast_pos.width()) / 2);  // Center horizontally
    toast_pos.y(350 - toast_pos.height() - 40);   // 40px above bottom of FFT area
    toast->setPos(toast_pos);
    
    // Add to the dedicated toast band so notifications stay above ordinary
    // windows without consuming the emergency/system overlay slot.
    ApplicationWidget::getInstance().addWindow(std::move(toast), ZOrder::TOAST);
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
            handleStartFirstTrackEvent();
            break;
        case DO_NEXT_TRACK: 
            handleDoNextTrackEvent();
            break;
        case DO_PREV_TRACK:
            handleDoPrevTrackEvent();
            break;
        case TRACK_LOAD_SUCCESS: 
            handleTrackLoadSuccessEvent(static_cast<TrackLoadResult*>(event.data1));
            break;
        case TRACK_LOAD_FAILURE:
            handleTrackLoadFailureEvent(static_cast<TrackLoadResult*>(event.data1));
            break;
        case TRACK_PRELOAD_SUCCESS:
            handleTrackPreloadSuccessEvent(static_cast<TrackLoadResult*>(event.data1));
            break;
        case TRACK_PRELOAD_FAILURE:
            handleTrackPreloadFailureEvent(static_cast<TrackLoadResult*>(event.data1));
            break;
        case RUN_GUI_ITERATION:
            handleRunGuiIterationEvent();
            break;
        case TRACK_SEAMLESS_SWAP:
            handleTrackSeamlessSwapEvent();
            break;
        case DO_SAVE_PLAYLIST:
            handleDoSavePlaylistEvent();
            break;
        case SHOW_NOTIFICATION:
            handleShowNotificationEvent(static_cast<std::pair<std::string, NotificationType>*>(event.data1));
            break;
        case DO_SET_LOOP_MODE:
            handleDoSetLoopModeEvent(static_cast<LoopMode>(reinterpret_cast<intptr_t>(event.data1)));
            break;
    }
    return false; // Do not exit
}

/**
 * @brief Initialises SDL and all subsystems, builds the UI widget tree, and starts background threads.
 *
 * Must be called once before `EventLoop()`. Returns `false` and logs an error
 * if SDL initialisation fails. On success, the main window is visible and the
 * playlist populator thread is running.
 *
 * @param options Parsed command-line / configuration options.
 * @return `true` on success, `false` if any critical initialisation step failed.
 */
bool Player::Initialize(const PlayerOptions& options) {
    // Apply settings from the parsed options.
    scalefactor = options.scalefactor;
    decayfactor = options.decayfactor;
    m_automated_test_mode = options.automated_test_mode;
    m_unattended_quit = options.unattended_quit;
    m_show_mpris_errors = options.show_mpris_errors;

    // Initialize only the SDL subsystems needed to bring up the UI promptly.
    // Audio is initialized on demand in Audio::setup() so a stuck backend
    // (for example PipeWire) cannot hang the entire application at startup.
    if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
    {
        Debug::log("system", "Unable to init SDL: ", SDL_GetError());
        return false;
    }
    SDL_StartTextInput();



    Debug::log("system", "System::getStoragePath: ", System::getStoragePath().to8Bit(true));
    Debug::log("system", "System::getUser: ", System::getUser().to8Bit(true));
    Debug::log("system", "System::getHome: ", System::getHome().to8Bit(true));

    // Initialize UI and essential components first to show the window quickly.
    screen = std::make_unique<Display>();
#ifndef _WIN32
    // Set the SDL window/taskbar icon from the raw RGBA blob shipped in the data
    // dir (best-effort; a missing icon is non-fatal). Windows uses the icon
    // compiled into the exe via res/psymp3.rc, so this is Unix-only.
    {
        constexpr int icon_w = 128, icon_h = 128;
        constexpr size_t icon_bytes = static_cast<size_t>(icon_w) * icon_h * 4;
        for (const char* path : { PSYMP3_DATADIR "/psymp3.rgba", "./res/psymp3.rgba" }) {
            std::ifstream f(path, std::ios::binary);
            if (!f) continue;
            std::vector<uint8_t> rgba(icon_bytes);
            if (f.read(reinterpret_cast<char*>(rgba.data()), icon_bytes) &&
                static_cast<size_t>(f.gcount()) == icon_bytes) {
                screen->setWindowIcon(rgba.data(), icon_w, icon_h);
                break;
            }
        }
    }
#endif
    system = std::make_unique<System>();
#ifdef _WIN32
    System::setMainWindow(screen->getWindowHandle());
    Debug::log("system", "System::getHwnd: ", std::hex, System::getHwnd());
    system->InitializeIPC(this);
#endif
#if defined(_WIN32)
    // Font is embedded in the exe (see loadUiFont); no external vera.ttf needed.
    font = loadUiFont(12);
    // Create a larger font for status indicators like the pause message.
    m_large_font = loadUiFont(36);
#else
    font = std::make_unique<Font>(TagLib::String(PSYMP3_DATADIR "/vera.ttf"), 12);
    // Create a larger font for status indicators like the pause message.
    m_large_font = std::make_unique<Font>(TagLib::String(PSYMP3_DATADIR "/vera.ttf"), 36);
#endif // _WIN32
    Debug::log("font", "font->isValid(): ", font->isValid());
    
    graph = std::make_unique<Surface>(640, 400);
    // Enable alpha blending for the graph surface itself. This is crucial for it to be a valid
    // destination for other alpha-blended surfaces (like the fade effect, toasts, etc.).
    graph->SetAlpha(255);
    precomputeSpectrumColors();

    // Create an empty playlist. It will be populated in the background.
    playlist = std::make_unique<Playlist>();

    fft = std::make_unique<FastFourier>();
    mutex = std::make_unique<std::mutex>();

    // Set FFT mode after FFT object is created
    fft->setFFTMode(options.fft_mode);

    // NOTE (branch inapp-menu): the native Win32 menu (installWin32Menu) is
    // intentionally NOT used here; this branch uses the cross-platform in-app
    // MenuBarWidget built below, after the widget tree exists.

    // Initialize the ApplicationWidget as the root of the widget tree
    ApplicationWidget& app_widget = ApplicationWidget::getInstance(*screen);
    m_ui_root = &app_widget; // Reference to singleton - not owned
    
    // Add UI elements directly to ApplicationWidget (acts as the desktop/background)
    
    // Create a spectrum analyzer widget and add it to ApplicationWidget
    auto spectrum_widget = std::make_unique<SpectrumAnalyzerWidget>(640, 350);
    spectrum_widget->setPos(Rect(0, 0, 640, 350));
    m_spectrum_widget = spectrum_widget.get(); // Keep raw pointer for updates
    app_widget.addChild(std::move(spectrum_widget)); // ApplicationWidget takes ownership

    // Group the lower HUD under a solid black panel so the entire strip is
    // cleared together before any labels or progress elements repaint.
    auto hud_panel = std::make_unique<LayoutWidget>(640, 50, false);
    hud_panel->setPos(Rect(0, 350, 640, 50));
    hud_panel->setBackgroundColor(0, 0, 0);
    auto* hud_panel_ptr = hud_panel.get();
    
    // Create progress bar frame widget with hierarchical structure inside the HUD panel
    auto progress_frame_widget = std::make_unique<ProgressBarFrameWidget>();
    progress_frame_widget->setPos(Rect(399, 20, 222, 16));
    m_progress_widget = progress_frame_widget->getProgressBar(); // Get nested progress bar
    hud_panel_ptr->addChild(std::move(progress_frame_widget));
    
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
    
    auto add_label = [&](Widget& parent, const std::string& key, const Rect& pos, bool marquee = false) {
        auto label = std::make_unique<Label>(font.get(), pos);
        label->setMarqueeEnabled(marquee);
        m_labels[key] = label.get(); // Store non-owning pointer in map
        parent.addChild(std::move(label));
    };

    add_label(*hud_panel_ptr, "artist",   Rect(1, 4, 240, 16), true);
    add_label(*hud_panel_ptr, "title",    Rect(1, 19, 350, 16), true);
    add_label(*hud_panel_ptr, "album",    Rect(1, 34, 350, 16), true);
    add_label(*hud_panel_ptr, "playlist", Rect(270, 4, 120, 16));
    add_label(*hud_panel_ptr, "position", Rect(400, 3, 150, 16));
    // Shifted down by the menu-bar height so the in-app menu bar doesn't cover them.
    add_label(app_widget,     "scale",    Rect(545, MenuBarWidget::BAR_H + 0, 95, 16));
    add_label(app_widget,     "decay",    Rect(545, MenuBarWidget::BAR_H + 15, 95, 16));
    add_label(app_widget,     "fft_mode", Rect(545, MenuBarWidget::BAR_H + 30, 95, 16));

    app_widget.addChild(std::move(hud_panel));

    // In-app menu bar (cross-platform, top-most overlay). Mirrors the I, L, F,
    // Z/X/C and 1-4 keys; items call the same actions the keys do.
    {
        using MI = MenuBarWidget::Item;
        auto menu_bar = std::make_unique<MenuBarWidget>(640, 400, font.get());
        m_menu_bar = menu_bar.get();

        std::vector<MI> file_items;
#ifdef HAVE_FILEDIALOG
        file_items.push_back(MI::leaf("&Open Tracks...", [this]{ openTracksReplacingPlaylist(); }, nullptr, "Ctrl+O"));
#endif
        file_items.push_back(MI::leaf("&Clear Playlist", [this]{ clearPlaylist(); }));
        file_items.push_back(MI::sep());
#ifdef HAVE_FILEDIALOG
        file_items.push_back(MI::leaf("&Insert Track(s)", [this]{ openInsertDialog(); }, nullptr, "I"));
        file_items.push_back(MI::leaf("Temp &Load Track", [this]{ openTemporaryTrackDialog(); }, nullptr, "L"));
        file_items.push_back(MI::sep());
#endif
        file_items.push_back(MI::leaf("E&xit", []{ Player::synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr); }));
        menu_bar->addMenu("&File", std::move(file_items));

        // Playback: mirrors Space (pause), P/N (prev/next), Up/Down (volume).
        std::vector<MI> playback_items;
        playback_items.push_back(MI::leaf("&Pause", [this]{ playPause(); },
            [this]{ return state == PlayerState::Paused; }, "Space"));
        playback_items.push_back(MI::sep());
        playback_items.push_back(MI::leaf("Pre&vious Track", [this]{ prevTrack(); }, nullptr, "P"));
        playback_items.push_back(MI::leaf("&Next Track", [this]{ nextTrack(); }, nullptr, "N"));
        playback_items.push_back(MI::sep());
        playback_items.push_back(MI::leaf("Volume &Up", [this]{ volumeUp(); }, nullptr, "Up"));
        playback_items.push_back(MI::leaf("Volume &Down", [this]{ volumeDown(); }, nullptr, "Dn"));
        playback_items.push_back(MI::sep());
        playback_items.push_back(MI::leaf("&Equalizer...", [this]{ toggleEqualizerWindow(); }));
        menu_bar->addMenu("&Playback", std::move(playback_items));

        auto fft_mode_item = [this](const char* label, FFTMode mode) {
            return MI::leaf(label,
                [this, mode]{ setFFTMode(mode); },
                [this, mode]{ return fft->getFFTMode() == mode; });
        };
        auto delay_item = [this](const char* label, float value, const char* sc = "") {
            return MI::leaf(label,
                [this, value]{ setDelay(value); },
                [this, value]{ return decayfactor == value; }, sc);
        };
        auto intensity_item = [this](const char* label, int value) {
            return MI::leaf(label,
                [this, value]{ setIntensity(value); },
                [this, value]{ return scalefactor == value; });
        };

        std::vector<MI> settings_items;
        settings_items.push_back(MI::sub("FFT Mode", {
            fft_mode_item("mat-og", FFTMode::Original),
            fft_mode_item("vibe-1", FFTMode::Optimized),
            fft_mode_item("neomat-in", FFTMode::NeomatIn),
            fft_mode_item("neomat-out", FFTMode::NeomatOut),
        }));
        settings_items.push_back(MI::sub("Delay", {
            delay_item("&Long", 0.5f, "Z"),
            delay_item("&Normal", 1.0f, "X"),
            delay_item("&Short", 2.0f, "C"),
        }));
        settings_items.push_back(MI::sub("Intensity", {
            intensity_item("1", 1), intensity_item("2", 2),
            intensity_item("3", 3), intensity_item("4", 4),
        }));
        settings_items.push_back(MI::sep());
        settings_items.push_back(MI::leaf("2x &Zoom", [this]{ toggleZoom(); },
            [this]{ return screen && screen->getLogicalScale() == 2; }, "G"));
        menu_bar->addMenu("&Settings", std::move(settings_items));

        menu_bar->setZOrder(ZOrder::MAX); // sort above toasts
        app_widget.addWindow(std::move(menu_bar), ZOrder::MAX);
    }
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

    if (m_automated_test_mode) {
        Debug::log("test", "Automated test mode enabled.");
    }
    m_app_loop_timer_id = SDL_AddTimer(33, Player::AppLoopTimer, nullptr);
    if (m_automated_test_mode) {
        m_automated_test_timer_id = SDL_AddTimer(1000, Player::AutomatedTestTimer, this);
    }

    return true;
}

/**
 * @brief Runs the SDL main event loop until the application is requested to quit.
 *
 * Processes `SDL_KEYDOWN`, `SDL_KEYUP`, `SDL_MOUSEBUTTON*`, `SDL_MOUSEMOTION`,
 * `SDL_USEREVENT`, and `SDL_QUIT` events, dispatching them to the appropriate
 * handlers. Calls `processDeferredDeletions()` after each event.
 */
void Player::EventLoop() {
    bool done = false;
    while (!done) {
        // message processing loop
        SDL_Event event;
        while (SDL_WaitEvent(&event)) {
            // Map mouse positions from window coords into logical coords so
            // hit-testing matches the unscaled widget tree. Relative deltas
            // (xrel/yrel) are left in window units to avoid truncating
            // 1-pixel motions to zero.
            if (screen) {
                const int s = screen->getLogicalScale();
                if (s > 1) {
                    switch (event.type) {
                        case SDL_MOUSEBUTTONDOWN:
                        case SDL_MOUSEBUTTONUP:
                            event.button.x /= s;
                            event.button.y /= s;
                            break;
                        case SDL_MOUSEMOTION:
                            event.motion.x /= s;
                            event.motion.y /= s;
                            break;
                        default:
                            break;
                    }
                }
            }

            switch (event.type) {
            case SDL_WINDOWEVENT:
                if (handleWindowEvent(event.window)) {
                    synthesizeUserEvent(RUN_GUI_ITERATION, nullptr, nullptr);
                }
                break;
                // exit if the window is closed
            case SDL_QUIT:
                done = true;
                break;

#ifdef _WIN32
            case SDL_SYSWMEVENT:
                // Native menu bar clicks arrive as WM_COMMAND (HIWORD(wParam)==0
                // for menus, 1 for accelerators).
                if (event.syswm.msg && event.syswm.msg->subsystem == SDL_SYSWM_WINDOWS) {
                    const auto& w = event.syswm.msg->msg.win;
                    if (w.msg == WM_COMMAND && HIWORD(w.wParam) == 0) {
                        handleWin32MenuCommand(LOWORD(w.wParam));
                    }
                }
                break;
#endif

                // check for keypresses
            case SDL_KEYDOWN:
            {
                done = handleKeyPress(event.key.keysym);
                break;
            }
            case SDL_TEXTINPUT:
            {
                TextInputWidget::handleFocusedTextInput(event.text.text);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                TextInputWidget::clearFocusedWidget();

                // Dispatch by visual priority: a window-owned mouse capture is
                // authoritative (routed even while a menu is open, so a drag's
                // release is never lost); otherwise the menu bar (topmost) gets
                // first claim, then the floating windows (which consume all
                // in-bounds clicks), then the main widget tree, then the legacy
                // handlers.
                bool handled = false;
                if (windowOwnsMouseCapture()) {
                    handled = handleWindowMouseEvents(event);
                } else {
                    if (m_menu_bar && m_menu_bar->handleMouseDown(event.button, event.button.x, event.button.y)) {
                        handled = true;
                    }
                    if (!handled) {
                        handled = handleWindowMouseEvents(event);
                    }
                }
                if (!handled) {
                    if (m_use_widget_mouse_handling && m_ui_root) {
                        handled = m_ui_root->handleMouseDown(event.button, event.button.x, event.button.y);
                    }
                    // Fall back to old handler if the widget tree didn't handle it
                    if (!handled) {
                        handleMouseButtonDown(event.button);
                    }
                }
                break;
            }
            case SDL_MOUSEMOTION:
            {
                // The legacy seek drag holds no widget capture; keep feeding it
                // directly so moving over a window mid-drag doesn't freeze it.
                if (m_is_dragging && !Widget::getMouseCapturedWidget()) {
                    handleMouseMotion(event.motion);
                    break;
                }

                bool handled = false;
                if (windowOwnsMouseCapture()) {
                    handled = handleWindowMouseEvents(event);
                } else {
                    if (m_menu_bar && m_menu_bar->handleMouseMotion(event.motion, event.motion.x, event.motion.y)) {
                        handled = true; // an open menu owns hover
                    }
                    if (!handled) {
                        handled = handleWindowMouseEvents(event);
                    }
                }
                if (!handled) {
                    if (m_use_widget_mouse_handling && m_ui_root) {
                        handled = m_ui_root->handleMouseMotion(event.motion, event.motion.x, event.motion.y);
                    }
                    // Fall back to old handler if the widget tree didn't handle it
                    if (!handled) {
                        handleMouseMotion(event.motion);
                    }
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                // Complete a legacy seek drag no matter what is under the cursor.
                if (m_is_dragging && !Widget::getMouseCapturedWidget()) {
                    handleMouseButtonUp(event.button);
                    break;
                }

                // The menu bar sees the release first (unless a window-owned
                // capture is active): leaf items activate on mouse-up, so a
                // press-drag-release gesture selects the item under the cursor.
                bool handled = false;
                if (windowOwnsMouseCapture()) {
                    handled = handleWindowMouseEvents(event);
                } else {
                    if (m_menu_bar && m_menu_bar->handleMouseUp(event.button, event.button.x, event.button.y)) {
                        handled = true;
                    }
                    if (!handled) {
                        handled = handleWindowMouseEvents(event);
                    }
                }
                if (!handled) {
                    if (m_use_widget_mouse_handling && m_ui_root) {
                        handled = m_ui_root->handleMouseUp(event.button, event.button.x, event.button.y);
                    }
                    // Fall back to old handler if the widget tree didn't handle it
                    if (!handled) {
                        handleMouseButtonUp(event.button);
                    }
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
}

/**
 * @brief Performs final cleanup after the event loop exits.
 *
 * Removes the GUI timer, resets Windows taskbar progress, stops audio,
 * logs the clean-exit message, and calls `SDL_Quit()`.
 */
void Player::Cleanup() {
    if (m_app_loop_timer_id) {
        SDL_RemoveTimer(m_app_loop_timer_id);
    }
    SDL_StopTextInput();
#ifdef _WIN32
    if (system) system->progressState(TBPF_NOPROGRESS);
    if (system) system->updateProgress(0, 0);
    System::setMainWindow(nullptr);
#endif
    if (audio) audio->play(false);

    // Stop and join the background threads BEFORE SDL_Quit(): a loader thread
    // finishing a slow load would otherwise call synthesizeUserEvent ->
    // SDL_PushEvent against a torn-down event subsystem (use-after-free inside
    // SDL). ~Player also joins them, but its joinable() guards make that a
    // no-op once we have joined here.
    m_loader_active = false;
    m_loader_queue_cv.notify_one();
    if (m_loader_thread.joinable()) {
        m_loader_thread.join();
    }
    if (m_playlist_populator_thread.joinable()) {
        m_playlist_populator_thread.join();
    }

    // all is well ;)
    Debug::log("player", "Exited cleanly");
    SDL_Quit();
}

/**
 * @brief The main entry point and run loop for the Player.
 * This function initializes SDL and all major components (display, fonts, UI),
 * starts the main event loop, and handles cleanup on exit.
 * @param args The vector of command-line arguments passed to the application.
 */
void Player::Run(const PlayerOptions& options) {
    if (Initialize(options)) {
        EventLoop();
        Cleanup();
    }
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
/**
 * @brief SDL timer callback used in automated testing: skips to the next track.
 *
 * Fires at a fixed interval in automated-test mode. After 5 invocations,
 * removes itself and installs `AutomatedQuitTimer`.
 *
 * @param interval The timer interval in ms.
 * @param param    Pointer to the `Player` instance.
 * @return The interval for the next callback.
 */
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

/**
 * @brief SDL timer callback used in automated testing: terminates the application.
 *
 * Fires once (after `AutomatedTestTimer` arms it) to synthesise a
 * `QUIT_APPLICATION` event.
 *
 * @param interval The timer interval in ms.
 * @param param    Pointer to the `Player` instance.
 * @return The interval for the next callback.
 */
Uint32 Player::AutomatedQuitTimer(Uint32 interval, void* param) {
    Player* player = static_cast<Player*>(param);
    if (player) {
        player->synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
    }
    return interval;
}

/**
 * @brief Handles the case where the current track cannot be played.
 *
 * Increments `m_skip_attempts` and requests the next (or previous) track
 * depending on `m_navigation_direction`. Respects `m_loop_mode`. Returns
 * `false` when all tracks have been attempted without success.
 *
 * @return `true` if another track has been requested, `false` if nothing more can be tried.
 */
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

bool Player::handleWindowEvent(const SDL_WindowEvent& event)
{
    if (!screen) {
        return false;
    }

    return screen->handleWindowEvent(event);
}

/**
 * @brief Scans the playlist from position 0 for the first track that `MediaFile::open` accepts.
 *
 * Used at startup (after a `START_FIRST_TRACK` event) to skip over any
 * unreadable entries and begin playback at the first valid track.
 *
 * @return `true` if a playable track was found and loading was requested.
 */
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
    
    size_t capacity_needed = m_random_windows.size();
    if (m_test_window_h) capacity_needed++;
    if (m_test_window_b) capacity_needed++;
    sorted_windows.reserve(capacity_needed);

    if (m_test_window_h) {
        sorted_windows.push_back(m_test_window_h.get());
    }
    
    if (m_test_window_b) {
        sorted_windows.push_back(m_test_window_b.get());
    }
    
    // Add all random windows
    sorted_windows.reserve(sorted_windows.size() + m_random_windows.size());
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
 * @return true when a window consumed the event (it was dispatched to a window
 *         that holds the mouse capture, or it landed within a window's bounds —
 *         windows occlude whatever is beneath them, so an in-bounds event is
 *         consumed even if no inner widget claimed it).
 */
bool Player::handleWindowMouseEvents(const SDL_Event& event)
{
    // Create list of windows sorted by z-order (highest first for event handling)
    std::vector<WindowFrameWidget*> sorted_windows;
    
    size_t capacity_needed = m_random_windows.size();
    if (m_test_window_h) capacity_needed++;
    if (m_test_window_b) capacity_needed++;
    sorted_windows.reserve(capacity_needed);

    if (m_test_window_h) {
        sorted_windows.push_back(m_test_window_h.get());
    }
    
    if (m_test_window_b) {
        sorted_windows.push_back(m_test_window_b.get());
    }
    
    // Add all random windows
    sorted_windows.reserve(sorted_windows.size() + m_random_windows.size());
    for (const auto& window : m_random_windows) {
        sorted_windows.push_back(window.get());
    }
    
    std::sort(sorted_windows.begin(), sorted_windows.end(),
              [](const WindowFrameWidget* a, const WindowFrameWidget* b) {
                  return a->getZOrder() > b->getZOrder();
              });
    
    bool handled_any_window = false;
    Widget* captured = Widget::getMouseCapturedWidget();

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
        bool has_capture = widgetBelongsToWindow(captured, window);

        // A capture held OUTSIDE the window system (e.g. the main UI's progress
        // bar mid-drag) must keep receiving events; windows must not occlude it.
        if (captured && !has_capture) {
            continue;
        }

        bool in_bounds = (mouse_x >= window_rect.x() && mouse_x < window_rect.x() + window_rect.width() &&
                         mouse_y >= window_rect.y() && mouse_y < window_rect.y() + window_rect.height());

        if (has_capture || in_bounds) {
            int relative_x = mouse_x - window_rect.x();
            int relative_y = mouse_y - window_rect.y();

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                window->handleMouseDown(event.button, relative_x, relative_y);
            } else if (event.type == SDL_MOUSEMOTION) {
                window->handleMouseMotion(event.motion, relative_x, relative_y);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                window->handleMouseUp(event.button, relative_x, relative_y);
            }

            // Consume regardless of whether an inner widget claimed it: the
            // topmost window at this point occludes everything beneath, so the
            // event must not fall through to lower windows, the main UI, or the
            // legacy handlers (e.g. the seek-bar hitbox).
            handled_any_window = true;
            break;
        }
    }

    if (event.type == SDL_MOUSEMOTION && !handled_any_window && captured == nullptr) {
        WindowFrameWidget::restoreDefaultCursor();
    }

    return handled_any_window;
}

/**
 * @brief Whether the widget currently holding the mouse capture lives inside
 *        one of the Player-managed floating windows.
 */
bool Player::windowOwnsMouseCapture() const
{
    Widget* captured = Widget::getMouseCapturedWidget();
    if (!captured) return false;
    if (m_test_window_h && widgetBelongsToWindow(captured, m_test_window_h.get())) return true;
    if (m_test_window_b && widgetBelongsToWindow(captured, m_test_window_b.get())) return true;
    for (const auto& window : m_random_windows) {
        if (widgetBelongsToWindow(captured, window.get())) return true;
    }
    return false;
}

/**
 * @brief Toggles the test window H with the Win3 control demo content.
 *
 * Deliberately unwired: the H key handler was removed in handleKeyPress. This
 * is retained (not dead code) so the debug window can be re-enabled quickly.
 */
void Player::toggleTestWindowH()
{
    if (m_test_window_h) {
        // Close the window
        m_test_window_h.reset();
        showToast("Test Window H: Closed");
    } else {
        // Open the window using the same WindowFrameWidget path as the other test windows,
        // but preserve H's normal resizable window behavior.
        m_test_window_h = std::make_unique<WindowFrameWidget>(170, 142, "H", font.get());
        m_test_window_h->setMinimizable(false);
        m_test_window_h->setMaximizable(false);
        m_test_window_h->setClientArea(createTestWindowHClient(font.get()));
        m_test_window_h->refresh();
        
        // Only set position, keep the calculated size from refresh
        Rect calculated_size = m_test_window_h->getPos();
        m_test_window_h->setPos(Rect(434, 72, calculated_size.width(), calculated_size.height()));
        
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
            showToast("H Window: Closed");
        });
        
        m_test_window_h->setOnMinimize([this]() {
            showToast("H Window: Minimized");
        });
        
        m_test_window_h->setOnMaximize([this]() {
            showToast("H Window: Maximized");
        });
        
        m_test_window_h->setOnControlMenu([this]() {
            showToast("H Window: Control Menu");
        });
        
        m_test_window_h->setOnResize([this](int new_width, int new_height) {
            showToast("H Window: Resized to " + std::to_string(new_width) + "x" + std::to_string(new_height));
        });
        
        showToast("H Window: Opened");
    }
}

void Player::applyEqStateToAudio()
{
    if (!audio) return;
    for (int i = 0; i < Equalizer::kNumBands; ++i)
        audio->setEqBandGain(i, m_eq_gains[i]);
    audio->setEqEnabled(m_eq_enabled);
}

namespace {
std::string settingsFilePath()
{
    return System::getStoragePath().to8Bit(true) + "/psymp3.conf";
}

// Parse a full numeric token; rejects prefix garbage the way the .psymp3eq
// loader does.
bool parseSettingDouble(const std::string& s, double& out)
{
    try {
        size_t used = 0;
        double v = std::stod(s, &used);
        if (used == s.size()) { out = v; return true; }
    } catch (const std::exception&) {}
    return false;
}
} // namespace

void Player::loadSettings()
{
    std::ifstream f(System::pathFromUtf8(settingsFilePath()));
    if (!f) {
        return; // first run: keep defaults
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();

        double v = 0.0;
        if (key == "volume") {
            if (parseSettingDouble(value, v))
                m_volume = static_cast<float>(std::clamp(v, 0.0, 1.0));
        } else if (key == "eq_enabled") {
            m_eq_enabled = (value == "1" || value == "true");
        } else if (key.rfind("eq_band_", 0) == 0) {
            try {
                size_t used = 0;
                int band = std::stoi(key.substr(8), &used);
                if (used == key.size() - 8 && band >= 0 &&
                    band < static_cast<int>(m_eq_gains.size()) &&
                    parseSettingDouble(value, v)) {
                    m_eq_gains[band] = static_cast<float>(std::clamp(
                        v, static_cast<double>(Equalizer::kMinGainDb),
                           static_cast<double>(Equalizer::kMaxGainDb)));
                }
            } catch (const std::exception&) {
                // Malformed band index: skip the line.
            }
        }
    }
    Debug::log("player", "Settings loaded: volume=", m_volume, ", eq_enabled=", m_eq_enabled);
}

void Player::saveSettings() const
{
    System::createStoragePath();
    std::ofstream f(System::pathFromUtf8(settingsFilePath()), std::ios::out | std::ios::trunc);
    if (!f) {
        Debug::log("player", "Failed to write settings file: ", settingsFilePath());
        return;
    }
    f << "# PsyMP3 settings\n";
    f << "volume=" << m_volume << "\n";
    f << "eq_enabled=" << (m_eq_enabled ? 1 : 0) << "\n";
    for (size_t i = 0; i < m_eq_gains.size(); ++i)
        f << "eq_band_" << i << "=" << m_eq_gains[i] << "\n";
}

void Player::toggleEqualizerWindow()
{
    if (m_eq_window) {
        auto it = std::find_if(m_random_windows.begin(), m_random_windows.end(),
                               [this](const auto& w) { return w.get() == m_eq_window; });
        if (it != m_random_windows.end()) {
            deferWidgetDeletion(std::move(*it));
            m_random_windows.erase(it);
        }
        m_eq_window = nullptr;
        m_eq_client = nullptr;
        showToast("Equalizer: Closed");
        return;
    }

    std::vector<std::string> labels;
    std::vector<double> gains;
    for (int i = 0; i < Equalizer::kNumBands; ++i) {
        labels.emplace_back(Equalizer::bandLabel(i));
        gains.push_back(m_eq_gains[i]);
    }

    auto client = std::make_unique<EqualizerWindow>(
        font.get(), labels, Equalizer::kMinGainDb, Equalizer::kMaxGainDb, gains, m_eq_enabled,
        getVolume());
    m_eq_client = client.get();
    client->setOnVolumeChanged([this](double v) { setVolume(v); });
    client->setOnBandChanged([this](int band, double db) {
        if (band >= 0 && band < static_cast<int>(m_eq_gains.size()))
            m_eq_gains[band] = static_cast<float>(db);
        if (audio) audio->setEqBandGain(band, static_cast<float>(db));
    });
    client->setOnEnabledChanged([this](bool on) {
        m_eq_enabled = on;
        if (audio) audio->setEqEnabled(on);
    });
    client->setOnStatus([this](const std::string& msg) { showToast(msg); });

    const int cw = client->getPos().width();
    const int ch = client->getPos().height();
    auto frame = std::make_unique<WindowFrameWidget>(cw, ch, "Equalizer", font.get());
    frame->setMinimizable(false);
    frame->setMaximizable(false);
    frame->setClientArea(std::move(client));
    frame->refresh();
    Rect sz = frame->getPos();
    frame->setPos(Rect(40, 40, sz.width(), sz.height()));

    WindowFrameWidget* fp = frame.get();
    m_eq_window = fp;
    frame->setOnDrag([fp](int dx, int dy) {
        Rect p = fp->getPos(); p.x(p.x() + dx); p.y(p.y() + dy); fp->setPos(p);
    });
    frame->setOnDragStart([fp] { fp->bringToFront(); });
    frame->setOnClose([this, fp] {
        auto it = std::find_if(m_random_windows.begin(), m_random_windows.end(),
                               [fp](const auto& w) { return w.get() == fp; });
        if (it != m_random_windows.end()) {
            deferWidgetDeletion(std::move(*it));
            m_random_windows.erase(it);
        }
        m_eq_window = nullptr;
        m_eq_client = nullptr;
        showToast("Equalizer: Closed");
    });
    m_random_windows.push_back(std::move(frame));
    showToast("Equalizer: Opened");
}

/**
 * @brief Toggles the test window B (160x60 window).
 *
 * Deliberately unwired: the B key handler was removed in handleKeyPress. This
 * is retained (not dead code) so the debug window can be re-enabled quickly.
 */
void Player::toggleTestWindowB()
{
    if (m_test_window_b) {
        // Close the window
        m_test_window_b.reset();
        showToast("Test Window B: Closed");
    } else {
        // Open the window (client area is 160x60)
        m_test_window_b = std::make_unique<WindowFrameWidget>(160, 60, "Test Window B", font.get());
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

/**
 * @brief Creates five randomly-placed, randomly-sized `WindowFrameWidget` test windows.
 *
 * The J key that spawned a batch of five windows with randomised positions and
 * sizes is deliberately disabled (its handler was removed in handleKeyPress).
 * This method is retained (not dead code) so the feature can be re-enabled
 * quickly. Each window has close and drag callbacks set up to maintain the
 * `m_random_windows` vector.
 */
void Player::createRandomWindows()
{
    // Create distributions for random window properties
    std::uniform_int_distribution<int> width_dist(100, 299);
    std::uniform_int_distribution<int> height_dist(80, 229);
    std::uniform_int_distribution<int> x_dist(0, 399);
    std::uniform_int_distribution<int> y_dist(0, 299);

    // Create 5 random windows each time J is pressed
    for (int i = 0; i < 5; i++) {
        // Generate random window properties for client area
        int client_width = width_dist(m_rng);     // 100-299px wide client area
        int client_height = height_dist(m_rng);   // 80-229px tall client area
        int x = x_dist(m_rng);                    // Random X position (0-399)
        int y = y_dist(m_rng);                    // Random Y position (0-299)
        
        // Create WindowFrameWidget directly like H and B windows
        std::string title = "Random Window " + std::to_string(++m_random_window_counter);
        auto window = std::make_unique<WindowFrameWidget>(client_width, client_height, title, font.get());
        window->setPos(Rect(x, y, client_width + 8, client_height + 27)); // Include frame borders
        
        // Set up callbacks using the WindowFrameWidget system like H/B windows
        window->setOnClose([this, window_ptr = window.get()]() {
            // Remove from our vector and decrement counter
            auto it = std::find_if(m_random_windows.begin(), m_random_windows.end(),
                                  [window_ptr](const auto& w) { return w.get() == window_ptr; });
            if (it != m_random_windows.end()) {
                // Defer destruction rather than erase in place: this lambda is
                // the window's own m_on_close, so destroying the widget here
                // would run ~WindowFrameWidget while its handler is executing.
                // The H and B test windows defer for the same reason.
                deferWidgetDeletion(std::move(*it));
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

/**
 * @brief Checks whether the Last.fm scrobble threshold has been reached for the current track.
 *
 * Scrobbles when the track has been playing for at least the lesser of
 * 50 % of its duration or 4 minutes, and only for tracks longer than 30 s.
 * Called inside the GUI-update loop approximately every 30 s.
 */
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

/**
 * @brief Starts scrobbling tracking for the current track.
 *
 * Records the start time and clears the `m_track_scrobbled` flag, then
 * calls `submitNowPlaying()` to update the Last.fm "Now Playing" status.
 */
void Player::startTrackScrobbling()
{
    m_track_start_time = SDL_GetTicks();
    m_track_scrobbled = false;
    
    // Submit "Now Playing" status
    submitNowPlaying();
}

/**
 * @brief Submits the currently playing track as the Last.fm "Now Playing" status.
 *
 * Does nothing if there is no active stream or Last.fm manager.
 */
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

/**
 * @brief Sets the player volume (0.0 = mute, 1.0 = full).
 *
 * Clamps the value to [0.0, 1.0], forwards it to the `Audio` object if
 * active, shows a volume-level toast, and notifies MPRIS.
 *
 * @param volume Desired volume level.
 */
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

    // Reflect the change on the equalizer window's volume slider (no echo:
    // setVolume() there suppresses its onVolumeChanged callback).
    if (m_eq_client) {
        m_eq_client->setVolume(static_cast<double>(m_volume));
    }

#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->updateVolume(static_cast<double>(m_volume));
    }
#endif
}

/**
 * @brief Returns the current player volume.
 * @return Volume level in [0.0, 1.0].
 */
double Player::getVolume() const {
    return static_cast<double>(m_volume);
}

/**
 * @brief Enables or disables shuffle mode and shows a confirmation toast.
 *
 * Delegates to `Playlist::setShuffle()` and notifies MPRIS.
 *
 * @param shuffle `true` to enable shuffle, `false` to disable.
 */
void Player::setShuffle(bool shuffle) {
    if (playlist) {
        playlist->setShuffle(shuffle);
    }

    std::string msg = shuffle ? "Shuffle: On" : "Shuffle: Off";
    showToast(msg);

#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->updateShuffle(shuffle);
    }
#endif
}

/**
 * @brief Returns whether shuffle mode is currently active.
 * @return `true` if shuffle is on.
 */
bool Player::getShuffle() const {
    if (playlist) {
        return playlist->isShuffle();
    }
    return false;
}

/**
 * @brief Clears the list of deferred-deletion widgets.
 *
 * Called at the end of each event loop iteration to safely destroy widgets
 * whose lifetime must outlast the event that triggered their removal.
 */
void Player::processDeferredDeletions() {
    m_deferred_widgets.clear();
}

/**
 * @brief Moves a widget into the deferred-deletion queue.
 *
 * The widget's destructor is deferred until `processDeferredDeletions()` is
 * called at the end of the current event-loop iteration.
 *
 * @param widget Widget to defer-delete; must be non-null.
 */
void Player::deferWidgetDeletion(std::unique_ptr<Widget> widget) {
    if (widget) {
        m_deferred_widgets.push_back(std::move(widget));
    }
}

void Player::handleStartFirstTrackEvent() {
    if (playlist->entries() > 0) {
        // Use findFirstPlayableTrack to locate the first playable track
        if (!findFirstPlayableTrack()) {
            // No playable tracks found
            stop();
            updateInfo(false, "No playable tracks found in playlist.");
        }
    }
}

void Player::handleDoNextTrackEvent() {
    nextTrack(m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1);
}

void Player::handleDoPrevTrackEvent() {
    prevTrack();
}

void Player::handleTrackLoadSuccessEvent(TrackLoadResult* result) {
    Debug::log("loader", "Player::handleUserEvent(TRACK_LOAD_SUCCESS) called.");
    m_skip_attempts = 0; // Reset skip counter on a successful load.
    Stream* new_stream = result->stream;
    m_num_tracks_in_current_stream = result->num_chained_tracks;
    std::vector<int16_t> primed_samples = std::move(result->primed_samples);
    const bool primed_eof = result->primed_eof;
    delete result; // Free the result struct

    m_loading_track = false; // Loading complete

    // Take ownership of the new stream from the loader thread.
    std::unique_ptr<Stream> owned_new_stream(new_stream); // Take ownership immediately
    const bool recreate_audio = !canReuseAudioForStream(audio.get(), owned_new_stream.get());

    try {
        if (recreate_audio) {
            Debug::log("audio", "Track load changed audio format, recreating Audio object.");
            audio.reset();
            audio = std::make_unique<Audio>(std::move(owned_new_stream),
                                            fft.get(),
                                            mutex.get(),
                                            std::move(primed_samples),
                                            primed_eof);
            audio->setVolume(m_volume);
            applyEqStateToAudio();
        } else {
            Debug::log("audio", "Track load reusing existing Audio device.");
            audio->setStream(std::move(owned_new_stream), std::move(primed_samples), primed_eof);
        }
    } catch (const std::exception& e) {
        const std::string error_message = std::string("Audio initialization failed: ") + e.what();
        Debug::log("audio", "Player::handleTrackLoadSuccessEvent(): ", error_message);
        stream = nullptr;
        m_num_tracks_in_current_stream = 0;
        showNotification(error_message, NotificationType::Error);
        if (!handleUnplayableTrack()) {
            stop();
            updateInfo(false, toUtf8TagString(error_message));
        }
        return;
    }

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
                stream->getAlbum().to8Bit(true),
                static_cast<uint64_t>(stream->getLength()) * 1000
            );
        }
    }
#endif
#ifdef _WIN32
    if (system) system->announceNowPlaying(stream->getArtist(), stream->getTitle(), stream->getAlbum());
#endif
}

void Player::handleTrackLoadFailureEvent(TrackLoadResult* result) {
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
}

void Player::handleTrackPreloadSuccessEvent(TrackLoadResult* result) {
    // A newer request can supersede this preload while it was in flight: a
    // PlayNow (requestTrackLoad) sets m_loading_track and clears
    // m_preloading_track. In that case this stream is for the wrong track;
    // discard it instead of repopulating m_next_stream and later seamless-
    // swapping to a stale track (which also desyncs the playlist).
    if (!m_preloading_track || m_loading_track) {
        Debug::log("loader", "Discarding stale preload result (superseded by a newer request).");
        delete result->stream;
        delete result;
        return;
    }

    // Store the preloaded stream for seamless transition
    m_preloading_track = false;
    m_next_stream.reset(result->stream); // Take ownership of the preloaded stream
    // Carry the chained-stream track count forward (the swap handler moves it
    // into m_num_tracks_in_current_stream) so playlist advancement matches.
    m_num_tracks_in_next_stream = result->num_chained_tracks;
    m_next_stream_primed_samples = std::move(result->primed_samples);
    m_next_stream_primed_eof = result->primed_eof;
    Debug::log("loader", "Track preloaded successfully for seamless transition.");
    delete result; // Free the result struct but keep the stream
}

void Player::handleTrackPreloadFailureEvent(TrackLoadResult* result) {
    // Handle preload failure - no seamless transition possible
    m_preloading_track = false;
    m_next_stream_primed_samples.clear();
    m_next_stream_primed_eof = false;
    Debug::log("loader", "Failed to preload track: ", result->error_message.to8Bit(true));
    delete result;
}

void Player::handleRunGuiIterationEvent() {
#ifdef HAVE_DBUS
    // Pump incoming MPRIS D-Bus method calls / property requests on the main
    // thread (~30 Hz via the app-loop timer). Handlers call non-thread-safe
    // Player methods, so this must run here rather than on a worker thread.
    if (m_mpris_manager) {
        m_mpris_manager->processEvents();
    }
#endif
    if (updateGUI()) {
        // Track has ended.
        if (m_loop_mode == LoopMode::One) {
            // Loop current track by seeking to the beginning.
            seekToInternal(0, false);
        } else if (m_next_stream) {
            // A track was preloaded, perform seamless swap.
            synthesizeUserEvent(TRACK_SEAMLESS_SWAP, nullptr, nullptr);
        } else {
            // No preloaded track, use the old method.
            nextTrack(m_num_tracks_in_current_stream > 0 ? m_num_tracks_in_current_stream : 1);
        }
    }
}
