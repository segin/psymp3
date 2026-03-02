#ifndef URI_H
#define URI_H

#include "../taglib/tstring.h"

namespace PsyMP3 {
namespace IO {

class URI {
public:
    explicit URI(const TagLib::String& uri_string) : m_uri(uri_string) {}
    TagLib::String scheme() const { return "file"; }
    TagLib::String path() const { return m_uri; }
private:
    TagLib::String m_uri;
};

} // namespace IO
} // namespace PsyMP3

using PsyMP3::IO::URI;

#endif
