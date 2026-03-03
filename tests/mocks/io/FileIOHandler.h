#ifndef FILEIOHANDLER_H
#define FILEIOHANDLER_H

#include "IOHandler.h"
#include "../taglib/tstring.h"

namespace PsyMP3 {
namespace IO {
namespace File {

class FileIOHandler : public IOHandler {
public:
    FileIOHandler(const TagLib::String& path) {}
    size_t read(void* buffer, size_t size, size_t count) override { return 0; }
    int seek(off_t offset, int whence) override { return -1; }
    off_t tell() override { return -1; }
    int close() override { return 0; }
};

} // namespace File
} // namespace IO
} // namespace PsyMP3

using PsyMP3::IO::File::FileIOHandler;

#endif
