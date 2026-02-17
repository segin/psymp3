#pragma once
#include <sys/types.h>
#include "tstring.h"

namespace TagLib {
    // ByteVector is defined in tstring.h

    class IOStream {
    public:
        enum Position { Beginning, Current, End };

        virtual ~IOStream() {}
        virtual FileName name() const = 0;
        virtual ByteVector readBlock(size_t length) = 0;
        virtual void writeBlock(const ByteVector &data) = 0;
        virtual void insert(const ByteVector &data, unsigned long start = 0, unsigned long replace = 0) = 0;
        virtual void removeBlock(unsigned long start = 0, unsigned long length = 0) = 0;
        virtual bool readOnly() const = 0;
        virtual bool isOpen() const = 0;
        virtual void seek(long offset, Position p = Beginning) = 0;
        virtual void clear() {}
        virtual long tell() const = 0;
        virtual long length() = 0;
        virtual void truncate(long length) = 0;
    };
}
