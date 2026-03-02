#ifndef IOHANDLER_H
#define IOHANDLER_H

#include <sys/types.h>
#include <string>
#include <cstddef>

namespace PsyMP3 {
namespace IO {

class IOHandler {
public:
    virtual ~IOHandler() {}
    virtual size_t read(void* buffer, size_t size, size_t count) = 0;
    virtual int seek(off_t offset, int whence) = 0;
    virtual off_t tell() = 0;
    virtual int close() = 0;
};

} // namespace IO
} // namespace PsyMP3

using PsyMP3::IO::IOHandler;

#endif
