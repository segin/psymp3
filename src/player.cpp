/*
 * player.cpp - class implementation for player class
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psymp3.h"

std::string convertInt(long number)
{
   std::stringstream ss;
   ss << number;
   return ss.str();
}

std::string convertInt2(long number)
{
    char s[8];
    snprintf(s, 8, "%02ld", number);
    return s;
}

Player::Player()
{
    //ctor - you'd think we'd initialize our object pointers here,
    // but no, some depend on SDL, which is initialized in Run()
    // -- but we will delete them in ~Player()
    // So, instead, print a startup banner to the console.
    std::cout << "PsyMP3 version " << PSYMP3_VERSION << "." << std::endl;
    screen = (Display *) NULL;
    playlist = (Playlist *) NULL;
    graph = (Surface *) NULL;
    font = (Font *) NULL;
    stream = (Stream *) NULL;
    audio = (Audio *) NULL;
    fft = (FastFourier *) NULL;
    mutex = (Mutex *) NULL;
    system = (System *) NULL;
}

Player::~Player()
{
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

Uint32 Player::AppLoopTimer(Uint32 interval, void* param)
{
    // Create a user event to call the game loop.
    SDL_Event event;

    event.type = SDL_USEREVENT;
    event.user.code = RUN_GUI_ITERATION;
    event.user.data1 = 0;
    event.user.data2 = 0;

    if (!gui_iteration_running)
        SDL_PushEvent(&event);
    else
        std::cout << "timer: skipped" << std::endl;

    return interval;
}

void Player::Run(std::vector<std::string> args)
{
    struct atdata ATdata;
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
    FastFourier::init();

    screen = new Display();
    playlist = new Playlist(args);
    stream = MediaFile::open(playlist->getTrack(0));
    fft = new FastFourier();
    mutex = new Mutex();
    system = new System();
    ATdata.fft = fft;
    ATdata.stream = stream;
    ATdata.mutex = mutex;

    if (stream)
        audio = new Audio(&ATdata);
    font = new Font(PSYMP3_DATADIR "/vera.ttf");
    std::cout << "font->isValid(): " << font->isValid() << std::endl;
    graph = new Surface(640, 350);

    Rect dstrect;
    SDL_TimerID timer;

    Surface s_artist = font->Render("Artiest: " + stream->getArtist());
    Surface s_title = font->Render("Titel: " + stream->getTitle());
    Surface s_album = font->Render("Album: " + stream->getAlbum());
    Surface s_playlist = font->Render("Afspeellijst: " + 
                                      convertInt(playlist->getPosition() + 1) + "/" +
                                      convertInt(playlist->entries()));

    // program main loop
    bool done = false;
    audio->play(true);
    // if (system) system->progressState(TBPF_NORMAL);
    timer = SDL_AddTimer(33, AppLoopTimer, NULL);
    while (!done)
    {
        bool sdone = false;
        // message processing loop
        SDL_Event event;
        while (SDL_WaitEvent(&event))
        {
            // check for messages
            switch (event.type)
            {
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
                        TagLib::String nextfile = playlist->next();
			if (nextfile == "") {
                            done = true;
                        } else {
                            mutex->lock();
                            delete stream;
                            stream = MediaFile::open(nextfile);
                            ATdata.stream = stream;
                            mutex->unlock();
                            s_artist = font->Render("Artiest: " + stream->getArtist());
                            s_title = font->Render("Titel: " + stream->getTitle());
                            s_album = font->Render("Album: " + stream->getAlbum());
                            s_playlist = font->Render("Afspeellijst: " + 
                                      convertInt(playlist->getPosition() + 1) + "/" +
                                      convertInt(playlist->entries()));

                        }
                        break;
                    }
                    case SDLK_p:
                        mutex->lock();
			delete stream;
                        stream = MediaFile::open(playlist->prev());
                        ATdata.stream = stream;
                        mutex->unlock();
                        s_artist = font->Render("Artiest: " + stream->getArtist());
                        s_title = font->Render("Titel: " + stream->getTitle());
                        s_album = font->Render("Album: " + stream->getAlbum());
                        s_playlist = font->Render("Afspeellijst: " + 
                                      convertInt(playlist->getPosition() + 1) + "/" +
                                      convertInt(playlist->entries()));

                        break;
                    case SDLK_LEFT:
                        seek = 1;
                        break;
                    case SDLK_RIGHT:
                        seek = 2;
                        break;
                    case SDLK_SPACE:
                        audio->play(!audio->isPlaying());
                        break;
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
                if (event.user.code == RUN_GUI_ITERATION) {
                    gui_iteration_running = true;
                    screen->FillRect(screen->MapRGB(0, 0, 0));
                    // draw bitmap
                    //screen->Blit(bmp, dstrect);
                    // draw tag strings
                    Rect f(1, 354);
                    screen->Blit(s_artist, f);
                    f.width(270);
                    screen->Blit(s_playlist, f);
                    f.width(1);
                    f.height(369);
                    screen->Blit(s_title, f);
                    f.height(384);
                    screen->Blit(s_album, f);
                    // position indicator
                    Surface s_pos;
                    mutex->lock();
                    //system->updateProgress(stream->getPosition(), stream->getLength());
                    if(stream)
                    s_pos = font->Render("Positie: " + convertInt(stream->getPosition() / 60000)
                                                + ":" + convertInt2((stream->getPosition() / 1000) % 60)
                                                + "." + convertInt2((stream->getPosition() / 10) % 100)
                                                + "/" + convertInt(stream->getLength() / 60000)
                                                + ":" + convertInt2((stream->getLength() / 1000) % 60)
                                                + "." + convertInt2((stream->getLength() / 10) % 100));
                    else
                        s_pos = font->Render("Position: -:--.-- / -:--.--");
                    f.height(353);
                    f.width(400);
                    screen->Blit(s_pos, f);
                    screen->SetCaption("PsyMP3 " PSYMP3_VERSION +
                                       (std::string) " -:[ " + stream->getArtist().to8Bit(true) + " ]:- -- -:[ " +
                                       stream->getTitle().to8Bit(true) + " ]:- ["
                                       + convertInt(stream->getPosition() / 60000)
                                       + ":" + convertInt2((stream->getPosition() / 1000) % 60)
                                       + "." + convertInt2((stream->getPosition() / 10) % 100)
                                       + "/" + convertInt(stream->getLength() / 60000)
                                       + ":" + convertInt2((stream->getLength() / 1000) % 60)
                                       + "]", "PsyMP3 " PSYMP3_VERSION);
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
                    double t = ((double) stream->getPosition() / (double) stream->getLength()) * 220;
                    for(double x = 0; x < t; x++) {
                        if (x > 146) {
                            screen->vline(x + 400, 373, 382, (uint8_t) ((x - 146) * 3.5), 0, 255, 255);
                        } else if (x < 73) {
                            screen->vline(x + 400, 373, 382, 128, 255, (uint8_t) (x * 3.5), 255);
                        } else {
                            screen->vline(x + 400, 373, 382, (uint8_t) (128-((x-73)*1.75)), (uint8_t) (255-((x-73)*3.5)), 255, 255);
                        }
                    };
                    float *spectrum = fft->getFFT();
                    for (int x = 0; x < 350; x++) {
                        graph->hline(0, 639, x, 64);
                    }
                    for(uint16_t x=0; x < 320; x++) {
                        // graph->rectangle(x * 2, (int16_t) 350 - (spectrum[x] * 350.0f * 4) , (x * 2) + 1 , 350, 0xFFFFFFFF);
                        if (x > 213) {
                            graph->rectangle(x * 2, (int16_t) 350 - (spectrum[x] * 350.0f * 4) , (x * 2) + 1, 350, (uint8_t) ((x - 214) * 2.4), 0, 255, 255);
                        } else if (x < 106) {
                            graph->rectangle(x * 2, (int16_t) 350 - (spectrum[x] * 350.0f * 4) , (x * 2) + 1, 350, 128, 255, (uint8_t) (x * 2.4), 255);
                        } else {
                            graph->rectangle(x * 2, (int16_t) 350 - (spectrum[x] * 350.0f * 4) , (x * 2) + 1, 350, (uint8_t) (128 - ((x - 106) * 1.2)), (uint8_t) (255 - ((x - 106) * 2.4)), 255, 255);
                        }
                    };

                    mutex->unlock();
                    f.height(0);
                    f.width(0);
                    screen->Blit(*graph, f);
                    // DRAWING ENDS HERE


                    // finally, update the screen :)
                    screen->Flip();
                    // and if end of stream...
                    sdone = stream->eof();
                    gui_iteration_running = false;
                }
                break;
            } // end switch
            if (done) break;
            if (sdone) { 
                // synthesize "n" key event
                SDL_Event event;
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_n;
                SDL_PushEvent(&event);
            }
        } // end of message processing

        // DRAWING STARTS HERE

        // clear screen

    } // end main loop

    // all is well ;)
    printf("Exited cleanly\n");
    return;
}
