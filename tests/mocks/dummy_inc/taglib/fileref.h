#ifndef DUMMY_FILEREF_H
#define DUMMY_FILEREF_H
#include "tag.h"
namespace TagLib {
    class FileRef {
    public:
        bool isNull() const { return true; }
    };
}
#endif
