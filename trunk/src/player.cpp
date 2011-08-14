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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "psymp3.h"

std::string convertInt(long number)
{
   std::stringstream ss;//create a stringstream
   ss << number;//add number to the stream
   return ss.str();//return a string with the contents of the stream
}

std::string convertInt2(long number)
{
    std::stringstream ss;
    ss << number;
    if(ss.str().size() == 1)
        return "0" + ss.str();
    else
        return ss.str();
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
}

Uint32 Player::AppLoopTimer(Uint32 interval, void* param)
{
    // Create a user event to call the game loop.
    SDL_Event event;

    event.type = SDL_USEREVENT;
    event.user.code = RUN_GUI_ITERATION;
    event.user.data1 = 0;
    event.user.data2 = 0;

    SDL_PushEvent(&event);

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
    if ( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);

    TrueType::Init();
    Libmpg123::init();
    FastFourier::init();

    screen = new Display();
    playlist = new Playlist();
    stream = new Libmpg123(args[1]);
    fft = new FastFourier();
    ATdata.fft = fft;
    ATdata.stream = stream;
#ifdef DEBUG
    std::cout << "stream = " << std::hex << stream << ", fft = " << std::hex << fft << std::endl;
#endif
    if (stream)
        audio = new Audio(&ATdata);
    font = new Font("res/vera.ttf");
    std::cout << "font->isValid(): " << font->isValid() << std::endl;
    //Surface bmp = Surface::FromBMP("cb.bmp");
    graph = new Surface(640, 350);

    // centre the bitmap on screen
    Rect dstrect;
//    dstrect.width((screen->width() - bmp.width()) / 2);
//    dstrect.height((screen->height() - bmp.height()) / 2);

    SDL_TimerID timer;


    Surface s_artist = font->Render("Artist: " + stream->getArtist());
    Surface s_title = font->Render("Title: " + stream->getTitle());
    Surface s_album = font->Render("Album: " + stream->getAlbum());

    // program main loop
    bool done = false;
    audio->play(true);
    timer = SDL_AddTimer(33, AppLoopTimer, NULL);
    while (!done)
    {
        bool sdone = false;
        // message processing loop
        SDL_Event event;
        while (SDL_WaitEvent(&event))
        {
            // check for messages
 #ifdef DEBUG
            std::cout << "event.type: " << std::dec << (int) event.type << std::endl;
 #endif
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
                    case SDLK_LEFT:
#ifdef DEBUG
                        std::cout << "seeking back 1500msec" << std::endl;
#endif
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
                    screen->FillRect(screen->MapRGB(0, 0, 0));

                    // draw bitmap
                    //screen->Blit(bmp, dstrect);
                    // draw tag strings
                    Rect f(1, 354);
                    screen->Blit(s_artist, f);
                    f.height(369);
                    screen->Blit(s_title, f);
                    f.height(384);
                    screen->Blit(s_album, f);
                    // position indicator
                    Surface s_pos;
                    if(stream)
                    s_pos = font->Render("Position: " + convertInt(stream->getPosition() / 60000)
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
                                       (std::string) " -:[ " + stream->getArtist() + " ]:- -- -:[ " +
                                       stream->getTitle() + " ]:- ["
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
                    if(seek == 1) {
                        if (stream)
                            stream->seekTo((long long) stream->getPosition() > 1500? (long long) stream->getPosition() - 1500 : 0);
                    } else if (seek == 2) {
                        if (stream)
                            stream->seekTo((long long) stream->getPosition() + 1500);
                    }
                    double t = ((double) stream->getPosition() / (double) stream->getLength()) * 220;
                    for(double x = 0; x < t; x++) {
                        if(x > 146) {
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
                    for(int16_t x=0; x < 320; x++) {
                        graph->rectangle(x * 2, (int16_t) 350 - (spectrum[x] * 350.0f * 4) , (x * 2) + 1 , 350, 0xFFFFFFFF);
                    }
                    f.height(0);
                    f.width(0);
                    screen->Blit(*graph, f);
                    // DRAWING ENDS HERE


                    // finally, update the screen :)
                    screen->Flip();
                    // and if end of stream...
                    sdone = stream->eof();
                }
                break;
            } // end switch
            if (sdone || done) break;
        } // end of message processing

        // DRAWING STARTS HERE

        // clear screen

    } // end main loop

    // all is well ;)
    printf("Exited cleanly\n");
    return;
}
