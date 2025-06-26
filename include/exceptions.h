/*
 * exceptions.h - Various exception classes.
 * This also wraps SDL_gfx for primitives.
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

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

// When no format can open the file.
class InvalidMediaException : public std::exception
{
    public:
        InvalidMediaException(TagLib::String why);
        ~InvalidMediaException() noexcept override = default;
        const char *what() const noexcept override;
    protected:
    private:
        TagLib::String m_why;
};

// Correct format, but incorrect data for whatever reason (file corrupt, etc.)
class BadFormatException : public std::exception
{
    public:
        BadFormatException(TagLib::String why);
        ~BadFormatException() noexcept override = default;
        const char *what() const noexcept override;
    protected:
    private:
        TagLib::String m_why;
};

// Not correct data format for this class. Try next or throw InvalidMediaException
class WrongFormatException : public std::exception
{
    public:
        WrongFormatException(TagLib::String why);
        ~WrongFormatException() noexcept override = default;
        const char *what() const noexcept override;
    protected:
    private:
        TagLib::String m_why;
};

#endif // EXCEPTIONS_H
