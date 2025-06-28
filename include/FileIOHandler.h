/*
 * FileIOHandler.h - Concrete IOHandler for local file access.
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef FILEIOHANDLER_H
#define FILEIOHANDLER_H

#include "IOHandler.h"
#include <cstdio> // For FILE

class FileIOHandler : public IOHandler {
public:
    explicit FileIOHandler(const TagLib::String& path);
    ~FileIOHandler() override;

    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(long offset, int whence) override;
    long tell() override;
    int close() override;
    bool eof() override;

private:
    FILE* m_file_handle;
};

#endif // FILEIOHANDLER_H