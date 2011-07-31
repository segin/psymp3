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
    screen = NULL;
    playlist = NULL;
}

Player::~Player()
{
    if (screen)
        delete screen;
    if (playlist)
        delete playlist;
    if (font)
        delete font;
    if (audio)
        delete audio;
    if (stream)
        delete stream;
}

void Player::Run(std::vector<std::string> args)
{
    if((args.size() > 1) && args[1] == "--version") {
        about_console();
        return;
    }
    unsigned char a = 0;
    // initialize SDL video
    if ( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);

    TrueType::Init();
    Libmpg123::init();

    screen = new Display();
    playlist = new Playlist();
    stream = new Libmpg123(args[1]);
    if (stream)
        audio = new Audio(stream);
    font = new Font("res/vera.ttf");
    std::cout << "font->isValid(): " << font->isValid() << std::endl;
    Surface bmp = Surface::FromBMP("cb.bmp");

    // centre the bitmap on screen
    Rect dstrect;
    dstrect.width((screen->width() - bmp.width()) / 2);
    dstrect.height((screen->height() - bmp.height()) / 2);


    Surface s_artist = font->Render("Artist: " + stream->getArtist());
    Surface s_title = font->Render("Title: " + stream->getTitle());
    Surface s_album = font->Render("Album: " + stream->getAlbum());

    // program main loop
    bool done = false;
    audio->play(true);
    while (!done)
    {

        // message processing loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
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
                    if ((event.key.keysym.sym == SDLK_ESCAPE) || (event.key.keysym.sym == SDLK_q))
                        done = true;
                    break;
                }
            } // end switch
        } // end of message processing

        // DRAWING STARTS HERE

        // clear screen
        screen->FillRect(screen->MapRGB(0, 0, 0));

        // draw bitmap
        screen->Blit(bmp, dstrect);

        Rect f;
        f.width(1);
        f.height(354);

        screen->Blit(s_artist, f);
        f.height(369);
        screen->Blit(s_title, f);
        f.height(384);
        screen->Blit(s_album, f);

        Surface s_pos = font->Render("Position: " + convertInt(stream->getPosition() / 3600000)
                                    + ":" + convertInt2((stream->getPosition() / 60000) % 60)
                                    + ":" + convertInt2((stream->getPosition() / 1000) % 60)
                                    + "." + convertInt2((stream->getPosition() / 10) % 100));
        f.width(200);
        screen->Blit(s_pos, f);

        // DRAWING ENDS HERE
        screen->hline(0, 400, a++, 0xFFFFFF00 + (a & 255));

        // finally, update the screen :)
        screen->Flip();

        usleep(33000);
        if(stream->getPosition() >= stream->getLength()) break;
    } // end main loop

    // all is well ;)
    printf("Exited cleanly\n");
    return;
}
