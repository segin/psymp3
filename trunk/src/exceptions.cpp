/*
 * exceptions.cpp - Exception classes code
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

InvalidMediaException::InvalidMediaException(TagLib::String why) : m_why(why)
{
    //ctor
}

InvalidMediaException::~InvalidMediaException() throw ()
{

}

const char *InvalidMediaException::what()
{
    return m_why.toCString(true);
}

BadFormatException::BadFormatException(TagLib::String why) : m_why(why)
{
    //ctor
}

BadFormatException::~BadFormatException() throw ()
{

}

const char *BadFormatException::what()
{
    return m_why.toCString(true);
}

WrongFormatException::WrongFormatException(TagLib::String why) : m_why(why)
{
    //ctor
}

WrongFormatException::~WrongFormatException() throw ()
{

}

const char *WrongFormatException::what()
{
    return m_why.toCString(true);
}



