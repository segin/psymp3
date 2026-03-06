#ifndef DUMMY_TAG_H
#define DUMMY_TAG_H
#include <string>
namespace TagLib {
    typedef const char* FileName;
    class String {
    public:
        String() {}
        String(const char*) {}
        String(const std::string&) {}
        const char* toCString() const { return ""; }
    };
}
#endif
