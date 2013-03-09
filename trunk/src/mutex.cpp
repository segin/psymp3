/*
 * mutex.cpp - SDL_Mutex wrapper class.
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

Mutex::Mutex() : m_handle(SDL_CreateMutex())
{
    // Shouldn't really have anything to do here...
}

Mutex::~Mutex()
{
    SDL_DestroyMutex(m_handle);
}

void Mutex::lock()
{
    SDL_mutexP(m_handle);
    m_count++;
}

void Mutex::unlock()
{
    m_count--;
    SDL_mutexV(m_handle);
}
