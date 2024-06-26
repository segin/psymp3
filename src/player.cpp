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
#include "player.h"

bool Player::guiRunning;

std::string convertInt(long number) {
   std::stringstream ss;
   ss << number;
   return ss.str();
}

std::string convertInt2(long number) {
    char s[8];
    snprintf(s, 8, "%02ld", number);
    return s;
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
}

Player::~Player() {
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
void Player::openTrack(TagLib::String path) {
    audio->lock();
    if (stream) {
        delete stream;
    }
    stream = MediaFile::open(path);
    if (stream) {
        ATdata.stream = stream;
        if (stream && (audio->getRate() != stream->getRate()) || (audio->getChannels() != stream->getChannels())) {
            pause(); /* otherwise a race condition can occur */
            audio->unlock();
            delete audio;
            audio = new Audio(&ATdata);
        } else {
            audio->unlock();
        }
        info["artist"] = font->Render("Artist: " + stream->getArtist());
        info["title"] = font->Render("Title: " + stream->getTitle());
        info["album"] = font->Render("Album: " + stream->getAlbum());
        info["playlist"] = font->Render("Playlist: " +
            convertInt(playlist->getPosition() + 1) + "/" +
            convertInt(playlist->entries()));
        play();
    }
    return;
}

/* Player control functions */
bool Player::nextTrack(void) {
    TagLib::String nextfile = playlist->next();
    if (nextfile == "") {
        return false;
    } else {
        openTrack(nextfile);
    }
    return true;
}

bool Player::prevTrack(void) {
    openTrack(playlist->prev());
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
    for (int x = 0; x < 350; x++) {
        graph->hline(0, 639, x, 64 * decayfactor);
    }
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

    screen = new Display();
    playlist = new Playlist(args);
    if (playlist->entries()) 
        try {
            stream = MediaFile::open(playlist->getTrack(0));
        } catch (const std::exception& e) { 
            std::cerr << e.what() << ": Can't open stream: " << playlist->getTrack(0) << std::endl;
            stream = nullptr;
        }
    else {
       // stream = new NullStream(); 
    }
    fft = new FastFourier();
    mutex = new Mutex();
    system = new System();
    ATdata.fft = fft;
    ATdata.stream = stream;
    ATdata.mutex = mutex;

    if (stream)
        audio = new Audio(&ATdata);
#if defined(_WIN32)
    font = new Font("./vera.ttf");
#else
    font = new Font(PSYMP3_DATADIR "/vera.ttf");
#endif // _WIN32
    std::cout << "font->isValid(): " << font->isValid() << std::endl;
    graph = new Surface(640, 350);

    Rect dstrect;
    SDL_TimerID timer;
    if (stream) {
        info["artist"] = font->Render("Artist: " + stream->getArtist());
        info["title"] = font->Render("Title: " + stream->getTitle());
        info["album"] = font->Render("Album: " + stream->getAlbum());
        info["playlist"] = font->Render("Playlist: " +
                                        convertInt(playlist->getPosition() + 1) + "/" +
                                        convertInt(playlist->entries()));
        info["scale"] = font->Render("log scale = " + std::to_string(scalefactor));
        info["decay"] = font->Render("decay = " + std::to_string(decayfactor));
        // program main loop
        audio->play(true);
        state = PLAYING;
    } else {
        state = STOPPED;
    }
    bool done = false;
    // if (system) system->progressState(TBPF_NORMAL);
    timer = SDL_AddTimer(33, AppLoopTimer, nullptr);
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
                    stream->seekTo(0);
                default:
                    break;
                }
                break;
            }
            case SDL_KEYUP:
            {
                switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    seek = 0;
                default:
                    break;
                }
            }
            case SDL_USEREVENT:
                switch(event.user.code) {
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
                    case RUN_GUI_ITERATION:
                    {
                        Player::guiRunning = true;
                        screen->FillRect(screen->MapRGB(0, 0, 0));
                        // draw bitmap
                        //screen->Blit(bmp, dstrect);
                        // draw tag strings
                        Rect f(1, 354);
                        screen->Blit(info["artist"], f);
                        f.width(270);
                        screen->Blit(info["playlist"], f);
                        f.width(1);
                        f.height(369);
                        screen->Blit(info["title"], f);
                        f.height(384);
                        screen->Blit(info["album"], f);
                        // position indicator
                        mutex->lock();
                        //system->updateProgress(stream->getPosition(), stream->getLength());
                        if(stream)
                            info["position"] = font->Render("Position: " + convertInt(stream->getPosition() / 60000)
                                                    + ":" + convertInt2((stream->getPosition() / 1000) % 60)
                                                    + "." + convertInt2((stream->getPosition() / 10) % 100)
                                                    + "/" + convertInt(stream->getLength() / 60000)
                                                    + ":" + convertInt2((stream->getLength() / 1000) % 60)
                                                    + "." + convertInt2((stream->getLength() / 10) % 100));
                        else
                            info["position"] = font->Render("Position: -:--.-- / -:--.--");
                        f.height(353);
                        f.width(400);
                        screen->Blit(info["position"], f);
                        if(stream)
                            screen->SetCaption("PsyMP3 " PSYMP3_VERSION +
                                           (std::string) " -:[ " + stream->getArtist().to8Bit(true) + " ]:- -- -:[ " +
                                           stream->getTitle().to8Bit(true) + " ]:- ["
                                           + convertInt(stream->getPosition() / 60000)
                                           + ":" + convertInt2((stream->getPosition() / 1000) % 60)
                                           + "." + convertInt2((stream->getPosition() / 10) % 100)
                                           + "/" + convertInt(stream->getLength() / 60000)
                                           + ":" + convertInt2((stream->getLength() / 1000) % 60)
                                           + "]", "PsyMP3 " PSYMP3_VERSION);
                        else
                            screen->SetCaption((std::string) "PsyMP3 " PSYMP3_VERSION + " -:[ not playing ]:-", "PsyMP3 " PSYMP3_VERSION);
                        // draw progress bar
                        screen->vline(399, 370, 385, 0xFFFFFFFF);
                        screen->vline(621, 370, 385, 0xFFFFFFFF);
                        screen->hline(399, 402, 370, 0xFFFFFFFF);
                        screen->hline(399, 402, 385, 0xFFFFFFFF);
                        screen->hline(618, 621, 370, 0xFFFFFFFF);
                        screen->hline(618, 621, 385, 0xFFFFFFFF);
                        if (seek == 1) {
                            if (stream)
                                stream->seekTo((long long) stream->getPosition() > 1500? (long long) stream->getPosition() - 1500 : 0);
                        } else if (seek == 2) {
                            if (stream)
                                stream->seekTo((long long) stream->getPosition() + 1500);
                        }
                        double t;
                        if(stream)
                            t = ((double) stream->getPosition() / (double) stream->getLength()) * 220;
                        else   
                            t = 0.0f;
                        for(double x = 0; x < t; x++) {
                            if (x > 146) {
                                screen->vline(x + 400, 373, 382, (uint8_t) ((x - 146) * 3.5), 0, 255, 255);
                            } else if (x < 73) {
                                screen->vline(x + 400, 373, 382, 128, 255, (uint8_t) (x * 3.5), 255);
                            } else {
                                screen->vline(x + 400, 373, 382, (uint8_t) (128-((x-73)*1.75)), (uint8_t) (255-((x-73)*3.5)), 255, 255);
                            }
                        };
                        this->renderSpectrum(graph);
                        mutex->unlock();
                        f.height(0);
                        f.width(0);
                        screen->Blit(*graph, f);
                        f.width(550);
                        screen->Blit(info["scale"], f);
                        f.height(15);
                        screen->Blit(info["decay"], f);
                        // DRAWING ENDS HERE


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

    // all is well ;)
    printf("Exited cleanly\n");
    return;
}
