/*
 * exceptions.h - Various exception classes.
 * This also wraps SDL_gfx for primitives.
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

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

// When no format can open the file.
class InvalidMediaException : public std::exception
{
    public:
        InvalidMediaException(TagLib::String why);
        virtual const char *what();
        ~InvalidMediaException() throw ();
    protected:
    private:
    TagLib::String m_why;
};

// Correct format, but incorrect data for whatever reason (file corrupt, etc.)
class BadFormatException : public std::exception
{
    public:
        BadFormatException(TagLib::String why);
        ~BadFormatException() throw ();
        virtual const char *what();
    protected:
    private:
    TagLib::String m_why;
};

// Not correct data format for this class. Try next or throw InvalidMediaException
class WrongFormatException : public std::exception
{
    public:
        WrongFormatException(TagLib::String why);
        ~WrongFormatException() throw ();
        virtual const char *what();
    protected:
    private:
    TagLib::String m_why;
};

#endif // EXCEPTIONS_H
