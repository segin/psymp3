/*
 * flac.cpp - Extends the Stream base class to decode FLACs.
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

Flac::Flac(TagLib::String name) : Stream(name)
{

}

Flac::~Flac()
{

}

void Flac::open(TagLib::String name)
{

}

unsigned int Flac::getChannels()
{

}

unsigned int Flac::getRate()
{

}

unsigned int Flac::getEncoding()
{

}

unsigned int Flac::getLength()
{

}

unsigned long long Flac::getSLength()
{

}

unsigned int Flac::getPosition()
{

}

unsigned long long Flac::getSPosition()
{

}

size_t Flac::getData(size_t len, void *buf)
{

}

void Flac::seekTo(unsigned long pos)
{

}

bool Flac::eof()
{

}

void Flac::init()
{

}

void Flac::fini()
{

}
